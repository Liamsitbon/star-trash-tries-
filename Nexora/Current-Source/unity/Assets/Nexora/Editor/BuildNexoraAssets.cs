using System;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

namespace Nexora.Editor
{
    public static class BuildNexoraAssets
    {
        private const string DomeShaderPath = "Assets/Nexora/Shaders/NexoraDome.shader";
        private const string DomeMaterialPath = "Assets/Nexora/Materials/NexoraDome.mat";

        [MenuItem("Nexora/Build Android shader bundle")]
        public static void BuildAndroid()
        {
            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.Android, false);
            PlayerSettings.SetGraphicsAPIs(
                BuildTarget.Android,
                new[] { GraphicsDeviceType.Vulkan, GraphicsDeviceType.OpenGLES3 });

            var shader = AssetDatabase.LoadAssetAtPath<Shader>(DomeShaderPath);
            if (shader == null)
                throw new InvalidOperationException($"Nexora dome shader is unavailable: {DomeShaderPath}");

            var shaderSource = File.ReadAllText(
                Path.Combine(Application.dataPath, "Nexora/Shaders/NexoraDome.shader"));
            foreach (var requiredToken in new[]
                     {
                         "STEREO_MULTIVIEW_ON", "STEREO_INSTANCING_ON",
                         "_FlipY", "_SwapEyes", "_CameraAmount", "_VideoReady",
                         "frameReady callback", "ZTest LEqual"
                     })
            {
                if (!shaderSource.Contains(requiredToken))
                    throw new InvalidOperationException(
                        $"Nexora Quest shader contract is missing token: {requiredToken}");
            }

            var material = AssetDatabase.LoadAssetAtPath<Material>(DomeMaterialPath);
            if (material == null)
            {
                material = new Material(shader) { name = "NexoraDome" };
                AssetDatabase.CreateAsset(material, DomeMaterialPath);
            }
            else
            {
                material.shader = shader;
                EditorUtility.SetDirty(material);
            }

            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh(ImportAssetOptions.ForceSynchronousImport);

            var output = Path.Combine(Path.GetTempPath(), "NexoraAssetBundleAndroid");
            if (Directory.Exists(output)) Directory.Delete(output, true);
            Directory.CreateDirectory(output);

            var builds = new[]
            {
                new AssetBundleBuild
                {
                    assetBundleName = "nexoraassets.android",
                    assetNames = new[] { DomeMaterialPath, DomeShaderPath }
                }
            };
            var manifest = BuildPipeline.BuildAssetBundles(
                output,
                builds,
                BuildAssetBundleOptions.ChunkBasedCompression |
                BuildAssetBundleOptions.DeterministicAssetBundle,
                BuildTarget.Android);
            if (manifest == null)
                throw new InvalidOperationException("Unity did not build the Nexora Android bundle.");

            var source = Path.Combine(output, "nexoraassets.android");
            var destination = Path.GetFullPath(
                Path.Combine(Application.dataPath, "../../assets/nexoraassets.android"));
            Directory.CreateDirectory(Path.GetDirectoryName(destination));
            File.Copy(source, destination, true);

            var bytes = new FileInfo(destination).Length;
            if (bytes <= 0) throw new InvalidOperationException("Nexora Android bundle is empty.");
            Debug.Log($"NEXORA_ASSET_BUNDLE_OK path={destination} bytes={bytes} assets=2");
        }
    }
}
