#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEditor;
using UnityEngine;
using UnityEngine.Playables;
using UnityEngine.Timeline;

namespace VivifyTimelinePreview
{
public static class BeatSaberVivifyPreviewBuilder
{
    private class NotePrefabAssignment
    {
        public string asset;
        public string debrisAsset;
        public string anyDirectionAsset;
        public string loadMode;
        public double beat;
    }

    [MenuItem("Tools/Beat Saber/Create 3D Preview Rig from Selected Timeline", true)]
    private static bool ValidateCreateFromSelection() { return Selection.activeObject is TimelineAsset; }

    [MenuItem("Tools/Beat Saber/Create 3D Preview Rig from Selected Timeline")]
    private static void CreateFromSelection()
    {
        var timeline = Selection.activeObject as TimelineAsset;
        if (timeline != null) CreateRig(timeline, null, true, true, true);
    }

    public static BeatSaberVivifyPreviewController CreateRig(TimelineAsset timeline, AudioClip song, bool instantiatePrefabs, bool createNotes, bool createCamera, bool showSummary = true)
    {
        if (timeline != null) timeline = BeatSaberPreviewMapMetadata.CopyWithCorrectTiming(timeline);
        if (timeline == null) { if (showSummary) EditorUtility.DisplayDialog("Beat Saber Preview Rig", "Choose a Timeline first.", "OK"); else Debug.LogError("Beat Saber Preview Rig: Timeline is null."); return null; }
        BeatSaberMapData mapData = FindMapData(timeline);
        if (mapData == null || string.IsNullOrEmpty(mapData.json)) { if (showSummary) EditorUtility.DisplayDialog("Beat Saber Preview Rig", "This Timeline has no embedded BeatSaberMapData. Re-create it with the V5 Timeline builder first.", "OK"); else Debug.LogError("Beat Saber Preview Rig: embedded BeatSaberMapData is missing or empty."); return null; }

        if (song == null) song = BeatSaberPreviewMapMetadata.FindSong(mapData.songFilename);
        var rootJson = BeatSaberMiniJson.Deserialize(mapData.json) as Dictionary<string, object>;
        var lightshowJson = !string.IsNullOrEmpty(mapData.lightshowJson) ? BeatSaberMiniJson.Deserialize(mapData.lightshowJson) as Dictionary<string, object> : null;
        if (rootJson == null) { if (showSummary) EditorUtility.DisplayDialog("Beat Saber Preview Rig", "Embedded DAT JSON could not be parsed.", "OK"); else Debug.LogError("Beat Saber Preview Rig: embedded DAT JSON could not be parsed."); return null; }
        var events = BeatSaberMapCompat.GetCustomEvents(rootJson);

        var root = new GameObject(mapData.sourceName + " [Beat Saber Timeline Preview V6.0]");
        Undo.RegisterCreatedObjectUndo(root, "Create Beat Saber Timeline Preview Rig");
        var director = root.AddComponent<PlayableDirector>();
        director.playableAsset = timeline;
        director.extrapolationMode = DirectorWrapMode.None;
        director.playOnAwake = false;
        director.timeUpdateMode = DirectorUpdateMode.Manual;

        var audio = root.AddComponent<AudioSource>();
        audio.playOnAwake = false;
        audio.clip = song;

        var controller = root.AddComponent<BeatSaberVivifyPreviewController>();
        controller.timeline = timeline;
        controller.mapData = mapData;
        controller.director = director;
        controller.song = song;
        controller.audioSource = audio;
        controller.timelineOffsetSeconds = mapData.defaultTimelineOffsetSeconds;
        controller.syncAudioSourceWhenPlaying = false;
        controller.previewCustomSabers = true;
        controller.forceStandardNoteFallback = false;
        controller.alwaysShowStandardSabers = false;
        controller.previewObstacles = true;
        controller.saberReach = 1f;
        controller.saberMotionSmoothing = 24f;

        var playback = root.AddComponent<BeatSaberDeterministicPlayback>();
        playback.director = director;
        playback.preview = controller;
        playback.audioSource = audio;
        playback.song = song;
        playback.lockToOneX = true;
        playback.preBakeBeforePlayback = true;
        playback.autoPlayAfterPreBake = true;
        // Warm only materials used by this rig. Shader.WarmupAllShaders() on a full
        // Vivify project can block the editor for minutes before the first frame.
        playback.warmAllLoadedShaders = false;
        playback.settleFrames = 2;

        Transform generated = NewChild(root.transform, "Generated Preview");
        Transform tracksRoot = NewChild(generated, "Tracks");
        Transform prefabsRoot = NewChild(generated, "Prefabs");
        Transform notesRoot = NewChild(generated, "Notes");
        Transform gameplayRoot = NewChild(generated, "Gameplay");
        Transform sabersRoot = NewChild(generated, "Sabers");
        Transform helpersRoot = NewChild(generated, "Helpers");
        Transform spectatorRoot = NewChild(generated, "Spectator");
        controller.generatedRoot = generated; controller.trackRoot = tracksRoot; controller.prefabRoot = prefabsRoot; controller.noteRoot = notesRoot; controller.gameplayRoot = gameplayRoot; controller.saberRoot = sabersRoot; controller.spectatorRoot = spectatorRoot;

        HashSet<string> trackNames = CollectTrackNames(rootJson, events);
        var trackObjects = new Dictionary<string, Transform>(StringComparer.Ordinal);
        foreach (string trackName in trackNames)
        {
            if (string.IsNullOrEmpty(trackName)) continue;
            var tr = NewChild(tracksRoot, SafeName(trackName)); trackObjects[trackName] = tr;
            controller.tracks.Add(new BeatSaberVivifyPreviewController.TrackBinding { track = trackName, transform = tr });
        }

        Transform player = EnsureTrack("player", tracksRoot, trackObjects, controller);
        Transform head = EnsureTrack("head", tracksRoot, trackObjects, controller);
        Transform leftHand = EnsureTrack("leftHand", tracksRoot, trackObjects, controller);
        Transform rightHand = EnsureTrack("rightHand", tracksRoot, trackObjects, controller);
        if (head.parent == tracksRoot) head.SetParent(player, false);
        if (leftHand.parent == tracksRoot) leftHand.SetParent(player, false);
        if (rightHand.parent == tracksRoot) rightHand.SetParent(player, false);
        head.localPosition = Vector3.zero;
        // Put the preview controllers inside the player camera frustum. V4 used z=0.48,
        // which placed the handles below a 60-degree camera on many aspect ratios.
        // Stable Beat Saber-like first-person rest pose: low and apart, blades tilted
        // slightly upward/forward instead of pointing straight at the camera.
        leftHand.localPosition = new Vector3(-0.25f, 1.1f, 0.4f);
        rightHand.localPosition = new Vector3(0.25f, 1.1f, 0.4f);
        leftHand.localRotation = Quaternion.Euler(-22f, 7f, -7f);
        rightHand.localRotation = Quaternion.Euler(-22f, -7f, 7f);

        if (createCamera)
        {
            var camGo = new GameObject("Preview Camera"); camGo.transform.SetParent(head, false); camGo.transform.localPosition = new Vector3(0f, 1.6f, 0f);
            var cam = camGo.AddComponent<Camera>(); cam.nearClipPlane = 0.01f; cam.farClipPlane = 3000f; controller.previewCamera = cam; controller.blitPreview = camGo.AddComponent<BeatSaberBlitPreview>();

            var spectatorGo = new GameObject("Spectator Camera"); spectatorGo.transform.SetParent(spectatorRoot, false); spectatorGo.transform.position = new Vector3(0f, 4.5f, -14f); spectatorGo.transform.rotation = Quaternion.Euler(10f, 0f, 0f);
            var spectator = spectatorGo.AddComponent<Camera>(); spectator.nearClipPlane = 0.01f; spectator.farClipPlane = 4000f; spectator.enabled = false;
            var spectatorBlit = spectatorGo.AddComponent<BeatSaberBlitPreview>(); spectatorBlit.enabled = false;
            var controls = spectatorGo.AddComponent<BeatSaberSpectatorCamera>(); controls.orbitTarget = generated;
            controller.spectatorCamera = spectator; controller.spectatorBlitPreview = spectatorBlit; controller.spectatorControls = controls;
        }

        ApplyTrackParenting(events, trackObjects);
        controller.genericEnvironment = BeatSaberProceduralFactory.CreateGenericEnvironment(generated);

        var missing = new List<string>();
        if (instantiatePrefabs) BuildPrefabInstances(events, prefabsRoot, trackObjects, controller, missing);
        if (createNotes) BuildNotes(rootJson, events, notesRoot, controller, missing);
        BuildBombs(rootJson, gameplayRoot, controller);
        BuildObstacles(rootJson, gameplayRoot, controller);
        BuildArcsAndChains(rootJson, gameplayRoot, controller);
        Transform leftMotion = NewChild(leftHand, "Left Saber Motion");
        Transform rightMotion = NewChild(rightHand, "Right Saber Motion");
        controller.sabers.leftMotionPivot = leftMotion; controller.sabers.rightMotionPivot = rightMotion;
        BuildSabers(events, sabersRoot, leftMotion, rightMotion, controller, missing);
        AssignDestroyBeats(events, controller);
        // Capture the original Vivify material slots before replacing any source shaders.
        // Runtime can then drive SetMaterialProperty on a Metal-safe clone without losing
        // the renderer/slot association used by Vivify.
        BuildMaterialBindings(events, controller, missing);
        RepairAllGeneratedMaterialsForCurrentPlatform(generated, controller.leftNoteColor);

        foreach (var tr in timeline.GetOutputTracks()) if (tr is BeatSaberPreviewDriverTrack) director.SetGenericBinding(tr, controller);
        RemoveAutoSongTracks(timeline); // V5.9 uses exactly one scheduled AudioSource; Timeline audio caused audible double/late playback.

        controller.ForceReparse();
        // Pre-bake immediately in Edit Mode too. Runtime performs the same warm-up before
        // audio starts, which prevents first-play shader/prefab loading from changing the
        // apparent timing.
        controller.PreBakePreview(false);
        director.time = 0;
        director.Evaluate();
        Selection.activeGameObject = root;
        EditorGUIUtility.PingObject(root);

        string version = BeatSaberMapCompat.GetVersion(rootJson);
        string summary = "Created V5.9 preview rig.\n\nMap format: " + version + "\nNotes: " + controller.notes.Count + "\nBombs: " + controller.bombs.Count + "\nWalls: " + controller.obstacles.Count + "\nArcs: " + controller.arcs.Count + "\nChains: " + controller.chains.Count + "\nCustom saber prefab: " + ((controller.sabers.leftCustom != null || controller.sabers.rightCustom != null) ? "loaded (source Vivify saber is preferred; fallback only if source is missing/incomplete)" : "not present") + "\nPre-baked material passes: " + controller.PreBakedMaterialPasses + "\nPlayback: single scheduled AudioSource + audible-latency-compensated DSP x1.0\nGeneric environment: available (Auto hides it on detected Vivify maps).";
        if (showSummary && missing.Count > 0)
        {
            string msg = summary + "\n\nSome source assets could not be resolved; V5 falls back to standard Beat Saber-style visuals:\n" + string.Join("\n", missing.ToArray()); if (msg.Length > 3900) msg = msg.Substring(0, 3900) + "\n...";
            EditorUtility.DisplayDialog("Beat Saber Preview Rig", msg, "OK");
        }
        else if (showSummary) EditorUtility.DisplayDialog("Beat Saber Preview Rig", summary, "OK");
        return controller;
    }

    private static void BuildPrefabInstances(List<object> events, Transform prefabsRoot, Dictionary<string, Transform> trackObjects, BeatSaberVivifyPreviewController controller, List<string> missing)
    {
        if (events == null) return;
        for (int i = 0; i < events.Count; i++)
        {
            var ev = events[i] as Dictionary<string, object>; if (ev == null || GetString(ev, "t") != "InstantiatePrefab") continue;
            var d = GetDict(ev, "d"); string asset = GetString(d, "asset"); string id = GetString(d, "id"); string track = FirstTrack(GetValue(d, "track")); double beat = GetNumber(ev, "b", 0d);
            GameObject prefab = BeatSaberAssetResolver.Load<GameObject>(asset, "Prefab"); if (prefab == null) { missing.Add("Prefab: " + asset); continue; }
            Transform parent = prefabsRoot; Transform tr; if (!string.IsNullOrEmpty(track) && trackObjects.TryGetValue(track, out tr)) parent = tr;
            GameObject instance = PrefabUtility.InstantiatePrefab(prefab, parent) as GameObject; if (instance == null) instance = UnityEngine.Object.Instantiate(prefab, parent);
            instance.name = string.IsNullOrEmpty(id) ? prefab.name + " [Preview]" : id; RepairUnsupportedMaterials(instance, Color.white); instance.SetActive(false);
            controller.objects.Add(new BeatSaberVivifyPreviewController.PreviewObjectBinding { id = id, track = track, assetPath = asset, instance = instance, instantiateBeat = beat, destroyBeat = 999999d });
        }
    }

    private static void AssignDestroyBeats(List<object> events, BeatSaberVivifyPreviewController controller)
    {
        if (events == null) return;
        for (int i = 0; i < events.Count; i++)
        {
            var ev = events[i] as Dictionary<string, object>; if (ev == null || GetString(ev, "t") != "DestroyObject") continue;
            var d = GetDict(ev, "d"); string id = GetString(d, "id"); double beat = GetNumber(ev, "b", 0d);
            for (int j = 0; j < controller.objects.Count; j++) if (controller.objects[j] != null && controller.objects[j].id == id) controller.objects[j].destroyBeat = beat;
        }
    }

    private static void BuildMaterialBindings(List<object> events, BeatSaberVivifyPreviewController controller, List<string> missing)
    {
        var paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        if (events != null) for (int i = 0; i < events.Count; i++) { var ev = events[i] as Dictionary<string, object>; if (ev == null) continue; string type = GetString(ev, "t"); if (type != "SetMaterialProperty" && type != "Blit") continue; var d = GetDict(ev, "d"); string p = GetString(d, "asset"); if (!string.IsNullOrEmpty(p)) paths.Add(p); }
        var renderers = controller.generatedRoot != null ? controller.generatedRoot.GetComponentsInChildren<Renderer>(true) : new Renderer[0];
        foreach (string path in paths)
        {
            Material mat = BeatSaberAssetResolver.Load<Material>(path, "Material"); if (mat == null) { missing.Add("Material: " + path); continue; }
            var binding = new BeatSaberVivifyPreviewController.MaterialBinding { assetPath = path, sourceMaterial = mat };
            for (int r = 0; r < renderers.Length; r++) { var mats = renderers[r].sharedMaterials; for (int s = 0; s < mats.Length; s++) if (BeatSaberPreviewMaterials.SourceOf(mats[s]) == mat) binding.slots.Add(new BeatSaberVivifyPreviewController.MaterialSlot { renderer = renderers[r], slot = s }); }
            controller.materials.Add(binding);
        }
    }

    private static void BuildNotes(Dictionary<string, object> rootJson, List<object> events, Transform notesRoot, BeatSaberVivifyPreviewController controller, List<string> missing)
    {
        var colorNotes = BeatSaberMapCompat.GetColorNotes(rootJson); if (colorNotes == null) return; var unresolved = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        for (int i = 0; i < colorNotes.Count; i++)
        {
            var n = colorNotes[i] as Dictionary<string, object>; if (n == null) continue;
            double beat = GetNumber(n, "b", 0d); int x = (int)GetNumber(n, "x", 0), y = (int)GetNumber(n, "y", 0), color = (int)GetNumber(n, "c", 0), direction = (int)GetNumber(n, "d", 8);
            var cd = GetDict(n, "customData"); string[] tracks = TrackArray(cd == null ? null : GetValue(cd, "track")); float njs = (float)GetNumber(cd, "noteJumpMovementSpeed", controller.mapData != null ? controller.mapData.defaultNjs : 10d); float offset = (float)GetNumber(cd, "noteJumpStartBeatOffset", controller.mapData != null ? controller.mapData.defaultJumpOffset : 0d);
            NotePrefabAssignment assignment = ResolveNotePrefabAssignment(events, tracks, beat);
            GameObject instance = null, debrisInstance = null; bool sourceVisible = false, debrisVisible = false; string asset = assignment == null ? "" : ((direction == 8 && !string.IsNullOrEmpty(assignment.anyDirectionAsset)) ? assignment.anyDirectionAsset : assignment.asset); string debrisAsset = assignment == null ? "" : assignment.debrisAsset; string loadMode = assignment == null ? "" : assignment.loadMode;
            if (!string.IsNullOrEmpty(asset)) { var prefab = BeatSaberAssetResolver.Load<GameObject>(asset, "Prefab"); if (prefab != null) { instance = PrefabUtility.InstantiatePrefab(prefab, notesRoot) as GameObject; if (instance == null) instance = UnityEngine.Object.Instantiate(prefab, notesRoot); RepairUnsupportedMaterials(instance, color == 0 ? controller.leftNoteColor : controller.rightNoteColor); sourceVisible = HasRenderableGeometry(instance); } else unresolved.Add(asset); }
            if (instance == null) { instance = new GameObject("Custom Note Placeholder"); instance.transform.SetParent(notesRoot, false); }
            if (!string.IsNullOrEmpty(debrisAsset)) { var prefab = BeatSaberAssetResolver.Load<GameObject>(debrisAsset, "Prefab"); if (prefab != null) { debrisInstance = PrefabUtility.InstantiatePrefab(prefab, notesRoot) as GameObject; if (debrisInstance == null) debrisInstance = UnityEngine.Object.Instantiate(prefab, notesRoot); debrisInstance.name = "Note Debris b" + beat.ToString("0.###", CultureInfo.InvariantCulture); RepairUnsupportedMaterials(debrisInstance, color == 0 ? controller.leftNoteColor : controller.rightNoteColor); debrisVisible = HasRenderableGeometry(debrisInstance); debrisInstance.SetActive(false); } else unresolved.Add(debrisAsset); }
            GameObject standard = BeatSaberProceduralFactory.CreateStandardNote(notesRoot, color, direction, controller.leftNoteColor, controller.rightNoteColor); standard.name = "Standard Note b" + beat.ToString("0.###", CultureInfo.InvariantCulture); StyleGeneratedNoteFromAssignment(standard, asset, color == 0 ? controller.leftNoteColor : controller.rightNoteColor, color == 0 ? controller.rightNoteColor : controller.leftNoteColor); standard.SetActive(false);
            GameObject standardDebris = BeatSaberProceduralFactory.CreateStandardNoteDebris(notesRoot, color, controller.leftNoteColor, controller.rightNoteColor); standardDebris.name = "Standard Note Debris b" + beat.ToString("0.###", CultureInfo.InvariantCulture); standardDebris.SetActive(false);
            GameObject proxy = GameObject.CreatePrimitive(PrimitiveType.Cube); proxy.name = "__DEBUG Note Proxy"; proxy.transform.SetParent(notesRoot, false); proxy.transform.localScale = Vector3.one * 0.5f; var col = proxy.GetComponent<Collider>(); if (col != null) UnityEngine.Object.DestroyImmediate(col); proxy.SetActive(false);
            instance.name = "Custom Note b" + beat.ToString("0.###", CultureInfo.InvariantCulture) + " [" + string.Join(",", tracks) + "]"; instance.SetActive(false);
            controller.notes.Add(new BeatSaberVivifyPreviewController.NoteBinding { instance = instance, standardVisual = standard, standardDebris = standardDebris, debugProxy = proxy, debrisInstance = debrisInstance, sourcePrefabHasVisibleRenderer = sourceVisible, debrisPrefabHasVisibleRenderer = debrisVisible, assetPath = asset, debrisAssetPath = debrisAsset, loadMode = loadMode, beat = beat, x = x, y = y, color = color, direction = direction, mapIndex = i, njs = njs, jumpOffset = offset, tracks = tracks, prefabScale = instance.transform.localScale, prefabEuler = instance.transform.localEulerAngles, debrisScale = debrisInstance != null ? debrisInstance.transform.localScale : Vector3.one, debrisEuler = debrisInstance != null ? debrisInstance.transform.localEulerAngles : Vector3.zero });
        }
        foreach (var path in unresolved) missing.Add("Note source asset: " + path);
    }

    private static void BuildBombs(Dictionary<string, object> rootJson, Transform parent, BeatSaberVivifyPreviewController controller)
    {
        var list = BeatSaberMapCompat.GetBombNotes(rootJson); if (list == null) return;
        for (int i = 0; i < list.Count; i++) { var b = list[i] as Dictionary<string, object>; if (b == null) continue; var cd = GetDict(b, "customData"); GameObject go = BeatSaberProceduralFactory.CreateBomb(parent); go.SetActive(false); controller.bombs.Add(new BeatSaberVivifyPreviewController.BombBinding { instance = go, beat = GetNumber(b, "b", 0d), x = (int)GetNumber(b, "x", 0d), y = (int)GetNumber(b, "y", 0d), njs = (float)GetNumber(cd, "noteJumpMovementSpeed", controller.mapData != null ? controller.mapData.defaultNjs : 10d), jumpOffset = (float)GetNumber(cd, "noteJumpStartBeatOffset", controller.mapData != null ? controller.mapData.defaultJumpOffset : 0d) }); }
    }

    private static void BuildObstacles(Dictionary<string, object> rootJson, Transform parent, BeatSaberVivifyPreviewController controller)
    {
        var list = BeatSaberMapCompat.GetObstacles(rootJson); if (list == null) return;
        for (int i = 0; i < list.Count; i++) { var o = list[i] as Dictionary<string, object>; if (o == null) continue; var cd = GetDict(o, "customData"); GameObject go = BeatSaberProceduralFactory.CreateWall(parent, new Color(0.92f, 0.08f, 0.14f, 0.075f)); go.SetActive(false); controller.obstacles.Add(new BeatSaberVivifyPreviewController.ObstacleBinding { instance = go, beat = GetNumber(o, "b", 0d), duration = GetNumber(o, "d", 1d), x = (int)GetNumber(o, "x", 0d), y = (int)GetNumber(o, "y", 0d), width = (int)GetNumber(o, "w", 1d), height = (int)GetNumber(o, "h", 5d), njs = (float)GetNumber(cd, "noteJumpMovementSpeed", controller.mapData != null ? controller.mapData.defaultNjs : 10d), jumpOffset = (float)GetNumber(cd, "noteJumpStartBeatOffset", controller.mapData != null ? controller.mapData.defaultJumpOffset : 0d) }); }
    }

    private static void BuildArcsAndChains(Dictionary<string, object> rootJson, Transform parent, BeatSaberVivifyPreviewController controller)
    {
        var arcs = BeatSaberMapCompat.GetSliders(rootJson); if (arcs != null) for (int i = 0; i < arcs.Count; i++) { var a = arcs[i] as Dictionary<string, object>; if (a == null) continue; int c = (int)GetNumber(a, "c", 0d); var lr = BeatSaberProceduralFactory.CreateArcLine(parent, c == 0 ? controller.leftNoteColor : controller.rightNoteColor, "Arc b" + GetNumber(a, "b", 0d).ToString("0.###")); lr.gameObject.SetActive(false); controller.arcs.Add(new BeatSaberVivifyPreviewController.PathVisualBinding { line = lr, beat = GetNumber(a, "b", 0d), tailBeat = GetNumber(a, "tb", GetNumber(a, "b", 0d)), x = (int)GetNumber(a, "x", 0d), y = (int)GetNumber(a, "y", 0d), tailX = (int)GetNumber(a, "tx", 0d), tailY = (int)GetNumber(a, "ty", 0d), color = c, slices = 16 }); }
        var chains = BeatSaberMapCompat.GetBurstSliders(rootJson); if (chains != null) for (int i = 0; i < chains.Count; i++) { var a = chains[i] as Dictionary<string, object>; if (a == null) continue; int c = (int)GetNumber(a, "c", 0d); var lr = BeatSaberProceduralFactory.CreateArcLine(parent, c == 0 ? controller.leftNoteColor : controller.rightNoteColor, "Chain b" + GetNumber(a, "b", 0d).ToString("0.###")); lr.widthMultiplier = 0.075f; lr.gameObject.SetActive(false); controller.chains.Add(new BeatSaberVivifyPreviewController.PathVisualBinding { line = lr, beat = GetNumber(a, "b", 0d), tailBeat = GetNumber(a, "tb", GetNumber(a, "b", 0d)), x = (int)GetNumber(a, "x", 0d), y = (int)GetNumber(a, "y", 0d), tailX = (int)GetNumber(a, "tx", 0d), tailY = (int)GetNumber(a, "ty", 0d), color = c, slices = Math.Max(2, (int)GetNumber(a, "sc", 4d)) }); }
    }

    private static void BuildSabers(List<object> events, Transform saberRoot, Transform leftHand, Transform rightHand, BeatSaberVivifyPreviewController controller, List<string> missing)
    {
        controller.sabers.leftMotionPivot = leftHand; controller.sabers.rightMotionPivot = rightHand;
        controller.sabers.leftHand = leftHand.parent; controller.sabers.rightHand = rightHand.parent;
        controller.sabers.leftBasePosition = leftHand.parent != null ? leftHand.parent.localPosition : Vector3.zero; controller.sabers.rightBasePosition = rightHand.parent != null ? rightHand.parent.localPosition : Vector3.zero;
        controller.sabers.leftBaseEuler = leftHand.parent != null ? leftHand.parent.localEulerAngles : Vector3.zero; controller.sabers.rightBaseEuler = rightHand.parent != null ? rightHand.parent.localEulerAngles : Vector3.zero;
        controller.sabers.leftFallback = BeatSaberProceduralFactory.CreateSaber(leftHand, controller.leftNoteColor, "Left Saber [Fallback]");
        controller.sabers.rightFallback = BeatSaberProceduralFactory.CreateSaber(rightHand, controller.rightNoteColor, "Right Saber [Fallback]");
        Dictionary<string, object> saber = null;
        if (events != null) for (int i = 0; i < events.Count; i++) { var e = events[i] as Dictionary<string, object>; if (e == null || GetString(e, "t") != "AssignObjectPrefab") continue; var d = GetDict(e, "d"); var s = GetDict(d, "saber"); if (s != null) { saber = s; break; } }
        if (saber == null) return;
        string asset = GetString(saber, "asset"); string trailAsset = GetString(saber, "trailAsset"); controller.sabers.assetPath = asset; controller.sabers.trailAssetPath = trailAsset;
        var prefab = BeatSaberAssetResolver.Load<GameObject>(asset, "Prefab");
        if (prefab == null) { missing.Add("Saber prefab: " + asset); return; }
        GameObject l = PrefabUtility.InstantiatePrefab(prefab, leftHand) as GameObject; if (l == null) l = UnityEngine.Object.Instantiate(prefab, leftHand); l.name = "Left Custom Saber";
        GameObject r = PrefabUtility.InstantiatePrefab(prefab, rightHand) as GameObject; if (r == null) r = UnityEngine.Object.Instantiate(prefab, rightHand); r.name = "Right Custom Saber";
        RepairSaberForPreview(l, controller.leftNoteColor); RepairSaberForPreview(r, controller.rightNoteColor);
        if (!HasRenderableGeometry(l)) { UnityEngine.Object.DestroyImmediate(l); l = null; }
        if (!HasRenderableGeometry(r)) { UnityEngine.Object.DestroyImmediate(r); r = null; }
        controller.sabers.leftCustom = l; controller.sabers.rightCustom = r;
        if (l != null) { ApplyColorNow(l, controller.leftNoteColor); l.SetActive(controller.previewCustomSabers); }
        if (r != null) { ApplyColorNow(r, controller.rightNoteColor); r.SetActive(controller.previewCustomSabers); }
        Material trail = !string.IsNullOrEmpty(trailAsset) ? BeatSaberAssetResolver.Load<Material>(trailAsset, "Material") : null;
        if (trail != null) { if (l != null) AddTrail(l, trail, controller.leftNoteColor, (float)GetNumber(saber, "trailDuration", 0.4d)); if (r != null) AddTrail(r, trail, controller.rightNoteColor, (float)GetNumber(saber, "trailDuration", 0.4d)); }
        else if (!string.IsNullOrEmpty(trailAsset)) missing.Add("Saber trail material: " + trailAsset);
        controller.sabers.leftFallback.SetActive(l == null);
        controller.sabers.rightFallback.SetActive(r == null);
    }

    private static void AddTrail(GameObject root, Material source, Color color, float duration)
    {
        GameObject go = new GameObject("Preview Saber Trail");
        go.transform.SetParent(root.transform, false);
        var tr = go.AddComponent<TrailRenderer>();
        tr.time = Mathf.Max(0.03f, duration);
        tr.startWidth = 0.07f;
        tr.endWidth = 0.005f;
        tr.minVertexDistance = 0.02f;
        tr.autodestruct = false;
        Material trailMaterial = NeedsPreviewAdapter(source) ? CreatePreviewAdapterMaterial(source, color) : new Material(source);
        if (trailMaterial != null) trailMaterial.hideFlags = HideFlags.DontSave;
        tr.sharedMaterial = trailMaterial;
        if (tr.sharedMaterial != null && tr.sharedMaterial.HasProperty("_Color")) tr.sharedMaterial.SetColor("_Color", color);
        tr.startColor = color;
        tr.endColor = new Color(color.r, color.g, color.b, 0f);
    }

    private static void ApplyColorNow(GameObject go, Color color)
    {
        if (go == null) return; var rr = go.GetComponentsInChildren<Renderer>(true); var block = new MaterialPropertyBlock();
        for (int i = 0; i < rr.Length; i++) { if (rr[i] == null) continue; rr[i].GetPropertyBlock(block); block.SetColor("_Color", color); block.SetColor("_BaseColor", color); rr[i].SetPropertyBlock(block); block.Clear(); }
        // Keep the map's particle alpha, gradients, emission and counts.
        // A note tint is not a replacement for an authored flare envelope.
    }

    // Mirrors Vivify's important assignment rule for preview: the prefab that applies to a
    // note is the latest matching AssignObjectPrefab at or before that note's beat. This is
    // deliberately time-aware; using only the final assignment makes earlier notes disappear
    // or use the wrong model on maps that swap note prefabs mid-song.
    private static NotePrefabAssignment ResolveNotePrefabAssignment(List<object> events, string[] noteTracks, double noteBeat)
    {
        if (events == null) return null;
        NotePrefabAssignment best = null;
        double bestBeat = double.NegativeInfinity;
        for (int i = 0; i < events.Count; i++)
        {
            var ev = events[i] as Dictionary<string, object>;
            if (ev == null || GetString(ev, "t") != "AssignObjectPrefab") continue;
            double eventBeat = GetNumber(ev, "b", 0d);
            if (eventBeat > noteBeat + 0.000001 || eventBeat < bestBeat) continue;
            var d = GetDict(ev, "d"); var color = GetDict(d, "colorNotes"); if (color == null) continue;
            string[] assignedTracks = TrackArray(GetValue(color, "track"));
            bool match = assignedTracks.Length == 0;
            for (int a = 0; !match && a < assignedTracks.Length; a++)
                for (int n = 0; n < noteTracks.Length; n++)
                    if (string.Equals(assignedTracks[a], noteTracks[n], StringComparison.Ordinal)) { match = true; break; }
            if (!match) continue;
            string loadMode = GetString(d, "loadMode");
            // Additive assignments (for example YOU's note-mounted trailer cameras)
            // supplement the note. They must never replace its actual visual prefab.
            if (string.Equals(loadMode, "Additive", StringComparison.OrdinalIgnoreCase)) continue;
            bestBeat = eventBeat;
            best = new NotePrefabAssignment {
                asset = GetString(color, "asset"),
                debrisAsset = GetString(color, "debrisAsset"),
                anyDirectionAsset = GetString(color, "anyDirectionAsset"),
                loadMode = loadMode, beat = eventBeat
            };
        }
        return best;
    }

    private static Dictionary<string, NotePrefabAssignment> BuildNotePrefabMap(List<object> events)
    {
        var map = new Dictionary<string, NotePrefabAssignment>(StringComparer.Ordinal); if (events == null) return map;
        for (int i = 0; i < events.Count; i++)
        {
            var ev = events[i] as Dictionary<string, object>; if (ev == null || GetString(ev, "t") != "AssignObjectPrefab") continue;
            var d = GetDict(ev, "d"); var color = GetDict(d, "colorNotes"); if (color == null) continue;
            var assignment = new NotePrefabAssignment { asset = GetString(color, "asset"), debrisAsset = GetString(color, "debrisAsset"), anyDirectionAsset = GetString(color, "anyDirectionAsset"), loadMode = GetString(d, "loadMode"), beat = GetNumber(ev, "b", 0d) };
            string[] tracks = TrackArray(GetValue(color, "track"));
            if (tracks.Length == 0) map["*"] = assignment;
            else for (int t = 0; t < tracks.Length; t++) if (!string.IsNullOrEmpty(tracks[t])) map[tracks[t]] = assignment;
        }
        return map;
    }

    private static bool HasRenderableGeometry(GameObject go)
    {
        if (go == null) return false;
        var rr = go.GetComponentsInChildren<Renderer>(true);
        for (int i = 0; i < rr.Length; i++)
        {
            Renderer renderer = rr[i];
            if (renderer == null || !renderer.enabled) continue;
            var meshRenderer = renderer as MeshRenderer;
            if (meshRenderer != null)
            {
                var filter = meshRenderer.GetComponent<MeshFilter>();
                if (filter == null || filter.sharedMesh == null) continue;
            }
            var skinned = renderer as SkinnedMeshRenderer;
            if (skinned != null && skinned.sharedMesh == null) continue;
            Material[] materials = renderer.sharedMaterials;
            for (int m = 0; m < materials.Length; m++)
            {
                Material material = materials[m];
                if (material != null && material.shader != null && material.shader.isSupported &&
                    !string.Equals(material.shader.name, "Hidden/InternalErrorShader", StringComparison.Ordinal)) return true;
            }
        }
        return false;
    }

    private static void RepairSaberForPreview(GameObject go, Color color)
    {
        if (go == null) return;
        var renderers = go.GetComponentsInChildren<Renderer>(true);
        for (int r = 0; r < renderers.Length; r++)
        {
            Renderer renderer = renderers[r];
            if (renderer == null) continue;
            string objectName = renderer.gameObject.name ?? string.Empty;
            // SaberGuide is an authoring helper volume, not the visible blade. In a
            // normal Unity camera it becomes a large magenta/purple occluder.
            if (objectName.IndexOf("guide", StringComparison.OrdinalIgnoreCase) >= 0)
            {
                renderer.enabled = false;
                continue;
            }
            Material[] materials = renderer.sharedMaterials;
            bool changed = false;
            for (int m = 0; m < materials.Length; m++)
            {
                Material source = materials[m];
                Material safe = BeatSaberPreviewMaterials.Create(source);
                if (safe == null) continue;
                materials[m] = safe;
                changed = true;
            }
            if (changed) renderer.sharedMaterials = materials;
        }
        ApplyColorNow(go, color);
    }

    private static void StyleGeneratedNoteFromAssignment(GameObject note, string prefabAsset, Color bodyColor, Color markerColor)
    {
        if (note == null || string.IsNullOrEmpty(prefabAsset)) return;
        string path = NormalizePath(prefabAsset);
        string bodyPath = null, markerPath = null;
        if (path.IndexOf("reflectivenote", StringComparison.OrdinalIgnoreCase) >= 0)
        {
            bodyPath = "assets/materials/note/reflectivenote.mat";
            markerPath = "assets/materials/note/reflectivearrow.mat";
        }
        else if (path.IndexOf("glassnote", StringComparison.OrdinalIgnoreCase) >= 0)
        {
            bodyPath = "assets/materials/note/glassnote.mat";
            markerPath = "assets/materials/note/glassarrow.mat";
        }
        else if (path.IndexOf("portalnote", StringComparison.OrdinalIgnoreCase) >= 0)
        {
            bodyPath = "assets/materials/note/dropnote.mat";
            markerPath = "assets/materials/note/dropnotearrow.mat";
        }
        Material bodySource = !string.IsNullOrEmpty(bodyPath) ? BeatSaberAssetResolver.Load<Material>(bodyPath, "Material") : null;
        Material markerSource = !string.IsNullOrEmpty(markerPath) ? BeatSaberAssetResolver.Load<Material>(markerPath, "Material") : null;
        var renderers = note.GetComponentsInChildren<Renderer>(true);
        for (int i = 0; i < renderers.Length; i++)
        {
            bool marker = string.Equals(renderers[i].gameObject.name, "Arrow", StringComparison.OrdinalIgnoreCase) ||
                          string.Equals(renderers[i].gameObject.name, "Dot", StringComparison.OrdinalIgnoreCase);
            Material source = marker ? markerSource : bodySource;
            if (source == null) continue;
            Material safe = CreatePreviewAdapterMaterial(source, marker ? markerColor : bodyColor);
            if (safe != null) renderers[i].sharedMaterial = safe;
        }
    }

    private static void RepairUnsupportedMaterials(GameObject go, Color previewColor)
    {
        if (go == null) return;
        var rr = go.GetComponentsInChildren<Renderer>(true);
        for (int r = 0; r < rr.Length; r++)
        {
            Renderer renderer = rr[r]; if (renderer == null) continue;
            Material[] mats = renderer.sharedMaterials; bool changed = false;
            for (int m = 0; m < mats.Length; m++)
            {
                Material source = mats[m];
                if (!NeedsPreviewAdapter(source)) continue;
                Material replacement = CreatePreviewAdapterMaterial(source, previewColor);
                if (replacement == null) continue;
                mats[m] = replacement;
                changed = true;
            }
            if (changed) renderer.sharedMaterials = mats;
        }
    }

    private static void RepairAllGeneratedMaterialsForCurrentPlatform(Transform root, Color fallbackColor)
    {
        if (root == null) return;
        var renderers = root.GetComponentsInChildren<Renderer>(true);
        for (int i = 0; i < renderers.Length; i++)
        {
            var r = renderers[i]; if (r == null) continue;
            var mats = r.sharedMaterials; bool changed = false;
            for (int m = 0; m < mats.Length; m++)
            {
                Material src = mats[m]; if (!NeedsPreviewAdapter(src)) continue;
                Material safe = CreatePreviewAdapterMaterial(src, fallbackColor);
                if (safe != null) { mats[m] = safe; changed = true; }
            }
            if (changed) r.sharedMaterials = mats;
        }
    }

    private static bool NeedsPreviewAdapter(Material source)
    {
        return BeatSaberPreviewMaterials.NeedsAdapter(source);
    }

    private static Material CreatePreviewAdapterMaterial(Material source, Color previewColor)
    {
        return BeatSaberPreviewMaterials.Create(source, true, previewColor);
    }


    private static void ApplyTrackParenting(List<object> events, Dictionary<string, Transform> trackObjects)
    {
        if (events == null) return;
        for (int i = 0; i < events.Count; i++) { var ev = events[i] as Dictionary<string, object>; if (ev == null || GetString(ev, "t") != "AssignTrackParent") continue; var d = GetDict(ev, "d"); string parentName = GetString(d, "parentTrack"); Transform parent; if (!trackObjects.TryGetValue(parentName, out parent)) continue; var children = GetList(d, "childrenTracks"); if (children == null) continue; for (int c = 0; c < children.Count; c++) { Transform child; string childName = Convert.ToString(children[c]); if (trackObjects.TryGetValue(childName, out child) && child != parent) child.SetParent(parent, false); } }
    }

    private static HashSet<string> CollectTrackNames(Dictionary<string, object> rootJson, List<object> events)
    {
        var set = new HashSet<string>(StringComparer.Ordinal);
        if (events != null) for (int i = 0; i < events.Count; i++) { var ev = events[i] as Dictionary<string, object>; if (ev == null) continue; var d = GetDict(ev, "d"); AddTracks(set, d == null ? null : GetValue(d, "track")); if (GetString(ev, "t") == "AssignTrackParent") { AddTracks(set, d == null ? null : GetValue(d, "parentTrack")); var ch = d == null ? null : GetList(d, "childrenTracks"); if (ch != null) for (int c = 0; c < ch.Count; c++) set.Add(Convert.ToString(ch[c])); } if (GetString(ev, "t") == "AssignObjectPrefab" && d != null) { string[] keys = { "colorNotes", "burstSliders", "burstSliderElements", "chainHeads", "chainLinks" }; for (int k = 0; k < keys.Length; k++) { var x = GetDict(d, keys[k]); if (x != null) AddTracks(set, GetValue(x, "track")); } } }
        var notes = BeatSaberMapCompat.GetColorNotes(rootJson); if (notes != null) for (int i = 0; i < notes.Count; i++) { var n = notes[i] as Dictionary<string, object>; var cd = GetDict(n, "customData"); AddTracks(set, cd == null ? null : GetValue(cd, "track")); }
        return set;
    }

    private static void AddTracks(HashSet<string> set, object v) { var list = v as List<object>; if (list != null) { for (int i = 0; i < list.Count; i++) { string s = Convert.ToString(list[i]); if (!string.IsNullOrEmpty(s)) set.Add(s); } return; } string one = v == null ? "" : Convert.ToString(v); if (!string.IsNullOrEmpty(one)) set.Add(one); }
    private static Transform EnsureTrack(string name, Transform root, Dictionary<string, Transform> dict, BeatSaberVivifyPreviewController controller) { Transform t; if (dict.TryGetValue(name, out t)) return t; t = NewChild(root, SafeName(name)); dict[name] = t; controller.tracks.Add(new BeatSaberVivifyPreviewController.TrackBinding { track = name, transform = t }); return t; }

    private static void RemoveAutoSongTracks(TimelineAsset timeline)
    {
        if (timeline == null) return;
        var remove = new List<TrackAsset>();
        foreach (var tr in timeline.GetOutputTracks())
            if (tr is AudioTrack && tr.name == "♪ Song") remove.Add(tr);
        for (int i = 0; i < remove.Count; i++) timeline.DeleteTrack(remove[i]);
        if (remove.Count > 0) { EditorUtility.SetDirty(timeline); AssetDatabase.SaveAssets(); }
    }

    private static void EnsureSongTrack(TimelineAsset timeline, AudioClip song, BeatSaberMapData mapData)
    {
        if (timeline == null || song == null) return; foreach (var tr in timeline.GetOutputTracks()) if (tr is AudioTrack && tr.name == "♪ Song") return;
        try { var audio = timeline.CreateTrack<AudioTrack>(null, "♪ Song"); var clip = audio.CreateClip<AudioPlayableAsset>(); var a = clip.asset as AudioPlayableAsset; if (a != null) a.clip = song; clip.start = 0; clip.duration = Math.Min(song.length, mapData.maxBeat * 60.0 / Math.Max(0.001, mapData.bpm)); EditorUtility.SetDirty(timeline); AssetDatabase.SaveAssets(); } catch (Exception ex) { Debug.LogWarning("Could not add song track: " + ex.Message); }
    }

    public static BeatSaberMapData FindMapData(TimelineAsset timeline)
    {
        if (timeline == null) return null;
        string path = AssetDatabase.GetAssetPath(timeline);
        var all = AssetDatabase.LoadAllAssetsAtPath(path);
        for (int i = 0; i < all.Length; i++) { var d = all[i] as BeatSaberMapData; if (d != null) return d; }
        // Some Unity 2019 AssetDatabase states expose custom sub-assets only through
        // representations until the Timeline has been opened once in the editor.
        var representations = AssetDatabase.LoadAllAssetRepresentationsAtPath(path);
        for (int i = 0; i < representations.Length; i++) { var d = representations[i] as BeatSaberMapData; if (d != null) return d; }
        return null;
    }
    private static Transform NewChild(Transform parent, string name) { var go = new GameObject(name); go.transform.SetParent(parent, false); return go.transform; }
    private static string SafeName(string s) { return string.IsNullOrEmpty(s) ? "Track" : s.Replace('/', '∕').Replace('\\', '⧵'); }
    private static string FirstTrack(object v) { var l = v as List<object>; return l != null && l.Count > 0 ? Convert.ToString(l[0]) : (v == null ? "" : Convert.ToString(v)); }
    private static string[] TrackArray(object v) { var l = v as List<object>; if (l != null) { var a = new string[l.Count]; for (int i = 0; i < l.Count; i++) a[i] = Convert.ToString(l[i]); return a; } return v == null ? new string[0] : new[] { Convert.ToString(v) }; }
    private static string NormalizePath(string value) { return string.IsNullOrEmpty(value) ? string.Empty : value.Replace('\\', '/').Trim().ToLowerInvariant(); }
    private static object GetValue(Dictionary<string, object> d, string k) { object v; return d != null && d.TryGetValue(k, out v) ? v : null; }
    private static Dictionary<string, object> GetDict(Dictionary<string, object> d, string k) { return GetValue(d, k) as Dictionary<string, object>; }
    private static List<object> GetList(Dictionary<string, object> d, string k) { return GetValue(d, k) as List<object>; }
    private static string GetString(Dictionary<string, object> d, string k) { object v = GetValue(d, k); return v == null ? "" : Convert.ToString(v); }
    private static double GetNumber(Dictionary<string, object> d, string k, double f) { object v = GetValue(d, k); try { return v == null ? f : Convert.ToDouble(v, CultureInfo.InvariantCulture); } catch { return f; } }
}
}
#endif
