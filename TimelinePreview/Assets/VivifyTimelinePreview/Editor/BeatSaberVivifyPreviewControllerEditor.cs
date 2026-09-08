#if UNITY_EDITOR
using UnityEditor;
using UnityEngine;
using UnityEngine.Playables;
using UnityEngine.Timeline;

namespace VivifyTimelinePreview
{
[CustomEditor(typeof(BeatSaberVivifyPreviewController))]
public class BeatSaberVivifyPreviewControllerEditor : Editor
{
    private float jumpBeat;

    public override void OnInspectorGUI()
    {
        DrawDefaultInspector();
        var c = (BeatSaberVivifyPreviewController)target;
        if (c.director == null) c.director = c.GetComponent<PlayableDirector>();
        if (c.timeline != null && c.director != null && c.director.playableAsset != c.timeline)
        {
            Undo.RecordObject(c.director, "Change Beat Saber Timeline"); c.director.playableAsset = c.timeline;
            var embedded = BeatSaberVivifyPreviewBuilder.FindMapData(c.timeline); if (embedded != null) c.mapData = embedded;
            foreach (var tr in c.timeline.GetOutputTracks()) if (tr is BeatSaberPreviewDriverTrack) c.director.SetGenericBinding(tr, c);
            c.ForceReparse(); EditorUtility.SetDirty(c);
        }
        if (c.audioSource == null) c.audioSource = c.GetComponent<AudioSource>();
        if (c.audioSource != null && c.song != null && c.audioSource.clip != c.song) c.audioSource.clip = c.song;

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("Timeline Preview Controls V6.0", EditorStyles.boldLabel);
        EditorGUILayout.LabelField("Current", "beat " + c.CurrentBeat.ToString("0.###") + " / " + c.CurrentSeconds.ToString("0.00") + "s");
        using (new EditorGUILayout.HorizontalScope())
        {
            if (GUILayout.Button("◀ 0")) SetTime(c, 0);
            if (GUILayout.Button("Evaluate")) { if (c.director != null) c.director.Evaluate(); else c.EvaluateAtSeconds(c.CurrentSeconds); SceneView.RepaintAll(); }
            if (GUILayout.Button("Reparse DAT")) c.ForceReparse();
        }
        jumpBeat = EditorGUILayout.FloatField("Jump to beat", jumpBeat);
        if (GUILayout.Button("Jump + evaluate")) { float seconds = Mathf.Max(0f, jumpBeat * 60f / Mathf.Max(0.001f, c.EffectiveBpm) - c.timelineOffsetSeconds); SetTime(c, seconds); }

        EditorGUILayout.Space();
        using (new EditorGUILayout.HorizontalScope())
        {
            if (GUILayout.Button("Guarantee notes + sabers"))
            {
                Undo.RecordObject(c, "Guarantee gameplay visuals");
                c.forceStandardNoteFallback = true;
                c.alwaysShowStandardSabers = true;
                c.previewCustomSabers = false;
                c.previewNotes = true; c.previewSabers = true;
                c.ForceReparse();
            }
            if (GUILayout.Button("Prefer map's Vivify visuals"))
            {
                Undo.RecordObject(c, "Prefer custom gameplay visuals");
                c.forceStandardNoteFallback = false;
                c.alwaysShowStandardSabers = false;
                c.previewCustomSabers = true;
                c.preferCustomNotePrefabs = true;
                c.ForceReparse();
            }
        }
        using (new EditorGUI.DisabledScope(Application.isPlaying))
        {
            if (GUILayout.Button("Rebuild preview with original map assets"))
            {
                // Rebuild generated instances only. Previously serialized
                // generic flare adapters cannot recover their original shader
                // after a domain reload; the source prefab remains untouched.
                var rebuilt = BeatSaberVivifyPreviewBuilder.CreateRig(c.timeline, c.song, true, true, true, false);
                if (rebuilt != null)
                {
                    rebuilt.transform.SetPositionAndRotation(c.transform.position, c.transform.rotation);
                    rebuilt.previewCustomSabers = true;
                    rebuilt.preferCustomNotePrefabs = true;
                    rebuilt.alwaysShowStandardSabers = false;
                    rebuilt.forceVisibleNoteProxy = false;
                    Undo.DestroyObjectImmediate(c.gameObject);
                    Selection.activeGameObject = rebuilt.gameObject;
                    GUIUtility.ExitGUI();
                }
            }
        }
        using (new EditorGUILayout.HorizontalScope())
        {
            if (GUILayout.Button("Spectator ON")) { Undo.RecordObject(c, "Spectator on"); c.useSpectatorCamera = true; c.EvaluateAtSeconds(c.CurrentSeconds); }
            if (GUILayout.Button("Player camera")) { Undo.RecordObject(c, "Player camera"); c.useSpectatorCamera = false; c.EvaluateAtSeconds(c.CurrentSeconds); }
        }

        var locked = c.GetComponent<BeatSaberDeterministicPlayback>();
        EditorGUILayout.Space();
        EditorGUILayout.LabelField("Pre-bake / x1.0 playback", EditorStyles.boldLabel);
        EditorGUILayout.LabelField("Pre-baked", c.PreBakeComplete ? ("yes · " + c.PreBakedMaterialPasses + " material passes") : "not yet");
        if (locked != null) EditorGUILayout.HelpBox(locked.PlaybackStatus, MessageType.None);
        using (new EditorGUILayout.HorizontalScope())
        {
            if (GUILayout.Button("Pre-bake now"))
            {
                if (locked != null) locked.PreBakeAgain();
                else c.PreBakePreview(false);
                SceneView.RepaintAll();
            }
            GUI.enabled = Application.isPlaying && locked != null;
            if (GUILayout.Button("Restart x1.0")) locked.RestartLockedPlayback();
            if (GUILayout.Button("Pause")) locked.PauseLockedPlayback();
            if (GUILayout.Button("Resume")) locked.ResumeLockedPlayback();
            GUI.enabled = true;
        }

        EditorGUILayout.HelpBox("V6.0 uses a fixed grip and a song-time blade arc through the authored hit pose. Standard cubes are 0.5 x 0.5 x 0.5. Auto Cut Notes is preview autoplay, not Beat Saber's scoring or collision engine. Turn it off to watch notes pass through without automatic cuts.", MessageType.Info);
        EditorGUILayout.HelpBox("The song is loaded before playback. Visuals follow actual AudioSource samples; preview shader time, prefab Animators and particles use that clock too. Pause, seeking and a late audio start do not run effects ahead. Original map shaders/materials are not modified; editor copies and gameplay preview adapters are used.", MessageType.None);
        EditorGUILayout.HelpBox("Spectator mode: enable Use Spectator Camera. In Play Mode hold right mouse to look, WASD to move, Q/E down/up, Shift for faster movement. The spectator can also receive the Vivify Blit stack.", MessageType.None);
        EditorGUILayout.HelpBox("V4 maps store lighting in a separate Lightshow DAT. Rebuild the Timeline with that file selected if you want fallback environment lights to react. Exact official environment models are not bundled; the fallback stage is procedural so the package does not copy Beat Saber assets.", MessageType.None);
    }

    private static void SetTime(BeatSaberVivifyPreviewController c, double time)
    {
        var locked = c.GetComponent<BeatSaberDeterministicPlayback>();
        if (Application.isPlaying && locked != null) locked.SeekLocked(time);
        else if (c.director != null) { c.director.time = time; c.director.Evaluate(); }
        else c.EvaluateAtSeconds((float)time);
        SceneView.RepaintAll();
    }
}
}
#endif
