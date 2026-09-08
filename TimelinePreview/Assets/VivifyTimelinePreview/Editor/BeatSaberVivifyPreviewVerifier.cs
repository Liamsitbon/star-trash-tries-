#if UNITY_EDITOR
using System;
using System.IO;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Timeline;

namespace VivifyTimelinePreview
{
public static class BeatSaberVivifyPreviewVerifier
{
    // Headless-friendly editor smoke test used by maintainers after changing the preview.
    // It creates a clean transient scene, evaluates the real YOU Timeline at beat 15,
    // renders the preview camera and exits with a failing code if notes or sabers vanish.
    [MenuItem("Tools/Run YOU Preview Smoke Test")]
    public static void RunYouProjectSmokeTest()
    {
        const string timelinePath = "Assets/HardStandard_Timeline_V5.playable";
        const string songPath = "Assets/song.mp3";
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        TimelineAsset timeline = AssetDatabase.LoadAssetAtPath<TimelineAsset>(timelinePath);
        AudioClip song = AssetDatabase.LoadAssetAtPath<AudioClip>(songPath);
        if (timeline == null)
        {
            Debug.LogError("VIVIFY_PREVIEW_SMOKE_FAIL missing Timeline: " + timelinePath);
            EditorApplication.Exit(2);
            return;
        }
        BeatSaberMapData embedded = BeatSaberVivifyPreviewBuilder.FindMapData(timeline);
        Debug.Log("VIVIFY_PREVIEW_SMOKE_INPUT timeline=" + timeline.name +
                  " mapData=" + (embedded != null ? embedded.name : "null") +
                  " jsonChars=" + (embedded != null && embedded.json != null ? embedded.json.Length : 0));

        BeatSaberVivifyPreviewController controller = BeatSaberVivifyPreviewBuilder.CreateRig(timeline, song, true, true, true, false);
        if (controller == null || controller.mapData == null)
        {
            Debug.LogError("VIVIFY_PREVIEW_SMOKE_FAIL rig creation failed");
            EditorApplication.Exit(3);
            return;
        }

        // Immediately before the b15.625 right-note hit: the right saber should be in
        // the strike path while multiple notes and the intro environment remain visible.
        float seconds = 15.61f * 60f / Mathf.Max(0.001f, controller.mapData.bpm);
        controller.EvaluateAtSeconds(seconds);

        int activeNotes = 0;
        for (int i = 0; i < controller.notes.Count; i++)
        {
            var note = controller.notes[i];
            if (note == null) continue;
            bool visible = (note.instance != null && note.instance.activeInHierarchy) ||
                           (note.standardVisual != null && note.standardVisual.activeInHierarchy);
            if (visible) activeNotes++;
        }
        bool leftSaber = controller.sabers != null &&
            ((controller.sabers.leftCustom != null && controller.sabers.leftCustom.activeInHierarchy) ||
             (controller.sabers.leftFallback != null && controller.sabers.leftFallback.activeInHierarchy));
        bool rightSaber = controller.sabers != null &&
            ((controller.sabers.rightCustom != null && controller.sabers.rightCustom.activeInHierarchy) ||
             (controller.sabers.rightFallback != null && controller.sabers.rightFallback.activeInHierarchy));

        string output = Path.GetFullPath(Path.Combine(Application.dataPath, "../../VivifyTimelinePreview_V5_9_SmokeTest.png"));
        bool rendered = RenderCamera(controller.previewCamera, output);
        bool passed = controller.notes.Count == 160 && activeNotes > 0 && leftSaber && rightSaber && rendered;
        string result = "notes=" + controller.notes.Count + " activeNotes=" + activeNotes +
                        " leftSaber=" + leftSaber + " rightSaber=" + rightSaber +
                        " rendered=" + rendered + " screenshot=" + output;
        if (passed) Debug.Log("VIVIFY_PREVIEW_SMOKE_PASS " + result);
        else Debug.LogError("VIVIFY_PREVIEW_SMOKE_FAIL " + result);
        EditorApplication.Exit(passed ? 0 : 4);
    }

    private static bool RenderCamera(Camera camera, string output)
    {
        if (camera == null) return false;
        RenderTexture target = new RenderTexture(1280, 720, 24, RenderTextureFormat.ARGB32);
        Texture2D image = new Texture2D(1280, 720, TextureFormat.RGB24, false);
        RenderTexture previous = RenderTexture.active;
        RenderTexture oldTarget = camera.targetTexture;
        try
        {
            camera.targetTexture = target;
            RenderTexture.active = target;
            camera.Render();
            image.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0);
            image.Apply(false, false);
            File.WriteAllBytes(output, image.EncodeToPNG());
            return File.Exists(output) && new FileInfo(output).Length > 1024;
        }
        catch (Exception ex)
        {
            Debug.LogError("VIVIFY_PREVIEW_SMOKE render failed: " + ex);
            return false;
        }
        finally
        {
            camera.targetTexture = oldTarget;
            RenderTexture.active = previous;
            UnityEngine.Object.DestroyImmediate(image);
            UnityEngine.Object.DestroyImmediate(target);
        }
    }
}
}
#endif
