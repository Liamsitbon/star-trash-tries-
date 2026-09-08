using System.Collections.Generic;
using UnityEngine;

namespace VivifyTimelinePreview
{

[ExecuteInEditMode]
[RequireComponent(typeof(Camera))]
public class BeatSaberBlitPreview : MonoBehaviour
{
    [System.Serializable]
    public class ActiveBlit
    {
        public Material material;
        public int pass = -1;
    }

    [System.NonSerialized] public readonly List<ActiveBlit> activeBlits = new List<ActiveBlit>();

    private void OnRenderImage(RenderTexture source, RenderTexture destination)
    {
        if (activeBlits == null || activeBlits.Count == 0)
        {
            Graphics.Blit(source, destination);
            return;
        }

        RenderTexture current = source;
        RenderTexture temp = null;
        for (int i = 0; i < activeBlits.Count; i++)
        {
            var entry = activeBlits[i];
            bool last = i == activeBlits.Count - 1;
            RenderTexture target = last ? destination : RenderTexture.GetTemporary(source.descriptor);
            if (entry != null && IsBlitMaterialSafe(entry.material))
            {
                int passCount = entry.material.passCount;
                if (entry.pass >= 0 && entry.pass < passCount) Graphics.Blit(current, target, entry.material, entry.pass);
                else Graphics.Blit(current, target, entry.material);
            }
            else Graphics.Blit(current, target);

            if (temp != null) RenderTexture.ReleaseTemporary(temp);
            temp = last ? null : target;
            current = target;
        }
        if (temp != null) RenderTexture.ReleaseTemporary(temp);
    }

    private static bool IsBlitMaterialSafe(Material material)
    {
        if (material == null || material.shader == null || !material.shader.isSupported) return false;
        string shaderName = material.shader.name ?? string.Empty;
        if (shaderName == "Hidden/InternalErrorShader") return false;
        bool metalEditor = Application.platform == RuntimePlatform.OSXEditor &&
            SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Metal;
        // A raw Standard material imported from a custom Vivify material may contain stale
        // serialized texture bindings. Never feed one into Graphics.Blit on Metal.
        if (metalEditor && shaderName == "Standard") return false;
        return true;
    }
}
}
