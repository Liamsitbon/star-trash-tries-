using System;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

namespace CinemaQuest.Editor
{
    public static class BuildCinemaAssets
    {
        private const string ShaderPath =
            "Assets/Cinema/Shaders/CinemaVideoScreen.shader";

        [MenuItem("Cinema Quest/Build Android shader bundle")]
        public static void BuildAndroid()
        {
            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.Android, false);
            PlayerSettings.SetGraphicsAPIs(
                BuildTarget.Android,
                new[] { GraphicsDeviceType.Vulkan, GraphicsDeviceType.OpenGLES3 });

            var shader = AssetDatabase.LoadAssetAtPath<Shader>(ShaderPath);
            if (shader == null)
                throw new InvalidOperationException(
                    $"Cinema Quest shader is unavailable: {ShaderPath}");

            var sourceText = File.ReadAllText(
                Path.Combine(Application.dataPath,
                    "Cinema/Shaders/CinemaVideoScreen.shader"));
            foreach (var required in new[]
                     {
                         "STEREO_MULTIVIEW_ON", "STEREO_INSTANCING_ON",
                         "ZTest LEqual", "_MainTex", "_Gamma"
                     })
            {
                if (!sourceText.Contains(required))
                    throw new InvalidOperationException(
                        $"Cinema Quest shader contract is missing: {required}");
            }

            var output = Path.Combine(Path.GetTempPath(),
                "CinemaQuestAssetBundleAndroid");
            if (Directory.Exists(output)) Directory.Delete(output, true);
            Directory.CreateDirectory(output);

            var builds = new[]
            {
                new AssetBundleBuild
                {
                    assetBundleName = "cinemaassets.android",
                    assetNames = new[] { ShaderPath }
                }
            };
            var manifest = BuildPipeline.BuildAssetBundles(
                output, builds,
                BuildAssetBundleOptions.ChunkBasedCompression |
                BuildAssetBundleOptions.DeterministicAssetBundle,
                BuildTarget.Android);
            if (manifest == null)
                throw new InvalidOperationException(
                    "Unity did not build the Cinema Quest Android bundle.");

            var source = Path.Combine(output, "cinemaassets.android");
            var destination = Path.GetFullPath(Path.Combine(
                Application.dataPath, "../../assets/cinemaassets.android"));
            Directory.CreateDirectory(Path.GetDirectoryName(destination));
            File.Copy(source, destination, true);
            var bytes = new FileInfo(destination).Length;
            if (bytes <= 0)
                throw new InvalidOperationException("Cinema Quest bundle is empty.");
            Debug.Log($"CINEMA_QUEST_ASSET_BUNDLE_OK path={destination} bytes={bytes} assets=1");
        }
    }
}
