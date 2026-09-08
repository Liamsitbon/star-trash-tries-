#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.Timeline;

namespace VivifyTimelinePreview
{
public class BeatSaberVivifyTimelineBuilder : EditorWindow
{
    private string datPath = "";
    private string infoPath = "";
    private string lightshowPath = "";
    private string detectedEnvironment = "";
    private string detectedSongFilename = "";
    private float bpm = 120f;
    private float defaultNjs = 10f;
    private float defaultJumpOffset = 0f;
    private AudioClip song;
    private bool includeCustomEvents = true;
    private bool includeNotes = true;
    private bool includeGameplay = true;
    private bool groupByEventType = true;
    private bool addPreviewDriver = true;
    private bool addAudioTrack = false;
    private bool useInfoSettings = true;
    private TimelineAsset lastTimeline;

    [MenuItem("Tools/Beat Saber/Beat Saber Timeline Preview V6.0")]
    public static void Open() { GetWindow<BeatSaberVivifyTimelineBuilder>("Beat Saber Timeline V6.0"); }

    // Keep the old menu path so existing muscle memory still works.
    [MenuItem("Tools/Beat Saber/Vivify Timeline Preview")]
    public static void OpenLegacy() { Open(); }

    private void OnGUI()
    {
        EditorGUILayout.LabelField("Beat Saber DAT → Unity Timeline Preview V6.0", EditorStyles.boldLabel);
        EditorGUILayout.HelpBox("V6.0 is map-general: vanilla v2/v3, v4 beatmap objects, optional v4 Lightshow DAT, and Vivify/Noodle-style custom events. Vanilla maps get exact 0.5-scale red/blue notes with opposite-color arrows, bombs, faint walls, arcs/chains, two fallback preview sabers when a map has no custom saber, and a Beat Saber-style fallback environment. Vivify maps use real renderable source prefabs/materials/effects when those assets exist and fall back safely when a dependency is incomplete.", MessageType.Info);

        using (new EditorGUILayout.HorizontalScope())
        {
            datPath = EditorGUILayout.TextField("Beatmap DAT", datPath);
            if (GUILayout.Button("Browse…", GUILayout.Width(80)))
            {
                string chosen = EditorUtility.OpenFilePanel("Choose Beat Saber beatmap DAT", "", "dat");
                if (!string.IsNullOrEmpty(chosen))
                {
                    datPath = chosen;
                    string beside = Path.Combine(Path.GetDirectoryName(datPath) ?? "", "Info.dat");
                    if (File.Exists(beside)) infoPath = beside;
                }
            }
        }
        using (new EditorGUILayout.HorizontalScope())
        {
            infoPath = EditorGUILayout.TextField("Info.dat (optional)", infoPath);
            if (GUILayout.Button("Browse…", GUILayout.Width(80))) { string chosen = EditorUtility.OpenFilePanel("Choose Beat Saber Info.dat", "", "dat"); if (!string.IsNullOrEmpty(chosen)) infoPath = chosen; }
            GUI.enabled = File.Exists(infoPath) && File.Exists(datPath);
            if (GUILayout.Button("Auto", GUILayout.Width(55))) LoadSettingsFromInfo();
            GUI.enabled = true;
        }
        using (new EditorGUILayout.HorizontalScope())
        {
            lightshowPath = EditorGUILayout.TextField("V4 Lightshow DAT (optional)", lightshowPath);
            if (GUILayout.Button("Browse…", GUILayout.Width(80))) { string chosen = EditorUtility.OpenFilePanel("Choose v4 Lightshow DAT", "", "dat"); if (!string.IsNullOrEmpty(chosen)) lightshowPath = chosen; }
            if (GUILayout.Button("Clear", GUILayout.Width(55))) lightshowPath = "";
        }
        useInfoSettings = EditorGUILayout.Toggle("Read timing from Info.dat", useInfoSettings);
        bpm = EditorGUILayout.FloatField("BPM (manual if Info timing is off)", bpm);
        defaultNjs = EditorGUILayout.FloatField("Default NJS", defaultNjs);
        defaultJumpOffset = EditorGUILayout.FloatField("Default jump offset", defaultJumpOffset);
        if (!string.IsNullOrEmpty(detectedEnvironment)) EditorGUILayout.LabelField("Info environment", detectedEnvironment);
        if (!string.IsNullOrEmpty(detectedSongFilename)) EditorGUILayout.LabelField("Info song file", detectedSongFilename);
        song = (AudioClip)EditorGUILayout.ObjectField("Song (optional)", song, typeof(AudioClip), false);
        includeCustomEvents = EditorGUILayout.Toggle("Readable custom-event tracks", includeCustomEvents);
        includeNotes = EditorGUILayout.Toggle("Readable note clips", includeNotes);
        includeGameplay = EditorGUILayout.Toggle("Readable bombs/walls/arcs/chains", includeGameplay);
        groupByEventType = EditorGUILayout.Toggle("One track per event type", groupByEventType);
        addPreviewDriver = EditorGUILayout.Toggle("Add real preview driver", addPreviewDriver);
        addAudioTrack = EditorGUILayout.Toggle("Add Timeline AudioTrack (OFF for locked x1.0)", addAudioTrack);

        EditorGUILayout.Space();
        GUI.enabled = File.Exists(datPath) && bpm > 0f;
        if (GUILayout.Button("1. Create Timeline + embedded map data", GUILayout.Height(34))) lastTimeline = CreateTimeline();
        GUI.enabled = true;
        EditorGUILayout.Space(8);
        lastTimeline = (TimelineAsset)EditorGUILayout.ObjectField("Timeline for 3D rig", lastTimeline, typeof(TimelineAsset), false);
        song = (AudioClip)EditorGUILayout.ObjectField("Rig song override", song, typeof(AudioClip), false);
        GUI.enabled = lastTimeline != null;
        if (GUILayout.Button("2. Create 3D Preview Rig", GUILayout.Height(34))) BeatSaberVivifyPreviewBuilder.CreateRig(lastTimeline, song, true, true, true);
        GUI.enabled = true;
        EditorGUILayout.HelpBox("For a v4 map, choose its separate Lightshow DAT if you want the fallback environment lights to react. Unknown/custom mods are still safe to open: unsupported pieces are skipped while standard gameplay objects remain visible.", MessageType.None);
    }

    private bool LoadSettingsFromInfo()
    {
        if (!File.Exists(infoPath) || !File.Exists(datPath)) return false;
        var settings = ScriptableObject.CreateInstance<BeatSaberMapData>();
        try
        {
            BeatSaberPreviewMapMetadata.ApplyInfo(settings, infoPath, datPath);
            bpm = settings.bpm;
            defaultNjs = settings.defaultNjs;
            defaultJumpOffset = settings.defaultJumpOffset;
            detectedEnvironment = settings.environmentName;
            detectedSongFilename = settings.songFilename;
            if (!string.IsNullOrEmpty(settings.sourceLightshowPath)) lightshowPath = settings.sourceLightshowPath;
            if (song == null) song = BeatSaberPreviewMapMetadata.FindSong(detectedSongFilename);
            Repaint();
            return true;
        }
        catch (Exception ex)
        {
            EditorUtility.DisplayDialog("Beat Saber Timeline V6.0", "Could not parse Info.dat:\\n" + ex.Message, "OK");
            return false;
        }
        finally { DestroyImmediate(settings); }
    }

    private TimelineAsset CreateTimeline()
    {
        if (useInfoSettings)
        {
            if (!File.Exists(infoPath)) infoPath = Path.Combine(Path.GetDirectoryName(datPath) ?? "", "Info.dat");
            if (!File.Exists(infoPath))
            {
                EditorUtility.DisplayDialog("Map timing required", "Select Info.dat, or explicitly turn off Info timing and enter the map's BPM. Playback will not guess 120 BPM.", "OK");
                return null;
            }
            if (!LoadSettingsFromInfo()) return null;
        }
        string json, lightJson = ""; Dictionary<string, object> root, lightRoot = null;
        try
        {
            json = File.ReadAllText(datPath); root = BeatSaberMiniJson.Deserialize(json) as Dictionary<string, object>; if (root == null) throw new Exception("Root JSON is not an object.");
            if (!string.IsNullOrEmpty(lightshowPath) && File.Exists(lightshowPath)) { lightJson = File.ReadAllText(lightshowPath); lightRoot = BeatSaberMiniJson.Deserialize(lightJson) as Dictionary<string, object>; }
        }
        catch (Exception ex) { EditorUtility.DisplayDialog("Beat Saber Timeline V6.0", "Could not read DAT:\n" + ex.Message, "OK"); return null; }

        string baseName = Path.GetFileNameWithoutExtension(datPath);
        string outPath = EditorUtility.SaveFilePanelInProject("Save Timeline Asset", baseName + "_Timeline_V5", "playable", "Choose where to save the generated TimelineAsset."); if (string.IsNullOrEmpty(outPath)) return null;
        var timeline = ScriptableObject.CreateInstance<TimelineAsset>(); timeline.name = Path.GetFileNameWithoutExtension(outPath); AssetDatabase.CreateAsset(timeline, outPath);

        double maxBeat = BeatSaberMapCompat.FindMaxBeat(root, lightRoot); double secondsPerBeat = 60.0 / Math.Max(0.001, bpm); double maxTime = Math.Max(0.1, maxBeat * secondsPerBeat);
        var mapData = ScriptableObject.CreateInstance<BeatSaberMapData>(); mapData.name = baseName + "_MapData"; mapData.json = json; mapData.lightshowJson = lightJson; mapData.sourceName = baseName; mapData.sourceDatPath = datPath; mapData.sourceLightshowPath = lightshowPath; mapData.sourceInfoPath = infoPath; mapData.songFilename = detectedSongFilename; mapData.environmentName = detectedEnvironment; mapData.detectedFormat = BeatSaberMapCompat.GetVersion(root); mapData.bpm = bpm; mapData.defaultNjs = defaultNjs; mapData.defaultJumpOffset = defaultJumpOffset; mapData.maxBeat = maxBeat; AssetDatabase.AddObjectToAsset(mapData, timeline);

        var tracks = new Dictionary<string, BeatSaberEventTrack>();
        Func<string, BeatSaberEventTrack> getTrack = delegate(string key) { string safe = string.IsNullOrEmpty(key) ? "Events" : key; BeatSaberEventTrack t; if (!tracks.TryGetValue(safe, out t)) { t = timeline.CreateTrack<BeatSaberEventTrack>(null, safe); tracks[safe] = t; } return t; };

        var customEvents = BeatSaberMapCompat.GetCustomEvents(root);
        if (includeCustomEvents && customEvents != null)
        {
            for (int i = 0; i < customEvents.Count; i++)
            {
                var ev = customEvents[i] as Dictionary<string, object>; if (ev == null) continue; double beat = GetNumber(ev, "b", 0d); string type = GetString(ev, "t", "CustomEvent"); var d = GetDict(ev, "d"); string trackName = TrackText(d == null ? null : GetValue(d, "track")); double durationBeats = d == null ? 0d : GetNumber(d, "duration", 0d);
                var clip = getTrack(groupByEventType ? type : "Custom Events").CreateClip<BeatSaberEventAsset>(); clip.start = beat * secondsPerBeat; double durSec = durationBeats > 0 ? durationBeats * secondsPerBeat : Math.Max(1.0 / 60.0, secondsPerBeat * 0.03); if (durationBeats > 1000) durSec = Math.Max(0.01, maxTime - clip.start); clip.duration = Math.Max(0.01, Math.Min(durSec, Math.Max(0.01, maxTime - clip.start)));
                var a = clip.asset as BeatSaberEventAsset; a.eventType = type; a.trackName = trackName; a.beat = beat; a.durationBeats = durationBeats; a.rawJson = JsonPretty.Snapshot(ev); clip.displayName = string.IsNullOrEmpty(trackName) ? type + " · b" + Fmt(beat) : type + " · " + trackName + " · b" + Fmt(beat);
            }
        }

        if (includeNotes)
        {
            var notes = BeatSaberMapCompat.GetColorNotes(root); if (notes != null) { var tr = getTrack("Color Notes"); for (int i = 0; i < notes.Count; i++) { var n = notes[i] as Dictionary<string, object>; if (n == null) continue; double beat = GetNumber(n, "b", 0d); int c = (int)GetNumber(n, "c", -1); var clip = tr.CreateClip<BeatSaberEventAsset>(); clip.start = beat * secondsPerBeat; clip.duration = Math.Max(0.01, secondsPerBeat * 0.05); var a = clip.asset as BeatSaberEventAsset; a.eventType = c == 0 ? "Note L" : c == 1 ? "Note R" : "Note"; a.beat = beat; a.rawJson = JsonPretty.Snapshot(n); var cd = GetDict(n, "customData"); a.trackName = TrackText(cd == null ? null : GetValue(cd, "track")); clip.displayName = a.eventType + " · b" + Fmt(beat); } }
        }

        if (includeGameplay)
        {
            AddSimpleClips(getTrack, "Bombs", "Bomb", BeatSaberMapCompat.GetBombNotes(root), secondsPerBeat);
            AddSimpleClips(getTrack, "Obstacles", "Wall", BeatSaberMapCompat.GetObstacles(root), secondsPerBeat);
            AddRangeClips(getTrack, "Arcs", "Arc", BeatSaberMapCompat.GetSliders(root), secondsPerBeat);
            AddRangeClips(getTrack, "Chains", "Chain", BeatSaberMapCompat.GetBurstSliders(root), secondsPerBeat);
            AddSimpleClips(getTrack, "Basic Lights", "Light", BeatSaberMapCompat.GetBasicEvents(root, lightRoot), secondsPerBeat);
        }

        if (addPreviewDriver)
        {
            var driver = timeline.CreateTrack<BeatSaberPreviewDriverTrack>(null, "★ REAL 3D PREVIEW DRIVER V5"); var clip = driver.CreateClip<BeatSaberPreviewDriverAsset>(); clip.start = 0; clip.duration = maxTime; var a = clip.asset as BeatSaberPreviewDriverAsset; a.mapData = mapData; clip.displayName = "Beat Saber / Vivify Preview V5 · " + baseName;
        }
        if (song != null && addAudioTrack) { try { var audio = timeline.CreateTrack<AudioTrack>(null, "♪ Song"); var ac = audio.CreateClip<AudioPlayableAsset>(); var aa = ac.asset as AudioPlayableAsset; if (aa != null) aa.clip = song; ac.start = 0; ac.duration = Math.Min(song.length, maxTime > 0 ? maxTime : song.length); } catch (Exception ex) { Debug.LogWarning("Could not create AudioTrack: " + ex.Message); } }

        EditorUtility.SetDirty(mapData); EditorUtility.SetDirty(timeline); AssetDatabase.SaveAssets(); AssetDatabase.Refresh(); Selection.activeObject = timeline; EditorGUIUtility.PingObject(timeline);
        EditorUtility.DisplayDialog("Beat Saber Timeline V6.0", "Created.\n\nFormat: " + mapData.detectedFormat + "\nLength: " + maxTime.ToString("0.00") + "s\nMax beat: " + Fmt(maxBeat) + "\nReadable tracks: " + tracks.Count + "\nV4 lightshow embedded: " + (!string.IsNullOrEmpty(lightJson) ? "yes" : "no") + "\nPreview driver: " + (addPreviewDriver ? "yes" : "no"), "OK");
        return timeline;
    }

    private static void AddSimpleClips(Func<string, BeatSaberEventTrack> getTrack, string trackName, string label, List<object> list, double spb)
    {
        if (list == null || list.Count == 0) return; var tr = getTrack(trackName);
        for (int i = 0; i < list.Count; i++) { var d = list[i] as Dictionary<string, object>; if (d == null) continue; double beat = GetNumber(d, "b", 0d); var clip = tr.CreateClip<BeatSaberEventAsset>(); clip.start = beat * spb; clip.duration = Math.Max(0.01, spb * 0.05); var a = clip.asset as BeatSaberEventAsset; a.eventType = label; a.beat = beat; a.rawJson = JsonPretty.Snapshot(d); clip.displayName = label + " · b" + Fmt(beat); }
    }

    private static void AddRangeClips(Func<string, BeatSaberEventTrack> getTrack, string trackName, string label, List<object> list, double spb)
    {
        if (list == null || list.Count == 0) return; var tr = getTrack(trackName);
        for (int i = 0; i < list.Count; i++) { var d = list[i] as Dictionary<string, object>; if (d == null) continue; double beat = GetNumber(d, "b", 0d), tail = GetNumber(d, "tb", beat); var clip = tr.CreateClip<BeatSaberEventAsset>(); clip.start = beat * spb; clip.duration = Math.Max(0.01, (tail - beat) * spb); var a = clip.asset as BeatSaberEventAsset; a.eventType = label; a.beat = beat; a.durationBeats = Math.Max(0d, tail - beat); a.rawJson = JsonPretty.Snapshot(d); clip.displayName = label + " · b" + Fmt(beat) + "→" + Fmt(tail); }
    }

    private static object GetValue(Dictionary<string, object> d, string k) { object v; return d != null && d.TryGetValue(k, out v) ? v : null; }
    private static Dictionary<string, object> GetDict(Dictionary<string, object> d, string k) { return GetValue(d, k) as Dictionary<string, object>; }
    private static string GetString(Dictionary<string, object> d, string k, string f) { object v = GetValue(d, k); return v == null ? f : Convert.ToString(v); }
    private static double GetNumber(Dictionary<string, object> d, string k, double f) { object v = GetValue(d, k); try { return v == null ? f : Convert.ToDouble(v, CultureInfo.InvariantCulture); } catch { return f; } }
    private static string TrackText(object v) { var l = v as List<object>; if (l != null) { var s = new List<string>(); for (int i = 0; i < l.Count; i++) s.Add(Convert.ToString(l[i])); return string.Join(", ", s.ToArray()); } return v == null ? "" : Convert.ToString(v); }
    private static string Fmt(double d) { return d.ToString("0.###", CultureInfo.InvariantCulture); }

    private static class JsonPretty
    {
        public static string Snapshot(object obj) { return Serialize(obj); }
        private static string Serialize(object o)
        {
            if (o == null) return "null"; if (o is string) return "\"" + ((string)o).Replace("\\", "\\\\").Replace("\"", "\\\"") + "\""; if (o is bool) return (bool)o ? "true" : "false"; if (o is IFormattable) return ((IFormattable)o).ToString(null, CultureInfo.InvariantCulture);
            var d = o as Dictionary<string, object>; if (d != null) { var parts = new List<string>(); foreach (var kv in d) parts.Add("\"" + kv.Key + "\": " + Serialize(kv.Value)); return "{ " + string.Join(", ", parts.ToArray()) + " }"; }
            var l = o as List<object>; if (l != null) { var parts = new List<string>(); for (int i = 0; i < l.Count; i++) parts.Add(Serialize(l[i])); return "[" + string.Join(", ", parts.ToArray()) + "]"; }
            return "\"" + Convert.ToString(o) + "\"";
        }
    }
}
}
#endif
