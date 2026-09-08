using System;
using System.Collections.Generic;
using System.Globalization;

namespace VivifyTimelinePreview
{
public static class BeatSaberMapCompat
{
    public static string GetVersion(Dictionary<string, object> root)
    {
        if (root == null) return "unknown";
        object v;
        if (root.TryGetValue("version", out v) && v != null) return Convert.ToString(v);
        if (root.TryGetValue("_version", out v) && v != null) return Convert.ToString(v);
        return "unknown";
    }

    public static bool IsV2(Dictionary<string, object> root)
    {
        string v = GetVersion(root);
        return v.StartsWith("2", StringComparison.Ordinal) || (root != null && root.ContainsKey("_notes"));
    }

    public static bool IsV4(Dictionary<string, object> root)
    {
        string v = GetVersion(root);
        return v.StartsWith("4", StringComparison.Ordinal) || (root != null && root.ContainsKey("colorNotesData"));
    }

    public static Dictionary<string, object> GetCustomData(Dictionary<string, object> root)
    {
        if (root == null) return null;
        object o;
        if (root.TryGetValue("customData", out o)) return o as Dictionary<string, object>;
        if (root.TryGetValue("_customData", out o)) return o as Dictionary<string, object>;
        return null;
    }

    public static List<object> GetCustomEvents(Dictionary<string, object> root)
    {
        var cd = GetCustomData(root);
        if (cd == null) return null;
        object o;
        if (cd.TryGetValue("customEvents", out o)) return NormalizeCustomEvents(o as List<object>);
        if (cd.TryGetValue("_customEvents", out o)) return NormalizeCustomEvents(o as List<object>);
        return null;
    }

    public static List<object> GetColorNotes(Dictionary<string, object> root)
    {
        var result = new List<object>();
        if (root == null) return result;
        if (IsV2(root))
        {
            var notes = GetList(root, "_notes");
            if (notes == null) return result;
            for (int i = 0; i < notes.Count; i++)
            {
                var n = notes[i] as Dictionary<string, object>;
                if (n == null) continue;
                int type = (int)GetNumber(n, "_type", -1d);
                if (type != 0 && type != 1) continue;
                var d = new Dictionary<string, object>();
                d["b"] = GetNumber(n, "_time", 0d);
                d["x"] = GetNumber(n, "_lineIndex", 0d);
                d["y"] = GetNumber(n, "_lineLayer", 0d);
                d["c"] = type;
                d["d"] = GetNumber(n, "_cutDirection", 8d);
                d["a"] = 0d;
                var cd = GetDict(n, "_customData");
                if (cd != null) d["customData"] = NormalizeCustomData(cd);
                result.Add(d);
            }
            return result;
        }
        if (IsV4(root))
        {
            var refs = GetList(root, "colorNotes");
            var data = GetList(root, "colorNotesData");
            if (refs == null || data == null) return result;
            for (int i = 0; i < refs.Count; i++)
            {
                var r = refs[i] as Dictionary<string, object>; if (r == null) continue;
                int idx = (int)GetNumber(r, "i", -1d); if (idx < 0 || idx >= data.Count) continue;
                var md = data[idx] as Dictionary<string, object>; if (md == null) continue;
                var d = Copy(md);
                d["b"] = GetNumber(r, "b", 0d);
                if (r.ContainsKey("r")) d["r"] = GetNumber(r, "r", 0d);
                result.Add(d);
            }
            return result;
        }
        var direct = GetList(root, "colorNotes");
        return direct != null ? direct : result;
    }

    public static List<object> GetBombNotes(Dictionary<string, object> root)
    {
        var result = new List<object>();
        if (root == null) return result;
        if (IsV2(root))
        {
            var notes = GetList(root, "_notes");
            if (notes == null) return result;
            for (int i = 0; i < notes.Count; i++)
            {
                var n = notes[i] as Dictionary<string, object>; if (n == null) continue;
                if ((int)GetNumber(n, "_type", -1d) != 3) continue;
                var d = new Dictionary<string, object>();
                d["b"] = GetNumber(n, "_time", 0d); d["x"] = GetNumber(n, "_lineIndex", 0d); d["y"] = GetNumber(n, "_lineLayer", 0d);
                var cd = GetDict(n, "_customData"); if (cd != null) d["customData"] = NormalizeCustomData(cd);
                result.Add(d);
            }
            return result;
        }
        if (IsV4(root))
        {
            var refs = GetList(root, "bombNotes"); var data = GetList(root, "bombNotesData");
            if (refs == null || data == null) return result;
            for (int i = 0; i < refs.Count; i++)
            {
                var r = refs[i] as Dictionary<string, object>; if (r == null) continue;
                int idx = (int)GetNumber(r, "i", -1d); if (idx < 0 || idx >= data.Count) continue;
                var md = data[idx] as Dictionary<string, object>; if (md == null) continue;
                var d = Copy(md); d["b"] = GetNumber(r, "b", 0d); if (r.ContainsKey("r")) d["r"] = GetNumber(r, "r", 0d); result.Add(d);
            }
            return result;
        }
        var direct = GetList(root, "bombNotes"); return direct != null ? direct : result;
    }

    public static List<object> GetObstacles(Dictionary<string, object> root)
    {
        var result = new List<object>();
        if (root == null) return result;
        if (IsV2(root))
        {
            var src = GetList(root, "_obstacles"); if (src == null) return result;
            for (int i = 0; i < src.Count; i++)
            {
                var o = src[i] as Dictionary<string, object>; if (o == null) continue;
                int type = (int)GetNumber(o, "_type", 0d);
                var d = new Dictionary<string, object>();
                d["b"] = GetNumber(o, "_time", 0d); d["d"] = GetNumber(o, "_duration", 0d); d["x"] = GetNumber(o, "_lineIndex", 0d); d["w"] = GetNumber(o, "_width", 1d);
                d["y"] = type == 1 ? 2d : 0d; d["h"] = type == 1 ? 3d : 5d;
                var cd = GetDict(o, "_customData"); if (cd != null) d["customData"] = NormalizeCustomData(cd);
                result.Add(d);
            }
            return result;
        }
        if (IsV4(root))
        {
            var refs = GetList(root, "obstacles"); var data = GetList(root, "obstaclesData"); if (refs == null || data == null) return result;
            for (int i = 0; i < refs.Count; i++)
            {
                var r = refs[i] as Dictionary<string, object>; if (r == null) continue;
                int idx = (int)GetNumber(r, "i", -1d); if (idx < 0 || idx >= data.Count) continue;
                var md = data[idx] as Dictionary<string, object>; if (md == null) continue;
                var d = Copy(md); d["b"] = GetNumber(r, "b", 0d); if (r.ContainsKey("r")) d["r"] = GetNumber(r, "r", 0d); result.Add(d);
            }
            return result;
        }
        var direct = GetList(root, "obstacles"); return direct != null ? direct : result;
    }

    public static List<object> GetBurstSliders(Dictionary<string, object> root)
    {
        var result = new List<object>();
        if (root == null) return result;
        if (!IsV4(root))
        {
            var direct = GetList(root, "burstSliders"); return direct != null ? direct : result;
        }
        var refs = GetList(root, "chains"); var data = GetList(root, "chainsData"); var noteData = GetList(root, "colorNotesData");
        if (refs == null || data == null || noteData == null) return result;
        for (int i = 0; i < refs.Count; i++)
        {
            var r = refs[i] as Dictionary<string, object>; if (r == null) continue;
            int hi = (int)GetNumber(r, "i", -1d); int ci = (int)GetNumber(r, "ci", -1d);
            if (hi < 0 || hi >= noteData.Count || ci < 0 || ci >= data.Count) continue;
            var head = noteData[hi] as Dictionary<string, object>; var chain = data[ci] as Dictionary<string, object>; if (head == null || chain == null) continue;
            var d = new Dictionary<string, object>();
            d["b"] = GetNumber(r, "hb", 0d); d["tb"] = GetNumber(r, "tb", GetNumber(r, "hb", 0d));
            d["x"] = GetNumber(head, "x", 0d); d["y"] = GetNumber(head, "y", 0d); d["c"] = GetNumber(head, "c", 0d); d["d"] = GetNumber(head, "d", 8d);
            d["tx"] = GetNumber(chain, "tx", 0d); d["ty"] = GetNumber(chain, "ty", 0d); d["sc"] = GetNumber(chain, "c", 1d); d["s"] = GetNumber(chain, "s", 1d);
            result.Add(d);
        }
        return result;
    }

    public static List<object> GetSliders(Dictionary<string, object> root)
    {
        var result = new List<object>();
        if (root == null) return result;
        if (!IsV4(root))
        {
            var direct = GetList(root, "sliders");
            if (direct != null) return direct;

            // v2.6 arcs/sliders use the old underscored field names. Normalize them to the
            // compact v3-style keys used by the preview builder so old maps get real arcs too.
            var old = GetList(root, "_sliders");
            if (old == null) return result;
            for (int i = 0; i < old.Count; i++)
            {
                var a = old[i] as Dictionary<string, object>; if (a == null) continue;
                var d = new Dictionary<string, object>();
                d["b"] = GetNumber(a, "_headTime", 0d);
                d["tb"] = GetNumber(a, "_tailTime", GetNumber(a, "_headTime", 0d));
                d["x"] = GetNumber(a, "_headLineIndex", 0d);
                d["y"] = GetNumber(a, "_headLineLayer", 0d);
                d["c"] = GetNumber(a, "_colorType", 0d);
                d["d"] = GetNumber(a, "_headCutDirection", 8d);
                d["tx"] = GetNumber(a, "_tailLineIndex", 0d);
                d["ty"] = GetNumber(a, "_tailLineLayer", 0d);
                d["tc"] = GetNumber(a, "_tailCutDirection", 8d);
                d["mu"] = GetNumber(a, "_headControlPointLengthMultiplier", 1d);
                d["tmu"] = GetNumber(a, "_tailControlPointLengthMultiplier", 1d);
                d["m"] = GetNumber(a, "_sliderMidAnchorMode", 0d);
                var cd = GetDict(a, "_customData"); if (cd != null) d["customData"] = NormalizeCustomData(cd);
                result.Add(d);
            }
            return result;
        }
        var refs = GetList(root, "arcs"); var data = GetList(root, "arcsData"); var noteData = GetList(root, "colorNotesData");
        if (refs == null || data == null || noteData == null) return result;
        for (int i = 0; i < refs.Count; i++)
        {
            var r = refs[i] as Dictionary<string, object>; if (r == null) continue;
            int hi = (int)GetNumber(r, "hi", -1d); int ti = (int)GetNumber(r, "ti", -1d); int ai = (int)GetNumber(r, "ai", -1d);
            if (hi < 0 || ti < 0 || hi >= noteData.Count || ti >= noteData.Count) continue;
            var head = noteData[hi] as Dictionary<string, object>; var tail = noteData[ti] as Dictionary<string, object>; if (head == null || tail == null) continue;
            var arc = ai >= 0 && ai < data.Count ? data[ai] as Dictionary<string, object> : null;
            var d = new Dictionary<string, object>();
            d["b"] = GetNumber(r, "hb", 0d); d["tb"] = GetNumber(r, "tb", 0d); d["x"] = GetNumber(head, "x", 0d); d["y"] = GetNumber(head, "y", 0d); d["c"] = GetNumber(head, "c", 0d); d["d"] = GetNumber(head, "d", 8d);
            d["tx"] = GetNumber(tail, "x", 0d); d["ty"] = GetNumber(tail, "y", 0d); d["tc"] = GetNumber(tail, "d", 8d); d["mu"] = arc == null ? 1d : GetNumber(arc, "m", 1d); d["tmu"] = arc == null ? 1d : GetNumber(arc, "tm", 1d);
            result.Add(d);
        }
        return result;
    }

    public static List<object> GetBasicEvents(Dictionary<string, object> root, Dictionary<string, object> lightshow)
    {
        var result = new List<object>();
        if (root == null) root = new Dictionary<string, object>();
        if (IsV2(root))
        {
            var src = GetList(root, "_events"); if (src == null) return result;
            for (int i = 0; i < src.Count; i++)
            {
                var e = src[i] as Dictionary<string, object>; if (e == null) continue;
                var d = new Dictionary<string, object>(); d["b"] = GetNumber(e, "_time", 0d); d["et"] = GetNumber(e, "_type", 0d); d["i"] = GetNumber(e, "_value", 0d); d["f"] = GetNumber(e, "_floatValue", 1d);
                var cd = GetDict(e, "_customData"); if (cd != null) d["customData"] = NormalizeCustomData(cd); result.Add(d);
            }
            return result;
        }
        if (IsV4(root))
        {
            var ls = lightshow != null ? lightshow : root;
            var refs = GetList(ls, "basicEvents"); var data = GetList(ls, "basicEventsData"); if (refs == null || data == null) return result;
            for (int i = 0; i < refs.Count; i++)
            {
                var r = refs[i] as Dictionary<string, object>; if (r == null) continue; int idx = (int)GetNumber(r, "i", -1d); if (idx < 0 || idx >= data.Count) continue;
                var md = data[idx] as Dictionary<string, object>; if (md == null) continue; var d = new Dictionary<string, object>();
                d["b"] = GetNumber(r, "b", 0d); d["et"] = GetNumber(md, "t", 0d); d["i"] = GetNumber(md, "i", 0d); d["f"] = GetNumber(md, "f", 1d); result.Add(d);
            }
            return result;
        }
        var direct = GetList(root, "basicBeatmapEvents"); return direct != null ? direct : result;
    }

    // Approximate modern light-event-box data as legacy-like basic events for the procedural
    // fallback stage. This does not attempt to reproduce a specific official environment's light IDs;
    // it preserves the timing/color/brightness feel so v3/v4 maps are not visually static.
    public static List<object> GetFallbackLightEvents(Dictionary<string, object> root, Dictionary<string, object> lightshow)
    {
        var result = new List<object>();
        var basic = GetBasicEvents(root, lightshow);
        if (basic != null) for (int i = 0; i < basic.Count; i++) result.Add(basic[i]);
        if (root == null) root = new Dictionary<string, object>();

        if (!IsV4(root))
        {
            var groups = GetList(root, "lightColorEventBoxGroups");
            if (groups != null)
            {
                for (int gi = 0; gi < groups.Count; gi++)
                {
                    var g = groups[gi] as Dictionary<string, object>; if (g == null) continue;
                    double groupBeat = GetNumber(g, "b", 0d); int groupId = (int)GetNumber(g, "g", gi);
                    var boxes = GetList(g, "e"); if (boxes == null) continue;
                    for (int bi = 0; bi < boxes.Count; bi++)
                    {
                        var box = boxes[bi] as Dictionary<string, object>; if (box == null) continue;
                        var evs = GetList(box, "e"); if (evs == null) continue;
                        for (int ei = 0; ei < evs.Count; ei++)
                        {
                            var ev = evs[ei] as Dictionary<string, object>; if (ev == null) continue;
                            double b = groupBeat + GetNumber(ev, "b", 0d);
                            int color = (int)GetNumber(ev, "c", 1d);
                            double brightness = Math.Max(0.05d, GetNumber(ev, "s", 1d));
                            result.Add(MakeFallbackLight(b, groupId, color, brightness));
                        }
                    }
                }
            }
        }
        else
        {
            var ls = lightshow != null ? lightshow : root;
            var groups = GetList(ls, "eventBoxGroups");
            var boxesData = GetList(ls, "lightColorEventBoxes");
            var eventsData = GetList(ls, "lightColorEvents");
            if (groups != null && boxesData != null && eventsData != null)
            {
                for (int gi = 0; gi < groups.Count; gi++)
                {
                    var g = groups[gi] as Dictionary<string, object>; if (g == null) continue;
                    if ((int)GetNumber(g, "t", 0d) != 1) continue; // color box group
                    double groupBeat = GetNumber(g, "b", 0d); int groupId = (int)GetNumber(g, "g", gi);
                    var refs = GetList(g, "e"); if (refs == null) continue;
                    for (int ri = 0; ri < refs.Count; ri++)
                    {
                        var br = refs[ri] as Dictionary<string, object>; if (br == null) continue;
                        int boxIndex = (int)GetNumber(br, "e", -1d); if (boxIndex < 0 || boxIndex >= boxesData.Count) continue;
                        var box = boxesData[boxIndex] as Dictionary<string, object>; if (box == null) continue;
                        var links = GetList(box, "l"); if (links == null) continue;
                        for (int li = 0; li < links.Count; li++)
                        {
                            var link = links[li] as Dictionary<string, object>; if (link == null) continue;
                            int eventIndex = (int)GetNumber(link, "i", -1d); if (eventIndex < 0 || eventIndex >= eventsData.Count) continue;
                            var ev = eventsData[eventIndex] as Dictionary<string, object>; if (ev == null) continue;
                            double b = groupBeat + GetNumber(link, "b", 0d);
                            int color = (int)GetNumber(ev, "c", 1d);
                            double brightness = Math.Max(0.05d, GetNumber(ev, "b", 1d));
                            result.Add(MakeFallbackLight(b, groupId, color, brightness));
                        }
                    }
                }
            }
        }
        result.Sort(delegate(object a, object b)
        {
            var da = a as Dictionary<string, object>; var db = b as Dictionary<string, object>;
            return GetNumber(da, "b", 0d).CompareTo(GetNumber(db, "b", 0d));
        });
        return result;
    }

    private static Dictionary<string, object> MakeFallbackLight(double beat, int groupId, int color, double brightness)
    {
        var d = new Dictionary<string, object>();
        d["b"] = beat;
        d["et"] = Math.Abs(groupId) % 5;
        // Legacy value ranges encode blue-ish colors as 1..4 and red-ish colors as 5..8.
        // White/other colors alternate by group so the generic stage still has spatial contrast.
        bool left = color == 0 || (color > 1 && (groupId & 1) == 0);
        d["i"] = left ? 5d : 1d;
        d["f"] = brightness;
        return d;
    }

    public static List<object> GetColorBoostEvents(Dictionary<string, object> root, Dictionary<string, object> lightshow)
    {
        var result = new List<object>();
        if (root == null) root = new Dictionary<string, object>();
        if (IsV2(root)) return result;
        if (IsV4(root))
        {
            var ls = lightshow != null ? lightshow : root; var refs = GetList(ls, "colorBoostEvents"); var data = GetList(ls, "colorBoostEventsData"); if (refs == null || data == null) return result;
            for (int i = 0; i < refs.Count; i++)
            {
                var r = refs[i] as Dictionary<string, object>; if (r == null) continue; int idx = (int)GetNumber(r, "i", -1d); if (idx < 0 || idx >= data.Count) continue; var md = data[idx] as Dictionary<string, object>; if (md == null) continue;
                var d = new Dictionary<string, object>(); d["b"] = GetNumber(r, "b", 0d); d["o"] = GetNumber(md, "b", 0d) != 0d; result.Add(d);
            }
            return result;
        }
        var direct = GetList(root, "colorBoostBeatmapEvents"); return direct != null ? direct : result;
    }

    public static double FindMaxBeat(Dictionary<string, object> root, Dictionary<string, object> lightshow)
    {
        double max = 0d;
        List<object>[] groups = { GetColorNotes(root), GetBombNotes(root), GetObstacles(root), GetSliders(root), GetBurstSliders(root), GetBasicEvents(root, lightshow), GetColorBoostEvents(root, lightshow), GetCustomEvents(root) };
        for (int g = 0; g < groups.Length; g++)
        {
            var list = groups[g]; if (list == null) continue;
            for (int i = 0; i < list.Count; i++)
            {
                var d = list[i] as Dictionary<string, object>; if (d == null) continue;
                double b = GetNumber(d, "b", GetNumber(d, "_time", 0d)); double tb = GetNumber(d, "tb", b);
                var evd = GetDict(d, "d"); if (evd != null) { double dur = GetNumber(evd, "duration", 0d); if (dur > 0 && dur < 10000) b += dur; }
                max = Math.Max(max, Math.Max(b, tb));
            }
        }
        return max;
    }

    private static List<object> NormalizeCustomEvents(List<object> src)
    {
        if (src == null) return null;
        var result = new List<object>();
        for (int i = 0; i < src.Count; i++)
        {
            var e = src[i] as Dictionary<string, object>; if (e == null) continue;
            if (e.ContainsKey("b") && e.ContainsKey("t")) { result.Add(e); continue; }
            var n = new Dictionary<string, object>(); n["b"] = GetNumber(e, "_time", 0d); n["t"] = GetString(e, "_type", "CustomEvent");
            var data = GetDict(e, "_data"); n["d"] = data == null ? new Dictionary<string, object>() : NormalizeDictionary(data);
            result.Add(n);
        }
        return result;
    }

    private static Dictionary<string, object> NormalizeCustomData(Dictionary<string, object> src)
    {
        if (src == null) return null;
        return NormalizeDictionary(src);
    }

    private static Dictionary<string, object> NormalizeDictionary(Dictionary<string, object> src)
    {
        var d = new Dictionary<string, object>();
        foreach (var kv in src)
        {
            string key = kv.Key != null && kv.Key.StartsWith("_", StringComparison.Ordinal) ? kv.Key.Substring(1) : kv.Key;
            object v = kv.Value;
            var dict = v as Dictionary<string, object>;
            var list = v as List<object>;
            if (dict != null) v = NormalizeDictionary(dict);
            else if (list != null)
            {
                var nl = new List<object>();
                for (int i = 0; i < list.Count; i++)
                {
                    var ld = list[i] as Dictionary<string, object>; nl.Add(ld != null ? (object)NormalizeDictionary(ld) : list[i]);
                }
                v = nl;
            }
            if (key == "time") key = "b";
            else if (key == "type") key = "t";
            else if (key == "data") key = "d";
            d[key] = v;
        }
        return d;
    }

    private static Dictionary<string, object> Copy(Dictionary<string, object> src)
    {
        var d = new Dictionary<string, object>(); if (src != null) foreach (var kv in src) d[kv.Key] = kv.Value; return d;
    }
    private static object GetValue(Dictionary<string, object> d, string k) { object v; return d != null && d.TryGetValue(k, out v) ? v : null; }
    private static Dictionary<string, object> GetDict(Dictionary<string, object> d, string k) { return GetValue(d, k) as Dictionary<string, object>; }
    private static List<object> GetList(Dictionary<string, object> d, string k) { return GetValue(d, k) as List<object>; }
    private static string GetString(Dictionary<string, object> d, string k, string f) { object v = GetValue(d, k); return v == null ? f : Convert.ToString(v); }
    private static double GetNumber(Dictionary<string, object> d, string k, double f) { object v = GetValue(d, k); try { return v == null ? f : Convert.ToDouble(v, CultureInfo.InvariantCulture); } catch { return f; } }
}
}
