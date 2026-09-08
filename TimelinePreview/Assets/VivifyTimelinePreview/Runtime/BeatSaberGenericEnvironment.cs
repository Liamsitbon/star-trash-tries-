using System;
using System.Collections.Generic;
using UnityEngine;

namespace VivifyTimelinePreview
{
[ExecuteInEditMode]
public class BeatSaberGenericEnvironment : MonoBehaviour
{
    [Serializable]
    public class LightGroup
    {
        public string name;
        public Renderer[] renderers;
    }

    public LightGroup backLights = new LightGroup { name = "Back" };
    public LightGroup ringLights = new LightGroup { name = "Ring" };
    public LightGroup leftLights = new LightGroup { name = "Left" };
    public LightGroup rightLights = new LightGroup { name = "Right" };
    public LightGroup centerLights = new LightGroup { name = "Center" };
    public LightGroup extraLights = new LightGroup { name = "Extra" };
    public Renderer[] laneGlow;
    public Renderer[] staticGeometry;

    public Color leftColor = new Color(1f, 0.04f, 0.08f, 1f);
    public Color rightColor = new Color(0.05f, 0.35f, 1f, 1f);
    public Color boostLeftColor = new Color(1f, 0.18f, 0.55f, 1f);
    public Color boostRightColor = new Color(0.1f, 0.95f, 1f, 1f);
    public Color dimColor = new Color(0.015f, 0.02f, 0.035f, 1f);
    public float emissionMultiplier = 3.2f;

    private static readonly int ColorId = Shader.PropertyToID("_Color");
    private static readonly int EmissionColorId = Shader.PropertyToID("_EmissionColor");
    private MaterialPropertyBlock block;

    private void Awake()
    {
        EnsureBlock();
    }

    private void OnEnable()
    {
        EnsureBlock();
    }

    private void EnsureBlock()
    {
        if (block == null) block = new MaterialPropertyBlock();
    }

    public void Evaluate(double beat, List<object> basicEvents, List<object> colorBoostEvents)
    {
        bool boost = false;
        if (colorBoostEvents != null)
        {
            for (int i = 0; i < colorBoostEvents.Count; i++)
            {
                var e = colorBoostEvents[i] as Dictionary<string, object>; if (e == null) continue;
                double b = Num(e, "b", 0d); if (b > beat) break;
                boost = Bool(e, "o", false);
            }
        }

        Color l = boost ? boostLeftColor : leftColor;
        Color r = boost ? boostRightColor : rightColor;
        ApplyGroup(backLights, EvaluateGroup(beat, basicEvents, 0, l, r));
        ApplyGroup(ringLights, EvaluateGroup(beat, basicEvents, 1, l, r));
        ApplyGroup(leftLights, EvaluateGroup(beat, basicEvents, 2, l, r));
        ApplyGroup(rightLights, EvaluateGroup(beat, basicEvents, 3, l, r));
        ApplyGroup(centerLights, EvaluateGroup(beat, basicEvents, 4, l, r));
        ApplyGroup(extraLights, EvaluateAnyExtra(beat, basicEvents, l, r));

        Color lane = Color.Lerp(l, r, 0.5f) * 0.32f;
        SetRenderers(laneGlow, lane, 0.65f);
    }

    public void SetIdle()
    {
        ApplyGroup(backLights, dimColor);
        ApplyGroup(ringLights, dimColor);
        ApplyGroup(leftLights, leftColor * 0.16f);
        ApplyGroup(rightLights, rightColor * 0.16f);
        ApplyGroup(centerLights, dimColor);
        ApplyGroup(extraLights, dimColor);
        SetRenderers(laneGlow, Color.Lerp(leftColor, rightColor, 0.5f) * 0.12f, 0.4f);
    }

    private Color EvaluateGroup(double beat, List<object> events, int eventType, Color left, Color right)
    {
        Dictionary<string, object> last = null;
        if (events != null)
        {
            for (int i = 0; i < events.Count; i++)
            {
                var e = events[i] as Dictionary<string, object>; if (e == null) continue;
                double b = Num(e, "b", 0d); if (b > beat) break;
                if ((int)Num(e, "et", -999d) == eventType) last = e;
            }
        }
        if (last == null) return dimColor;
        int value = (int)Num(last, "i", 0d);
        float brightness = Mathf.Max(0f, (float)Num(last, "f", 1d));
        double eventBeat = Num(last, "b", beat);
        if (value == 0) return dimColor;
        bool redSide = value >= 5;
        Color baseColor = redSide ? left : right;
        int mode = redSide ? value - 5 : value - 1;
        if (mode == 1) // flash
        {
            float t = Mathf.Clamp01((float)((beat - eventBeat) / 0.18));
            brightness *= Mathf.Lerp(2.4f, 0.55f, t);
        }
        else if (mode == 2) // fade
        {
            float t = Mathf.Clamp01((float)((beat - eventBeat) / 1.0));
            brightness *= Mathf.Lerp(1.3f, 0.08f, t);
        }
        return baseColor * Mathf.Clamp(brightness, 0.04f, 3f);
    }

    private Color EvaluateAnyExtra(double beat, List<object> events, Color left, Color right)
    {
        if (events == null) return dimColor;
        Dictionary<string, object> last = null;
        for (int i = 0; i < events.Count; i++)
        {
            var e = events[i] as Dictionary<string, object>; if (e == null) continue;
            double b = Num(e, "b", 0d); if (b > beat) break;
            int et = (int)Num(e, "et", -1d); if (et >= 5 && et <= 11) last = e;
        }
        if (last == null) return dimColor;
        int value = (int)Num(last, "i", 0d); if (value == 0) return dimColor;
        return (value >= 5 ? left : right) * Mathf.Max(0.08f, (float)Num(last, "f", 1d));
    }

    private void ApplyGroup(LightGroup group, Color c)
    {
        if (group == null) return;
        SetRenderers(group.renderers, c, emissionMultiplier);
    }

    private void SetRenderers(Renderer[] rr, Color c, float emission)
    {
        if (rr == null) return;
        EnsureBlock();
        for (int i = 0; i < rr.Length; i++)
        {
            Renderer renderer = rr[i]; if (renderer == null) continue;
            renderer.GetPropertyBlock(block);
            block.SetColor(ColorId, c);
            block.SetColor(EmissionColorId, c * emission);
            renderer.SetPropertyBlock(block);
            block.Clear();
        }
    }

    private static object Val(Dictionary<string, object> d, string key) { object v; return d != null && d.TryGetValue(key, out v) ? v : null; }
    private static double Num(Dictionary<string, object> d, string key, double fallback) { object v = Val(d, key); try { return v == null ? fallback : Convert.ToDouble(v, System.Globalization.CultureInfo.InvariantCulture); } catch { return fallback; } }
    private static bool Bool(Dictionary<string, object> d, string key, bool fallback) { object v = Val(d, key); if (v is bool) return (bool)v; if (v == null) return fallback; try { return Math.Abs(Convert.ToDouble(v)) > 0.0001; } catch { bool b; return bool.TryParse(Convert.ToString(v), out b) ? b : fallback; } }
}
}
