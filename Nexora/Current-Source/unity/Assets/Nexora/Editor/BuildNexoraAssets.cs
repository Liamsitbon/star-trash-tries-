using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

namespace Nexora.Editor
{
    public static class BuildNexoraAssets
    {
        private const string DomeShaderPath = "Assets/Nexora/Shaders/NexoraDome.shader";
        private const string DomeMaterialPath = "Assets/Nexora/Materials/NexoraDome.mat";

        [Serializable]
        private sealed class BundleProvenance
        {
            public int schemaVersion = 1;
            public string unityVersion;
            public string buildTarget;
            public string graphicsApis;
            public string shaderSha256;
            public string shaderMetaSha256;
            public string materialSha256;
            public string materialMetaSha256;
            public string builderSha256;
            public string bundleSha256;
        }

        private static string Sha256File(string path)
        {
            using (var algorithm = SHA256.Create())
            using (var stream = File.OpenRead(path))
            {
                var digest = algorithm.ComputeHash(stream);
                var text = new StringBuilder(digest.Length * 2);
                foreach (var value in digest) text.Append(value.ToString("x2"));
                return text.ToString();
            }
        }

        private static void ThrowOnShaderErrors(Shader shader)
        {
            foreach (var message in ShaderUtil.GetShaderMessages(shader))
            {
                if (string.Equals(message.severity.ToString(), "Error",
                                  StringComparison.OrdinalIgnoreCase))
                {
                    throw new InvalidOperationException(
                        $"Nexora Android shader compilation failed: {message.message}");
                }
            }
        }

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
                         "UNITY_VERTEX_INPUT_INSTANCE_ID", "UNITY_VERTEX_OUTPUT_STEREO",
                         "UNITY_SETUP_INSTANCE_ID", "UNITY_INITIALIZE_OUTPUT",
                         "UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO",
                         "UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX",
                         "PackVideoUV", "_Opacity * _Tint.a",
                         "_FlipY", "_SwapEyes", "_CameraAmount", "_VideoReady",
                         "ZTest LEqual"
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
            ThrowOnShaderErrors(shader);

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
            ThrowOnShaderErrors(shader);

            var source = Path.Combine(output, "nexoraassets.android");
            var destination = Path.GetFullPath(
                Path.Combine(Application.dataPath, "../../assets/nexoraassets.android"));
            Directory.CreateDirectory(Path.GetDirectoryName(destination));
            File.Copy(source, destination, true);

            var bytes = new FileInfo(destination).Length;
            if (bytes <= 0) throw new InvalidOperationException("Nexora Android bundle is empty.");

            var shaderAbsolute = Path.Combine(
                Application.dataPath, "Nexora/Shaders/NexoraDome.shader");
            var materialAbsolute = Path.Combine(
                Application.dataPath, "Nexora/Materials/NexoraDome.mat");
            var builderAbsolute = Path.Combine(
                Application.dataPath, "Nexora/Editor/BuildNexoraAssets.cs");
            var provenance = new BundleProvenance
            {
                unityVersion = Application.unityVersion,
                buildTarget = BuildTarget.Android.ToString(),
                graphicsApis = "Vulkan,OpenGLES3",
                shaderSha256 = Sha256File(shaderAbsolute),
                shaderMetaSha256 = Sha256File(shaderAbsolute + ".meta"),
                materialSha256 = Sha256File(materialAbsolute),
                materialMetaSha256 = Sha256File(materialAbsolute + ".meta"),
                builderSha256 = Sha256File(builderAbsolute),
                bundleSha256 = Sha256File(destination)
            };
            var provenancePath = destination + ".provenance.json";
            File.WriteAllText(
                provenancePath, JsonUtility.ToJson(provenance, true) + "\n",
                new UTF8Encoding(false));
            Debug.Log(
                $"NEXORA_ASSET_BUNDLE_OK path={destination} bytes={bytes} assets=2 " +
                $"sha256={provenance.bundleSha256} provenance={provenancePath}");
        }
    }
}
