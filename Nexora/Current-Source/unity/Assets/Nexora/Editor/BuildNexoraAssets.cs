using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using UnityEditor;
using UnityEditor.Build;
using UnityEditor.Rendering;
using UnityEditor.XR.Management;
using UnityEditor.XR.Management.Metadata;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.XR.Management;
using Unity.XR.Oculus;

namespace Nexora.Editor
{
    internal sealed class NexoraShaderBuildLog : IPreprocessShaders
    {
        public int callbackOrder => int.MaxValue;

        public void OnProcessShader(Shader shader, ShaderSnippetData snippet,
                                    IList<ShaderCompilerData> variants)
        {
            if (shader.name != "Nexora/VideoDome") return;
            var multiview = new ShaderKeyword("STEREO_MULTIVIEW_ON");
            var instancing = new ShaderKeyword("STEREO_INSTANCING_ON");
            var rgbd = new ShaderKeyword(shader, "NEXORA_RGBD_ON");
            foreach (var platform in variants.GroupBy(value => value.shaderCompilerPlatform))
                Debug.Log($"NEXORA_SHADER_COMPILE platform={platform.Key} stage={snippet.shaderType} " +
                    $"variants={platform.Count()} multiview={platform.Count(value => value.shaderKeywordSet.IsEnabled(multiview))} " +
                    $"stereoInstanced={platform.Count(value => value.shaderKeywordSet.IsEnabled(instancing))} " +
                    $"rgbd={platform.Count(value => value.shaderKeywordSet.IsEnabled(rgbd))}");
        }
    }

    public static class BuildNexoraAssets
    {
        private const string DomeShaderPath = "Assets/Nexora/Shaders/NexoraDome.shader";
        private const string DomeMaterialPath = "Assets/Nexora/Materials/NexoraDome.mat";
        private const string WarmupPath = "Assets/Nexora/Materials/NexoraWarmup.shadervariants";

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

        private static void ConfigureQuestXR()
        {
            // XR must be configured in the bundle project too. Merely having
            // stereo keywords in ShaderLab does not keep their compiled variants.
            const string folder = "Assets/Nexora/XR";
            if (!AssetDatabase.IsValidFolder(folder))
                AssetDatabase.CreateFolder("Assets/Nexora", "XR");
            string path = folder + "/XRGeneralSettings.asset";
            var settings = AssetDatabase.LoadAssetAtPath<XRGeneralSettingsPerBuildTarget>(path);
            if (settings == null)
            {
                settings = ScriptableObject.CreateInstance<XRGeneralSettingsPerBuildTarget>();
                AssetDatabase.CreateAsset(settings, path);
            }
            EditorBuildSettings.AddConfigObject(XRGeneralSettings.k_SettingsKey, settings, true);
            if (!settings.HasSettingsForBuildTarget(BuildTargetGroup.Android))
                settings.CreateDefaultSettingsForBuildTarget(BuildTargetGroup.Android);
            if (!settings.HasManagerSettingsForBuildTarget(BuildTargetGroup.Android))
                settings.CreateDefaultManagerSettingsForBuildTarget(BuildTargetGroup.Android);
            var android = settings.SettingsForBuildTarget(BuildTargetGroup.Android);
            android.InitManagerOnStart = true;
            if (!XRPackageMetadataStore.AssignLoader(android.Manager,
                    "Unity.XR.Oculus.OculusLoader", BuildTargetGroup.Android))
                throw new InvalidOperationException("Cannot configure Nexora Android Oculus XR loader.");

            var oculus = AssetDatabase.LoadAssetAtPath<OculusSettings>(folder + "/OculusSettings.asset");
            if (oculus == null)
            {
                oculus = ScriptableObject.CreateInstance<OculusSettings>();
                AssetDatabase.CreateAsset(oculus, folder + "/OculusSettings.asset");
            }
            oculus.m_StereoRenderingModeAndroid = OculusSettings.StereoRenderingModeAndroid.Multiview;
            EditorBuildSettings.AddConfigObject("Unity.XR.Oculus.Settings", oculus, true);
            PlayerSettings.stereoRenderingPath = StereoRenderingPath.Instancing;
            EditorUtility.SetDirty(oculus);
            EditorUtility.SetDirty(settings);
            EditorUtility.SetDirty(android);
            EditorUtility.SetDirty(android.Manager);
            AssetDatabase.SaveAssets();
        }

        [MenuItem("Nexora/Build Android shader bundle")]
        public static void BuildAndroid()
        {
            ConfigureQuestXR();
            PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.Android, false);
            PlayerSettings.SetGraphicsAPIs(
                BuildTarget.Android,
                new[] { GraphicsDeviceType.Vulkan, GraphicsDeviceType.OpenGLES3 });

            AssetDatabase.ImportAsset(DomeShaderPath,
                ImportAssetOptions.ForceUpdate | ImportAssetOptions.ForceSynchronousImport);
            var shader = AssetDatabase.LoadAssetAtPath<Shader>(DomeShaderPath);
            if (shader == null)
                throw new InvalidOperationException($"Nexora dome shader is unavailable: {DomeShaderPath}");

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
            material.enableInstancing = true;
            EditorUtility.SetDirty(material);
            var warmup = AssetDatabase.LoadAssetAtPath<ShaderVariantCollection>(WarmupPath);
            if (warmup == null) {
                warmup = new ShaderVariantCollection();
                AssetDatabase.CreateAsset(warmup, WarmupPath);
            }
            warmup.Clear();
            foreach (var stereo in new[] { "", "STEREO_MULTIVIEW_ON", "STEREO_INSTANCING_ON" })
                foreach (var depth in new[] { false, true }) {
                    var keywords = new List<string>();
                    if (stereo.Length > 0) keywords.Add(stereo);
                    if (depth) keywords.Add("NEXORA_RGBD_ON");
                    warmup.Add(new ShaderVariantCollection.ShaderVariant(shader, PassType.Normal, keywords.ToArray()));
                }
            EditorUtility.SetDirty(warmup);

            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh(ImportAssetOptions.ForceSynchronousImport);
            ThrowOnShaderErrors(shader);

            var output = Path.Combine(Path.GetTempPath(), "NexoraAssetBundleAndroid-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(output);

            var builds = new[]
            {
                new AssetBundleBuild
                {
                    assetBundleName = "nexoraassets.android",
                    assetNames = new[] { DomeMaterialPath, DomeShaderPath, WarmupPath }
                }
            };
            var manifest = BuildPipeline.BuildAssetBundles(
                output,
                builds,
                BuildAssetBundleOptions.ChunkBasedCompression |
                BuildAssetBundleOptions.DeterministicAssetBundle |
                BuildAssetBundleOptions.StrictMode |
                BuildAssetBundleOptions.ForceRebuildAssetBundle,
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
                $"NEXORA_ASSET_BUNDLE_OK path={destination} bytes={bytes} assets=3 " +
                $"sha256={provenance.bundleSha256} provenance={provenancePath}");
        }
    }
}
