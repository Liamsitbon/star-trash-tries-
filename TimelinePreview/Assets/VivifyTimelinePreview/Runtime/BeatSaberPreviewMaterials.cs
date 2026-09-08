using System;
using System.Collections.Generic;
using UnityEngine;

namespace VivifyTimelinePreview
{
    // Shared by rig authoring and playback: never put the original, incompatible
    // property/keyword set back onto a preview adapter on the next Timeline frame.
    public static class BeatSaberPreviewMaterials
    {
        public static Func<Shader, Shader> GetClockedShader;
        private static readonly Dictionary<Material, Material> sources = new Dictionary<Material, Material>();

        public static Material SourceOf(Material material)
        {
            Material source;
            return material != null && sources.TryGetValue(material, out source) && source != null ? source : material;
        }

        public static bool IsUsable(Shader shader)
        {
            if (shader == null || !shader.isSupported || shader.name == "Hidden/InternalErrorShader") return false;
#if UNITY_EDITOR
            if (UnityEditor.ShaderUtil.ShaderHasError(shader)) return false;
#endif
            return true;
        }

        private static string GameplayAdapter(Material source)
        {
            if (source == null) return null;
            string shader = source.shader != null ? source.shader.name : "";
            if (shader.StartsWith("VivifyTimelinePreview/", StringComparison.Ordinal)) return null;
            string key = (shader + " " + source.name).Replace(" ", "").ToLowerInvariant();
            if (key.Contains("saberblade")) return "YouSaberBladePreview";
            if (key.Contains("saberhilt")) return "YouSaberHiltPreview";
            if (key.Contains("sabertrail") || key.Contains("saberribbon")) return "YouSaberTrailPreview";
            if (key.Contains("saberparticle")) return "ParticlePreview";
            if (key.Contains("reflectivenote") || key.Contains("reflectivearrow")) return "YouReflectiveNotePreview";
            if (key.Contains("glassnote") || key.Contains("glassarrow")) return "YouGlassNotePreview";
            if (key.Contains("portalnote") || key.Contains("dropnote")) return "YouPortalNotePreview";
            return null;
        }

        public static bool NeedsAdapter(Material source)
        {
            if (source == null || !IsUsable(source.shader)) return true;
            // Prefer the map's real shader whenever this editor can compile it.
            // Replacing a procedural flare with an untextured transparent quad
            // creates the giant red/blue rectangles around the sabers.
            return Application.platform == RuntimePlatform.OSXEditor &&
                SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Metal && source.shader.name == "Standard";
        }

        public static Material Create(Material source, bool forceAdapter = false, Color? tint = null)
        {
            Material result;
            if (source != null && !forceAdapter && !NeedsAdapter(source))
            {
                result = new Material(source);
                if (GetClockedShader != null)
                {
                    Shader clocked = GetClockedShader(source.shader);
                    if (IsUsable(clocked)) result.shader = clocked;
                }
            }
            else
            {
                bool transparent = source != null && (source.renderQueue >= 3000 ||
                    source.GetTag("RenderType", false, "") == "Transparent");
                string adapter = GameplayAdapter(source) ?? (transparent ? "GenericTransparentPreview" : "GenericPreview");
                Shader safe = FindUsable("VivifyTimelinePreview/" + adapter,
                    transparent ? "VivifyTimelinePreview/GenericTransparentPreview" : "VivifyTimelinePreview/GenericPreview", "Unlit/Color");
                if (safe == null) return null;
                result = new Material(safe);
                // Keep the adapter's own compatible keyword set and render state.
                if (source != null && source.renderQueue >= 0) result.renderQueue = source.renderQueue;
                CopyTexture(source, result, "_MainTex", "_MainTex");
                CopyTexture(source, result, "_BaseMap", "_MainTex");
                CopyTexture(source, result, "_Mask", "_Mask");
                CopyTexture(source, result, "_EmissionMap", "_EmissionMap");
                string[] floats = { "_Cutout", "_ColorMix", "_RGBSplit", "_RefractiveIndex", "_DistortionAmount", "_FadeDistance", "_PlaneDistance", "_Void", "_Arrow", "_Debris", "_Opacity", "_FlareOpacity", "_FlareBrightness" };
                if (source != null) foreach (string property in floats)
                    if (source.HasProperty(property) && result.HasProperty(property)) result.SetFloat(property, source.GetFloat(property));
                if (source != null && source.HasProperty("_CutPlane") && result.HasProperty("_CutPlane"))
                    result.SetVector("_CutPlane", source.GetVector("_CutPlane"));
                Color color = source != null && source.HasProperty("_Color") ? source.GetColor("_Color") :
                    source != null && source.HasProperty("_BaseColor") ? source.GetColor("_BaseColor") : Color.white;
                if (result.HasProperty("_Color")) result.SetColor("_Color", color);
            }
            if (tint.HasValue && result.HasProperty("_Color")) result.SetColor("_Color", tint.Value);
            result.name = (source != null ? source.name : "Material") + " [Timeline Preview]";
            result.hideFlags = HideFlags.DontSave;
            if (source != null) sources[result] = SourceOf(source);
            return result;
        }

        public static Shader FindUsable(params string[] names)
        {
            foreach (string name in names)
            {
                Shader shader = Shader.Find(name);
                if (IsUsable(shader)) return shader;
            }
            return null;
        }

        private static void CopyTexture(Material source, Material target, string from, string to)
        {
            if (source == null || !source.HasProperty(from) || !target.HasProperty(to)) return;
            Texture texture = source.GetTexture(from);
            if (texture == null || texture.dimension != UnityEngine.Rendering.TextureDimension.Tex2D) return;
            target.SetTexture(to, texture);
            target.SetTextureOffset(to, source.GetTextureOffset(from));
            target.SetTextureScale(to, source.GetTextureScale(from));
        }

        public static void Forget(Material material) { if (material != null) sources.Remove(material); }
    }
}
