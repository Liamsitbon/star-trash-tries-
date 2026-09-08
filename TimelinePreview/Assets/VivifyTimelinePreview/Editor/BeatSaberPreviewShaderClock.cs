#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;
using UnityEditor;
using UnityEngine;

namespace VivifyTimelinePreview
{
    [InitializeOnLoad]
    public static class BeatSaberPreviewShaderClock
    {
        private const string Output = "Assets/VivifyTimelinePreview/GeneratedShaders";
        private static readonly Dictionary<Shader, Shader> cache = new Dictionary<Shader, Shader>();
        private const string ClockHeader = "\n#ifndef VIVIFY_PREVIEW_CLOCK\n#define VIVIFY_PREVIEW_CLOCK\n" +
            "#include \"UnityCG.cginc\"\nfloat4 _VivifyPreviewTime;\nfloat4 _VivifyPreviewSinTime;\n" +
            "float4 _VivifyPreviewCosTime;\nfloat4 _VivifyPreviewDeltaTime;\n" +
            "#define _Time _VivifyPreviewTime\n#define _SinTime _VivifyPreviewSinTime\n" +
            "#define _CosTime _VivifyPreviewCosTime\n#define unity_DeltaTime _VivifyPreviewDeltaTime\n#endif\n";

        static BeatSaberPreviewShaderClock() { BeatSaberPreviewMaterials.GetClockedShader = Get; }

        public static Shader Get(Shader source)
        {
            if (source == null || source.name.StartsWith("VivifyTimelinePreview/", StringComparison.Ordinal)) return source;
            Shader cached;
            if (cache.TryGetValue(source, out cached)) return cached;
            string path = AssetDatabase.GetAssetPath(source);
            // Only clone text-based Built-in shaders. Never rewrite the map's source,
            // built-in resources, Shader Graph, or a different render pipeline.
            if (!path.StartsWith("Assets/", StringComparison.Ordinal) || !path.EndsWith(".shader", StringComparison.OrdinalIgnoreCase))
                return source;
            string text = File.ReadAllText(path);
            if (!text.Contains("CGPROGRAM") || text.Contains("HLSLPROGRAM")) return source;
            string guid = AssetDatabase.AssetPathToGUID(path);
            string outputPath = Output + "/" + guid + ".shader";
            text = new Regex("Shader\\s+\"[^\"]+\"").Replace(text, "Shader \"VivifyTimelinePreview/Clocked/" + guid + "\"", 1);
            // Relative custom includes must still resolve from the original directory.
            string directory = Path.GetDirectoryName(path).Replace('\\', '/');
            text = Regex.Replace(text, "(#include\\s+\")([^\"]+)(\")", match =>
            {
                string include = match.Groups[2].Value;
                string relative = directory + "/" + include;
                return !include.StartsWith("Assets/", StringComparison.Ordinal) && File.Exists(relative)
                    ? match.Groups[1].Value + relative + match.Groups[3].Value : match.Value;
            });
            // Define time aliases after Unity's guarded declarations so shared includes
            // use the same song clock without redefining Unity's built-in uniforms.
            text = Regex.Replace(text, @"\b(CGPROGRAM|CGINCLUDE)\b", "$1" + ClockHeader);
            Directory.CreateDirectory(Output);
            if (!File.Exists(outputPath) || File.ReadAllText(outputPath) != text)
            {
                File.WriteAllText(outputPath, text);
                AssetDatabase.ImportAsset(outputPath, ImportAssetOptions.ForceSynchronousImport);
            }
            Shader result = AssetDatabase.LoadAssetAtPath<Shader>(outputPath);
            cache[source] = result != null ? result : source;
            return cache[source];
        }

        // Imports only preview-owned shader copies. Does not run a map or save a scene.
        public static void PrepareShaderCopies()
        {
            cache.Clear();
            foreach (string guid in AssetDatabase.FindAssets("t:Shader", new[] { "Assets/Shaders" }))
                Get(AssetDatabase.LoadAssetAtPath<Shader>(AssetDatabase.GUIDToAssetPath(guid)));
            Debug.Log("[Vivify Preview V6] Imported song-clock shader copies. Original map shaders unchanged.");
        }
    }
}
#endif
