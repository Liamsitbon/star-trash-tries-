#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.Timeline;

namespace VivifyTimelinePreview
{
    public static class BeatSaberPreviewMapMetadata
    {
        public static void ApplyInfo(BeatSaberMapData data, string infoPath, string datPath)
        {
            var info = BeatSaberMiniJson.Deserialize(File.ReadAllText(infoPath)) as Dictionary<string, object>;
            if (info == null) throw new InvalidDataException("Info.dat must contain an object.");
            bool v4 = info.ContainsKey("difficultyBeatmaps");
            var audio = Dict(info, "audio");
            data.bpm = (float)Number(v4 ? audio : info, v4 ? "bpm" : "_beatsPerMinute", 0d);
            if (data.bpm <= 0f || float.IsNaN(data.bpm) || float.IsInfinity(data.bpm))
                throw new InvalidDataException("Info.dat has no valid BPM. Set timing manually only if you know the map's BPM.");
            data.songFilename = Text(v4 ? audio : info, v4 ? "songFilename" : "_songFilename");
            data.environmentName = Text(info, "_environmentName");
            data.defaultTimelineOffsetSeconds = (float)Number(info, "_songTimeOffset", 0d);
            data.sourceInfoPath = infoPath;
            data.sourceDatPath = datPath;
            string filename = Path.GetFileName(datPath);
            var difficulties = new List<object>();
            if (v4) difficulties.AddRange(List(info, "difficultyBeatmaps"));
            else foreach (object set in List(info, "_difficultyBeatmapSets")) difficulties.AddRange(List(set as Dictionary<string, object>, "_difficultyBeatmaps"));
            foreach (object item in difficulties)
            {
                var difficulty = item as Dictionary<string, object>;
                if (!string.Equals(Path.GetFileName(Text(difficulty, v4 ? "beatmapDataFilename" : "_beatmapFilename")), filename, StringComparison.OrdinalIgnoreCase)) continue;
                data.defaultNjs = (float)Number(difficulty, v4 ? "noteJumpMovementSpeed" : "_noteJumpMovementSpeed", data.defaultNjs);
                data.defaultJumpOffset = (float)Number(difficulty, v4 ? "noteJumpStartBeatOffset" : "_noteJumpStartBeatOffset", data.defaultJumpOffset);
                var environments = List(info, v4 ? "environmentNames" : "_environmentNames");
                int index = (int)Number(difficulty, v4 ? "environmentNameIdx" : "_environmentNameIdx", -1d);
                if (index >= 0 && index < environments.Count) data.environmentName = Convert.ToString(environments[index]);
                string lightshow = Text(difficulty, "lightshowDataFilename");
                if (!string.IsNullOrEmpty(lightshow)) data.sourceLightshowPath = Path.Combine(Path.GetDirectoryName(infoPath) ?? "", lightshow);
                return;
            }
            throw new InvalidDataException("The selected DAT is not a difficulty in this Info.dat.");
        }

        public static TimelineAsset CopyWithCorrectTiming(TimelineAsset original)
        {
            var data = BeatSaberVivifyPreviewBuilder.FindMapData(original);
            if (data == null || !File.Exists(data.sourceInfoPath)) return original;
            var corrected = UnityEngine.Object.Instantiate(data);
            corrected.hideFlags = HideFlags.HideAndDontSave;
            try
            {
                ApplyInfo(corrected, data.sourceInfoPath, data.sourceDatPath);
                if (Mathf.Approximately(data.bpm, corrected.bpm) && Mathf.Approximately(data.defaultNjs, corrected.defaultNjs) &&
                    Mathf.Approximately(data.defaultJumpOffset, corrected.defaultJumpOffset)) return original;
                string originalPath = AssetDatabase.GetAssetPath(original);
                const string folder = "Assets/VivifyTimelinePreview/GeneratedTimelines";
                Directory.CreateDirectory(folder);
                AssetDatabase.Refresh();
                string guid = AssetDatabase.AssetPathToGUID(originalPath);
                string path = folder + "/" + original.name + "_" + guid.Substring(0, 8) + "_V6.playable";
                var copy = AssetDatabase.LoadAssetAtPath<TimelineAsset>(path);
                if (copy != null) return copy;
                if (!AssetDatabase.CopyAsset(originalPath, path)) throw new IOException("Could not create a corrected Timeline copy.");
                copy = AssetDatabase.LoadAssetAtPath<TimelineAsset>(path);
                var copyData = BeatSaberVivifyPreviewBuilder.FindMapData(copy);
                double ratio = Math.Max(0.001d, data.bpm) / corrected.bpm;
                ApplyInfo(copyData, data.sourceInfoPath, data.sourceDatPath);
                foreach (var track in copy.GetOutputTracks())
                {
                    foreach (var clip in track.GetClips())
                    {
                        if (track is AudioTrack) continue;
                        clip.start *= ratio;
                        clip.duration *= ratio;
                    }
                    EditorUtility.SetDirty(track);
                }
                if (copy.durationMode == TimelineAsset.DurationMode.FixedLength) copy.fixedDuration *= ratio;
                EditorUtility.SetDirty(copyData);
                EditorUtility.SetDirty(copy);
                AssetDatabase.SaveAssets();
                Debug.Log("[Vivify Preview V6] Corrected copy: " + data.bpm + " -> " + corrected.bpm + " BPM; NJS " + corrected.defaultNjs + ". Original Timeline unchanged: " + path);
                return copy;
            }
            catch (Exception ex)
            {
                Debug.LogError("[Vivify Preview] Could not read map timing: " + ex.Message);
                throw;
            }
            finally { UnityEngine.Object.DestroyImmediate(corrected); }
        }

        [MenuItem("Tools/Beat Saber/Create corrected copies of preview Timelines")]
        public static void RepairGeneratedTimelines()
        {
            // Only assets containing this tool's embedded MapData are eligible.
            foreach (string guid in AssetDatabase.FindAssets("t:TimelineAsset"))
            {
                string path = AssetDatabase.GUIDToAssetPath(guid);
                if (path.StartsWith("Assets/VivifyTimelinePreview/GeneratedTimelines/", StringComparison.Ordinal)) continue;
                CopyWithCorrectTiming(AssetDatabase.LoadAssetAtPath<TimelineAsset>(path));
            }
        }

        public static AudioClip FindSong(string filename)
        {
            string name = Path.GetFileNameWithoutExtension(filename);
            if (string.IsNullOrEmpty(name)) return null;
            AudioClip match = null;
            foreach (string guid in AssetDatabase.FindAssets("t:AudioClip " + name))
            {
                var clip = AssetDatabase.LoadAssetAtPath<AudioClip>(AssetDatabase.GUIDToAssetPath(guid));
                if (clip == null || !string.Equals(clip.name, name, StringComparison.OrdinalIgnoreCase)) continue;
                if (match != null) return null; // Ambiguous audio stays a user choice.
                match = clip;
            }
            return match;
        }

        private static Dictionary<string, object> Dict(Dictionary<string, object> value, string key)
        { object item; return value != null && value.TryGetValue(key, out item) ? item as Dictionary<string, object> : null; }
        private static List<object> List(Dictionary<string, object> value, string key)
        { object item; return value != null && value.TryGetValue(key, out item) && item is List<object> ? (List<object>)item : new List<object>(); }
        private static string Text(Dictionary<string, object> value, string key)
        { object item; return value != null && value.TryGetValue(key, out item) ? Convert.ToString(item) : ""; }
        private static double Number(Dictionary<string, object> value, string key, double fallback)
        { object item; return value != null && value.TryGetValue(key, out item) && item != null ? Convert.ToDouble(item, System.Globalization.CultureInfo.InvariantCulture) : fallback; }
    }
}
#endif
