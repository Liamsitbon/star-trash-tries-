using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Playables;
using UnityEngine.Timeline;

namespace VivifyTimelinePreview
{

[ExecuteInEditMode]
public class BeatSaberVivifyPreviewController : MonoBehaviour
{
    [Serializable]
    public class PreviewObjectBinding
    {
        public string id;
        public string track;
        public string assetPath;
        public GameObject instance;
        public double instantiateBeat;
        public double destroyBeat = 999999.0;
    }

    [Serializable]
    public class TrackBinding
    {
        public string track;
        public Transform transform;
        [HideInInspector] public Vector3 initialPosition;
        [HideInInspector] public Vector3 initialEuler;
        [HideInInspector] public Vector3 initialScale = Vector3.one;
    }

    [Serializable]
    public class MaterialSlot
    {
        public Renderer renderer;
        public int slot;
        [NonSerialized] public Material previewMaterialBeforeRuntime;
    }

    [Serializable]
    public class MaterialBinding
    {
        public string assetPath;
        public Material sourceMaterial;
        public List<MaterialSlot> slots = new List<MaterialSlot>();
        [NonSerialized] public Material runtimeMaterial;
        [NonSerialized] public Material previewBaseline;
    }

    [Serializable]
    public class NoteBinding
    {
        public GameObject instance;
        public GameObject standardVisual;
        public GameObject standardDebris;
        public GameObject debugProxy;
        public GameObject debrisInstance;
        public bool sourcePrefabHasVisibleRenderer;
        public bool debrisPrefabHasVisibleRenderer;
        public string assetPath;
        public string debrisAssetPath;
        public string loadMode;
        public double beat;
        public int x;
        public int y;
        public int color;
        public int direction;
        [HideInInspector] public int mapIndex = -1;
        public float njs = 10f;
        public float jumpOffset;
        public string[] tracks;
        [HideInInspector] public Vector3 prefabScale = Vector3.one;
        [HideInInspector] public Vector3 prefabEuler;
        [HideInInspector] public Vector3 debrisScale = Vector3.one;
        [HideInInspector] public Vector3 debrisEuler;
        [HideInInspector] public Vector3 lastEvaluatedPosition;
        [HideInInspector] public Vector3 lastEvaluatedEuler;
        [HideInInspector] public Vector3 lastEvaluatedScale = Vector3.one;
    }

    [Serializable]
    public class BombBinding
    {
        public GameObject instance;
        public double beat;
        public int x;
        public int y;
        public float njs = 10f;
        public float jumpOffset;
    }

    [Serializable]
    public class ObstacleBinding
    {
        public GameObject instance;
        public double beat;
        public double duration;
        public int x;
        public int y;
        public int width = 1;
        public int height = 5;
        public float njs = 10f;
        public float jumpOffset;
    }

    [Serializable]
    public class PathVisualBinding
    {
        public LineRenderer line;
        public double beat;
        public double tailBeat;
        public int x;
        public int y;
        public int tailX;
        public int tailY;
        public int color;
        public int slices;
    }

    [Serializable]
    public class SaberBinding
    {
        public GameObject leftCustom;
        public GameObject rightCustom;
        public GameObject leftFallback;
        public GameObject rightFallback;
        public string assetPath;
        public string trailAssetPath;
        public Transform leftHand;
        public Transform rightHand;
        [HideInInspector] public Vector3 leftBasePosition;
        [HideInInspector] public Vector3 rightBasePosition;
        [HideInInspector] public Vector3 leftBaseEuler;
        [HideInInspector] public Vector3 rightBaseEuler;
        public Transform leftMotionPivot;
        public Transform rightMotionPivot;
    }

    public enum EnvironmentPreviewMode { Auto, GenericBeatSaber, Off }

    [Header("Timeline")]
    public TimelineAsset timeline;
    public BeatSaberMapData mapData;
    public PlayableDirector director;
    [Tooltip("Added to the Timeline time before converting time to beat. Leave 0 for normal maps.")]
    public float timelineOffsetSeconds;
    [Tooltip("0 = use BPM stored in the generated Timeline map data.")]
    public float bpmOverride;

    [Header("Optional song")]
    public AudioClip song;
    public AudioSource audioSource;
    public bool syncAudioSourceWhenPlaying = false;

    [Header("Preview features")]
    public bool previewPrefabs = true;
    public bool previewNotes = true;
    public bool previewBombs = true;
    public bool previewObstacles = true;
    public bool previewArcsAndChains = true;
    public bool previewSabers = true;
    [Tooltip("Use the map's AssignObjectPrefab saber when one exists. V5.9 prefers genuinely renderable Vivify/source saber geometry and falls back when it is missing or incomplete.")]
    public bool previewCustomSabers = true;
    [Tooltip("Prefer AssignObjectPrefab source-note prefabs when they resolve. Standard Beat Saber-style notes are always generated as a fallback so vanilla maps never become invisible.")]
    public bool preferCustomNotePrefabs = true;
    [Tooltip("Force the generated standard note even when a custom source note is available. OFF by default in V5.9 because overlapping a custom note and a fallback note in the same place causes visible flicker/z-fighting.")]
    public bool forceStandardNoteFallback = false;
    [Tooltip("Old V3 debug cube. Usually leave this off; V5 has a proper standard-note fallback now.")]
    public bool forceVisibleNoteProxy = false;
    [Tooltip("Preview the map's assigned debris prefab briefly after a note reaches its hit beat. This is deterministic while scrubbing and does not require game runtime components.")]
    public bool previewNoteDebris = true;
    [Tooltip("How long the assigned debris prefab remains visible, measured in beats.")]
    public float noteDebrisDurationBeats = 0.45f;
    [Tooltip("Apply Beat Saber note colors to custom note/debris renderers through MaterialPropertyBlock. c=0 uses Left Note Color; c=1 uses Right Note Color.")]
    public bool applyNoteColors = true;
    public Color leftNoteColor = new Color(0.85f, 0.05f, 0.05f, 1f);
    public Color rightNoteColor = new Color(0.05f, 0.25f, 1.0f, 1f);
    [Tooltip("Also show generated sabers on top of custom sabers. OFF by default: overlapping sabers can flicker and look like they are vibrating.")]
    public bool alwaysShowStandardSabers = false;
    [Tooltip("Rotate each blade around a fixed grip, approaching from the opposite side of the cut direction as the note spawns. Preview autoplay, not game scoring.")]
    public bool autoSwingSabers = true;
    public bool autoCutNotes = true;
    public float saberSwingWindowBeats = 0.22f;
    public float saberApproachWindowBeats = 0.48f;
    [Tooltip("Aim strength. The grip stays fixed even at 1; only the blade rotates.")]
    [Range(0f, 1f)] public float saberReach = 1f;
    [HideInInspector] public float saberMotionSmoothing = 24f; // Legacy serialized field; curves now use song time only.
    [Tooltip("Note hit plane in front of the player, so a fixed-grip blade can cut forward instead of aiming behind the controller.")]
    public float noteHitPlaneZ = 0.8f;
    public bool previewMaterials = true;
    public bool previewPostProcessing = true;
    public bool previewRenderingSettings = true;
    public bool previewCameraSettings = true;

    [Header("Pre-bake")]
    [Tooltip("V5.9 builds all gameplay/custom objects up front. This status becomes true after the controller has parsed/cached events, cloned runtime materials and pre-baked only the material passes used by this rig.")]
    [SerializeField] private bool preBakeComplete;
    [SerializeField] private int preBakedMaterialPasses;
    public bool PreBakeComplete { get { return preBakeComplete; } }
    public int PreBakedMaterialPasses { get { return preBakedMaterialPasses; } }

    [Header("Vanilla / fallback environment")]
    public EnvironmentPreviewMode environmentMode = EnvironmentPreviewMode.Auto;
    [Tooltip("Procedural fallback environment used for vanilla/non-Vivify maps. It uses no Beat Saber game assets.")]
    public BeatSaberGenericEnvironment genericEnvironment;

    [Header("Camera mode")]
    public bool useSpectatorCamera = false;
    [Tooltip("Apply the map's active Blit/post-processing stack to the free spectator camera too.")]
    public bool spectatorReceivesPostProcessing = true;
    [Tooltip("Map camera driven by player/head tracks and Vivify camera/post-processing events.")]
    public Camera previewCamera;
    [Tooltip("Independent camera for watching the scene from far away while the Timeline plays.")]
    public Camera spectatorCamera;
    public BeatSaberSpectatorCamera spectatorControls;

    [Header("Generated hierarchy")]
    public Transform generatedRoot;
    public Transform trackRoot;
    public Transform prefabRoot;
    public Transform noteRoot;
    public Transform gameplayRoot;
    public Transform saberRoot;
    public Transform spectatorRoot;
    public BeatSaberBlitPreview blitPreview;
    public BeatSaberBlitPreview spectatorBlitPreview;

    public List<PreviewObjectBinding> objects = new List<PreviewObjectBinding>();
    public List<TrackBinding> tracks = new List<TrackBinding>();
    public List<MaterialBinding> materials = new List<MaterialBinding>();
    public List<NoteBinding> notes = new List<NoteBinding>();
    public List<BombBinding> bombs = new List<BombBinding>();
    public List<ObstacleBinding> obstacles = new List<ObstacleBinding>();
    public List<PathVisualBinding> arcs = new List<PathVisualBinding>();
    public List<PathVisualBinding> chains = new List<PathVisualBinding>();
    public SaberBinding sabers = new SaberBinding();

    [Header("Debug")]
    [SerializeField] private float currentBeat;
    [SerializeField] private float currentSeconds;
    [TextArea(2, 6)] public string lastWarning;

    private Dictionary<string, object> root;
    private Dictionary<string, object> lightshowRoot;
    private List<object> customEvents;
    private List<object> basicEvents;
    private List<object> genericLightEvents;
    private List<object> colorBoostEvents;
    private bool detectedVivify;
    // Parsed event caches keep Timeline scrubbing responsive on maps with hundreds of custom events.
    private readonly List<Dictionary<string, object>> animateTrackEvents = new List<Dictionary<string, object>>();
    private readonly List<Dictionary<string, object>> materialPropertyEvents = new List<Dictionary<string, object>>();
    private readonly Dictionary<string, List<Dictionary<string, object>>> animateEventsByTrack = new Dictionary<string, List<Dictionary<string, object>>>(StringComparer.Ordinal);
    private readonly Dictionary<string, List<Dictionary<string, object>>> materialEventsByAsset = new Dictionary<string, List<Dictionary<string, object>>>(StringComparer.OrdinalIgnoreCase);
    private readonly List<Dictionary<string, object>> renderingSettingEvents = new List<Dictionary<string, object>>();
    private readonly List<Dictionary<string, object>> cameraPropertyEvents = new List<Dictionary<string, object>>();
    private readonly List<Dictionary<string, object>> blitEvents = new List<Dictionary<string, object>>();
    private readonly Dictionary<string, List<Dictionary<string, object>>> pathEventsByTrack = new Dictionary<string, List<Dictionary<string, object>>>(StringComparer.Ordinal);
    private readonly Dictionary<string, TrackBinding> trackLookup = new Dictionary<string, TrackBinding>(StringComparer.Ordinal);
    private readonly Dictionary<string, MaterialBinding> materialLookup = new Dictionary<string, MaterialBinding>(StringComparer.OrdinalIgnoreCase);
    private bool prepared;
    private readonly Dictionary<GameObject, BeatSaberPreviewObjectClock> objectClocks = new Dictionary<GameObject, BeatSaberPreviewObjectClock>();
    private float previousShaderSeconds;
    private bool baselineFog;
    private bool baselineRealtimeReflectionProbes;
    private DepthTextureMode baselineDepthTextureMode;
    private DepthTextureMode baselineSpectatorDepthTextureMode;

    public float CurrentBeat { get { return currentBeat; } }
    public float CurrentSeconds { get { return currentSeconds; } }
    public float EffectiveBpm { get { return bpmOverride > 0.001f ? bpmOverride : (mapData != null ? mapData.bpm : 70f); } }

    private void OnEnable()
    {
        Prepare();
        if (director == null) director = GetComponent<PlayableDirector>();
        if (audioSource == null) audioSource = GetComponent<AudioSource>();
    }

    private void OnDisable()
    {
        RestoreMaterials();
        RestoreObjectClocks();
        prepared = false;
        preBakeComplete = false;
        preBakedMaterialPasses = 0;
    }

    public void ForceReparse()
    {
        // Restore/destroy previous preview material clones before rebuilding caches.
        RestoreMaterials();
        RestoreObjectClocks();
        prepared = false;
        Prepare();
        EvaluateAtSeconds(currentSeconds);
    }

    public void PreBakePreview(bool warmAllLoadedShaders)
    {
        Prepare();
        preBakedMaterialPasses = 0;

        var seen = new HashSet<Material>();
        Action<GameObject> warmObject = delegate(GameObject go)
        {
            if (go == null) return;
            var rr = go.GetComponentsInChildren<Renderer>(true);
            for (int r = 0; r < rr.Length; r++)
            {
                if (rr[r] == null) continue;
                var mats = rr[r].sharedMaterials;
                for (int m = 0; m < mats.Length; m++)
                {
                    Material mat = mats[m];
                    if (mat == null || mat.shader == null || !mat.shader.isSupported || !seen.Add(mat)) continue;
                    try
                    {
                        int passCount = Mathf.Max(0, mat.passCount);
                        for (int p = 0; p < passCount; p++)
                        {
                            if (mat.SetPass(p)) preBakedMaterialPasses++;
                        }
                    }
                    catch { }
                }
            }
        };

        for (int i = 0; i < objects.Count; i++) if (objects[i] != null) warmObject(objects[i].instance);
        for (int i = 0; i < notes.Count; i++)
        {
            NoteBinding n = notes[i];
            if (n == null) continue;
            warmObject(n.instance); warmObject(n.standardVisual); warmObject(n.debrisInstance); warmObject(n.standardDebris);
        }
        for (int i = 0; i < bombs.Count; i++) if (bombs[i] != null) warmObject(bombs[i].instance);
        for (int i = 0; i < obstacles.Count; i++) if (obstacles[i] != null) warmObject(obstacles[i].instance);
        if (sabers != null)
        {
            warmObject(sabers.leftFallback); warmObject(sabers.rightFallback);
            warmObject(sabers.leftCustom); warmObject(sabers.rightCustom);
        }

        if (warmAllLoadedShaders)
        {
            try { Shader.WarmupAllShaders(); } catch { }
        }

        EvaluateAtSeconds(0f);
        preBakeComplete = true;
    }

    public void EvaluateAtSeconds(float seconds)
    {
        Prepare();
        if (mapData == null || root == null) return;
        currentSeconds = Mathf.Max(0f, seconds);
        float bpm = Mathf.Max(0.001f, EffectiveBpm);
        currentBeat = (currentSeconds + timelineOffsetSeconds) * bpm / 60f;

        ResetTrackTransforms();
        if (previewPrefabs) EvaluatePrefabVisibility(currentBeat);
        else SetAllPrefabsActive(false);

        EvaluateAnimateTracks(currentBeat);
        if (previewMaterials) EvaluateMaterialProperties(currentBeat);
        else ResetRuntimeMaterials();
        if (previewRenderingSettings) EvaluateRenderingSettings(currentBeat);
        if (previewCameraSettings) EvaluateCameraSettings(currentBeat);
        if (previewPostProcessing) EvaluateBlits(currentBeat);
        else
        {
            if (blitPreview != null) blitPreview.activeBlits.Clear();
            if (spectatorBlitPreview != null) spectatorBlitPreview.activeBlits.Clear();
        }
        if (previewNotes) EvaluateNotes(currentBeat);
        else SetAllNotesActive(false);
        if (previewBombs) EvaluateBombs(currentBeat); else SetBombsActive(false);
        if (previewObstacles) EvaluateObstacles(currentBeat); else SetObstaclesActive(false);
        if (previewArcsAndChains) EvaluatePathVisuals(currentBeat); else SetPathVisualsActive(false);
        if (previewSabers) EvaluateSabers(currentBeat); else SetSabersActive(false);
        EvaluateGenericEnvironment(currentBeat);
        UpdateCameraMode();
        EvaluateObjectClocks(currentBeat);
        SetPreviewShaderTime(currentSeconds);

        if (syncAudioSourceWhenPlaying && Application.isPlaying && audioSource != null && song != null && director != null)
        {
            if (audioSource.clip != song) audioSource.clip = song;
            float t = Mathf.Clamp(currentSeconds, 0f, Mathf.Max(0f, song.length - 0.01f));
            if (Mathf.Abs(audioSource.time - t) > 0.08f) audioSource.time = t;
            if (director.state == PlayState.Playing && !audioSource.isPlaying) audioSource.Play();
            if (director.state != PlayState.Playing && audioSource.isPlaying) audioSource.Pause();
        }
    }

    private void Prepare()
    {
        if (prepared) return;
        prepared = true;
        lastWarning = "";
        trackLookup.Clear();
        materialLookup.Clear();

        for (int i = 0; i < tracks.Count; i++)
        {
            var t = tracks[i];
            if (t == null || t.transform == null || string.IsNullOrEmpty(t.track)) continue;
            t.initialPosition = t.transform.localPosition;
            t.initialEuler = t.transform.localEulerAngles;
            t.initialScale = t.transform.localScale;
            trackLookup[t.track] = t;
        }

        if (previewCamera != null) baselineDepthTextureMode = previewCamera.depthTextureMode;
        if (spectatorCamera != null) baselineSpectatorDepthTextureMode = spectatorCamera.depthTextureMode;
        baselineFog = RenderSettings.fog;
        baselineRealtimeReflectionProbes = QualitySettings.realtimeReflectionProbes;

        if (mapData != null && !string.IsNullOrEmpty(mapData.json))
        {
            try
            {
                root = BeatSaberMiniJson.Deserialize(mapData.json) as Dictionary<string, object>;
                lightshowRoot = !string.IsNullOrEmpty(mapData.lightshowJson) ? BeatSaberMiniJson.Deserialize(mapData.lightshowJson) as Dictionary<string, object> : null;
                customEvents = BeatSaberMapCompat.GetCustomEvents(root);
                basicEvents = BeatSaberMapCompat.GetBasicEvents(root, lightshowRoot);
                genericLightEvents = BeatSaberMapCompat.GetFallbackLightEvents(root, lightshowRoot);
                colorBoostEvents = BeatSaberMapCompat.GetColorBoostEvents(root, lightshowRoot);
                SortByBeat(customEvents);
                SortByBeat(basicEvents);
                SortByBeat(genericLightEvents);
                SortByBeat(colorBoostEvents);
                detectedVivify = DetectVivify(customEvents);
            }
            catch (Exception ex)
            {
                root = null;
                lightshowRoot = null;
                customEvents = null;
                basicEvents = null;
                genericLightEvents = null;
                colorBoostEvents = null;
                detectedVivify = false;
                lastWarning = "JSON parse failed: " + ex.Message;
            }
        }

        BuildEventCaches();
        PrepareRuntimeMaterials();
        PrepareObjectClocks();
        if (audioSource != null && song != null) audioSource.clip = song;
    }

    private void BuildEventCaches()
    {
        animateTrackEvents.Clear();
        materialPropertyEvents.Clear();
        renderingSettingEvents.Clear();
        cameraPropertyEvents.Clear();
        blitEvents.Clear();
        pathEventsByTrack.Clear();
        animateEventsByTrack.Clear();
        materialEventsByAsset.Clear();
        if (customEvents == null) return;

        for (int i = 0; i < customEvents.Count; i++)
        {
            var ev = customEvents[i] as Dictionary<string, object>;
            if (ev == null) continue;
            string type = GetString(ev, "t");
            if (type == "AnimateTrack")
            {
                animateTrackEvents.Add(ev);
                var data = GetDict(ev, "d");
                string track = GetTrackName(GetValue(data, "track"));
                if (!string.IsNullOrEmpty(track))
                {
                    List<Dictionary<string, object>> list;
                    if (!animateEventsByTrack.TryGetValue(track, out list)) { list = new List<Dictionary<string, object>>(); animateEventsByTrack[track] = list; }
                    list.Add(ev);
                }
            }
            else if (type == "SetMaterialProperty")
            {
                materialPropertyEvents.Add(ev);
                var data = GetDict(ev, "d");
                string asset = NormalizePath(GetString(data, "asset"));
                if (!string.IsNullOrEmpty(asset))
                {
                    List<Dictionary<string, object>> list;
                    if (!materialEventsByAsset.TryGetValue(asset, out list)) { list = new List<Dictionary<string, object>>(); materialEventsByAsset[asset] = list; }
                    list.Add(ev);
                }
            }
            else if (type == "SetRenderingSettings") renderingSettingEvents.Add(ev);
            else if (type == "SetCameraProperty") cameraPropertyEvents.Add(ev);
            else if (type == "Blit") blitEvents.Add(ev);
            else if (type == "AssignPathAnimation")
            {
                var data = GetDict(ev, "d");
                string[] names = GetTrackNames(GetValue(data, "track"));
                for (int n = 0; n < names.Length; n++)
                {
                    if (string.IsNullOrEmpty(names[n])) continue;
                    List<Dictionary<string, object>> list;
                    if (!pathEventsByTrack.TryGetValue(names[n], out list))
                    {
                        list = new List<Dictionary<string, object>>();
                        pathEventsByTrack[names[n]] = list;
                    }
                    list.Add(ev);
                }
            }
        }

        Comparison<Dictionary<string, object>> byBeat = (a, b) => GetNumber(a, "b", 0d).CompareTo(GetNumber(b, "b", 0d));
        animateTrackEvents.Sort(byBeat);
        materialPropertyEvents.Sort(byBeat);
        renderingSettingEvents.Sort(byBeat);
        cameraPropertyEvents.Sort(byBeat);
        blitEvents.Sort(byBeat);
        foreach (var pair in pathEventsByTrack) pair.Value.Sort(byBeat);
        foreach (var pair in animateEventsByTrack) pair.Value.Sort(byBeat);
        foreach (var pair in materialEventsByAsset) pair.Value.Sort(byBeat);
    }

    private void PrepareRuntimeMaterials()
    {
        // Include materials without DAT property events too: their shader time also
        // belongs to the song, and repaired renderer slots must survive Play Mode.
        var bySource = new Dictionary<Material, MaterialBinding>();
        var byName = new Dictionary<string, MaterialBinding>(StringComparer.Ordinal);
        var boundSlots = new HashSet<string>(StringComparer.Ordinal);
        foreach (var binding in materials)
        {
            if (binding == null || binding.sourceMaterial == null) continue;
            bySource[binding.sourceMaterial] = binding;
            byName[binding.sourceMaterial.name] = binding;
            foreach (var slot in binding.slots)
                if (slot != null && slot.renderer != null) boundSlots.Add(slot.renderer.GetInstanceID() + ":" + slot.slot);
        }
        if (generatedRoot != null) foreach (var renderer in generatedRoot.GetComponentsInChildren<Renderer>(true))
        {
            Material[] slots = renderer.sharedMaterials;
            for (int s = 0; s < slots.Length; s++)
            {
                if (boundSlots.Contains(renderer.GetInstanceID() + ":" + s)) continue;
                Material source = BeatSaberPreviewMaterials.SourceOf(slots[s]);
                if (source == null)
                {
#if UNITY_EDITOR
                    var original = UnityEditor.PrefabUtility.GetCorrespondingObjectFromSource(renderer);
                    if (original != null && s < original.sharedMaterials.Length) source = original.sharedMaterials[s];
#endif
                    if (source == null) source = BeatSaberPreviewMaterials.Create(null);
                    if (source == null) continue;
                }
                MaterialBinding binding;
                if (!bySource.TryGetValue(source, out binding))
                {
                    // Recover bindings from older rigs whose builder adapted a material
                    // before recording its original Vivify asset-to-renderer association.
                    int suffix = source.name.IndexOf(" [", StringComparison.Ordinal);
                    if (suffix < 0 || !byName.TryGetValue(source.name.Substring(0, suffix), out binding))
                    {
                        binding = new MaterialBinding { sourceMaterial = source };
                        materials.Add(binding);
                    }
                    bySource[source] = binding;
                }
                binding.slots.Add(new MaterialSlot { renderer = renderer, slot = s });
            }
        }
        foreach (var binding in materials)
        {
            if (binding == null || binding.sourceMaterial == null) continue;
            binding.runtimeMaterial = BeatSaberPreviewMaterials.Create(binding.sourceMaterial);
            if (binding.runtimeMaterial == null) continue;
            binding.previewBaseline = new Material(binding.runtimeMaterial) { hideFlags = HideFlags.DontSave };
            if (!string.IsNullOrEmpty(binding.assetPath)) materialLookup[NormalizePath(binding.assetPath)] = binding;
            foreach (var slot in binding.slots)
            {
                if (slot == null || slot.renderer == null) continue;
                var mats = slot.renderer.sharedMaterials;
                if (slot.slot < 0 || slot.slot >= mats.Length) continue;
                slot.previewMaterialBeforeRuntime = mats[slot.slot];
                mats[slot.slot] = binding.runtimeMaterial;
                slot.renderer.sharedMaterials = mats;
            }
        }
    }

    private void RestoreMaterials()
    {
        foreach (var binding in materials)
        {
            if (binding == null) continue;
            foreach (var slot in binding.slots)
            {
                if (slot == null || slot.renderer == null) continue;
                var mats = slot.renderer.sharedMaterials;
                if (slot.slot < 0 || slot.slot >= mats.Length || mats[slot.slot] != binding.runtimeMaterial) continue;
                mats[slot.slot] = slot.previewMaterialBeforeRuntime != null ? slot.previewMaterialBeforeRuntime : binding.sourceMaterial;
                slot.renderer.sharedMaterials = mats;
                slot.previewMaterialBeforeRuntime = null;
            }
            DisposePreviewMaterial(binding.runtimeMaterial);
            DisposePreviewMaterial(binding.previewBaseline);
            binding.runtimeMaterial = null;
            binding.previewBaseline = null;
        }
    }

    private static void DisposePreviewMaterial(Material material)
    {
        if (material == null) return;
        BeatSaberPreviewMaterials.Forget(material);
        if (Application.isPlaying) Destroy(material); else DestroyImmediate(material);
    }

    private void ResetRuntimeMaterials()
    {
        foreach (var binding in materials) ResetMaterial(binding);
    }

    private static void ResetMaterial(MaterialBinding binding)
    {
        if (binding == null || binding.runtimeMaterial == null || binding.previewBaseline == null) return;
        binding.runtimeMaterial.CopyPropertiesFromMaterial(binding.previewBaseline);
        binding.runtimeMaterial.shaderKeywords = binding.previewBaseline.shaderKeywords;
    }

    private void ResetTrackTransforms()
    {
        for (int i = 0; i < tracks.Count; i++)
        {
            var t = tracks[i];
            if (t == null || t.transform == null) continue;
            t.transform.localPosition = t.initialPosition;
            t.transform.localEulerAngles = t.initialEuler;
            t.transform.localScale = t.initialScale;
        }
    }

    private void EvaluatePrefabVisibility(double beat)
    {
        for (int i = 0; i < objects.Count; i++)
        {
            var o = objects[i];
            if (o == null || o.instance == null) continue;
            bool active = beat >= o.instantiateBeat && beat < o.destroyBeat;
            if (o.instance.activeSelf != active) o.instance.SetActive(active);
        }
    }

    private void SetAllPrefabsActive(bool active)
    {
        for (int i = 0; i < objects.Count; i++) if (objects[i] != null && objects[i].instance != null) objects[i].instance.SetActive(active);
    }

    private void EvaluateAnimateTracks(double beat)
    {
        foreach (var pair in animateEventsByTrack)
        {
            TrackBinding binding;
            if (!trackLookup.TryGetValue(pair.Key, out binding) || binding == null || binding.transform == null) continue;
            List<Dictionary<string, object>> list = pair.Value;
            int idx = FindLastEventIndex(list, beat);
            if (idx < 0) continue;

            bool havePosition = false, haveRotation = false, haveScale = false;
            for (int i = idx; i >= 0 && !(havePosition && haveRotation && haveScale); i--)
            {
                var ev = list[i]; var data = GetDict(ev, "d"); if (data == null) continue;
                double startBeat = GetNumber(ev, "b", 0d);
                double duration = Math.Max(0d, GetNumber(data, "duration", 0d));
                double normalized = duration <= 0.000001 ? 1d : Clamp01((beat - startBeat) / duration);
                object value;
                if (!havePosition && (data.TryGetValue("position", out value) || data.TryGetValue("localPosition", out value)))
                {
                    binding.transform.localPosition = EvalVector3(value, normalized, binding.initialPosition);
                    havePosition = true;
                }
                if (!haveRotation && (data.TryGetValue("rotation", out value) || data.TryGetValue("localRotation", out value)))
                {
                    binding.transform.localEulerAngles = EvalVector3(value, normalized, binding.initialEuler);
                    haveRotation = true;
                }
                if (!haveScale && data.TryGetValue("scale", out value))
                {
                    binding.transform.localScale = EvalVector3(value, normalized, binding.initialScale);
                    haveScale = true;
                }
            }
        }
    }

    private void EvaluateMaterialProperties(double beat)
    {
        // V5.9 does not replay every historical SetMaterialProperty event every frame.
        // For each material/property we evaluate only the latest write at the current beat.
        // This is equivalent for persistent material state and removes a major source of
        // Editor stalls on maps with hundreds of material events.
        for (int b = 0; b < materials.Count; b++)
        {
            MaterialBinding binding = materials[b];
            if (binding == null || binding.sourceMaterial == null || binding.runtimeMaterial == null) continue;
            ResetMaterial(binding);

            List<Dictionary<string, object>> list;
            string assetKey = NormalizePath(binding.assetPath);
            if (!materialEventsByAsset.TryGetValue(assetKey, out list) || list == null || list.Count == 0) continue;
            int idx = FindLastEventIndex(list, beat);
            if (idx < 0) continue;

            var seen = new HashSet<string>(StringComparer.Ordinal);
            for (int i = idx; i >= 0; i--)
            {
                var ev = list[i]; var data = GetDict(ev, "d"); if (data == null) continue;
                double startBeat = GetNumber(ev, "b", 0d);
                double duration = Math.Max(0d, GetNumber(data, "duration", 0d));
                double normalized = duration <= 0.000001 ? 1d : Clamp01((beat - startBeat) / duration);
                var props = GetList(data, "properties"); if (props == null) continue;
                for (int p = 0; p < props.Count; p++)
                {
                    var prop = props[p] as Dictionary<string, object>; if (prop == null) continue;
                    string id = GetString(prop, "id"); if (string.IsNullOrEmpty(id) || seen.Contains(id)) continue;
                    seen.Add(id);
                    string type = GetString(prop, "type"); object raw = GetValue(prop, "value");
                    if (type == "Float") binding.runtimeMaterial.SetFloat(id, (float)EvalScalar(raw, normalized, binding.runtimeMaterial.HasProperty(id) ? binding.runtimeMaterial.GetFloat(id) : 0f));
                    else if (type == "Vector") binding.runtimeMaterial.SetVector(id, EvalVector4(raw, normalized, binding.runtimeMaterial.HasProperty(id) ? binding.runtimeMaterial.GetVector(id) : Vector4.zero));
                    else if (type == "Keyword")
                    {
                        bool enabled = GetBoolValue(raw, false);
                        if (enabled) binding.runtimeMaterial.EnableKeyword(id); else binding.runtimeMaterial.DisableKeyword(id);
                    }
                }
            }
        }
    }

    private static int FindLastEventIndex(List<Dictionary<string, object>> list, double beat)
    {
        if (list == null || list.Count == 0) return -1;
        int lo = 0, hi = list.Count - 1, answer = -1;
        while (lo <= hi)
        {
            int mid = lo + ((hi - lo) >> 1);
            double b = GetNumber(list[mid], "b", 0d);
            if (b <= beat) { answer = mid; lo = mid + 1; } else hi = mid - 1;
        }
        return answer;
    }

    private void EvaluateRenderingSettings(double beat)
    {
        RenderSettings.fog = baselineFog;
        QualitySettings.realtimeReflectionProbes = baselineRealtimeReflectionProbes;
        for (int i = 0; i < renderingSettingEvents.Count; i++)
        {
            var ev = renderingSettingEvents[i];
            if (GetNumber(ev, "b", 0d) > beat) break;
            var data = GetDict(ev, "d");
            var render = GetDict(data, "renderSettings");
            var quality = GetDict(data, "qualitySettings");
            if (render != null && render.ContainsKey("fog")) RenderSettings.fog = GetBoolValue(render["fog"], RenderSettings.fog);
            if (quality != null && quality.ContainsKey("realtimeReflectionProbes")) QualitySettings.realtimeReflectionProbes = GetBoolValue(quality["realtimeReflectionProbes"], QualitySettings.realtimeReflectionProbes);
        }
    }

    private void EvaluateCameraSettings(double beat)
    {
        if (previewCamera != null) previewCamera.depthTextureMode = baselineDepthTextureMode;
        if (spectatorCamera != null) spectatorCamera.depthTextureMode = baselineSpectatorDepthTextureMode;
        for (int i = 0; i < cameraPropertyEvents.Count; i++)
        {
            var ev = cameraPropertyEvents[i];
            if (GetNumber(ev, "b", 0d) > beat) break;
            var data = GetDict(ev, "d");
            var props = GetDict(data, "properties");
            if (props == null) continue;
            object modes;
            if (props.TryGetValue("depthTextureMode", out modes))
            {
                DepthTextureMode mode = DepthTextureMode.None;
                var list = modes as List<object>;
                if (list != null)
                {
                    for (int m = 0; m < list.Count; m++)
                    {
                        string value = Convert.ToString(list[m]);
                        if (value == "Depth") mode |= DepthTextureMode.Depth;
                        else if (value == "DepthNormals") mode |= DepthTextureMode.DepthNormals;
                        else if (value == "MotionVectors") mode |= DepthTextureMode.MotionVectors;
                    }
                }
                if (previewCamera != null) previewCamera.depthTextureMode = mode;
                // The free camera is not map-driven, but matching depth-texture requirements
                // lets source post-processing/depth shaders render when studying from afar.
                if (spectatorCamera != null) spectatorCamera.depthTextureMode = mode;
            }
        }
    }

    private void EvaluateBlits(double beat)
    {
        if (blitPreview != null) blitPreview.activeBlits.Clear();
        if (spectatorBlitPreview != null) spectatorBlitPreview.activeBlits.Clear();
        for (int i = 0; i < blitEvents.Count; i++)
        {
            var ev = blitEvents[i];
            double startBeat = GetNumber(ev, "b", 0d);
            if (startBeat > beat) break;
            var data = GetDict(ev, "d");
            double duration = Math.Max(0.00001, GetNumber(data, "duration", 0d));
            if (beat < startBeat || beat > startBeat + duration) continue;
            string asset = NormalizePath(GetString(data, "asset"));
            MaterialBinding binding;
            if (!materialLookup.TryGetValue(asset, out binding) || binding.runtimeMaterial == null) continue;
            int pass = (int)GetNumber(data, "pass", -1d);

            if (blitPreview != null)
            {
                var active = new BeatSaberBlitPreview.ActiveBlit();
                active.material = binding.runtimeMaterial;
                active.pass = pass;
                blitPreview.activeBlits.Add(active);
            }
            if (spectatorBlitPreview != null && spectatorReceivesPostProcessing)
            {
                var spectatorActive = new BeatSaberBlitPreview.ActiveBlit();
                spectatorActive.material = binding.runtimeMaterial;
                spectatorActive.pass = pass;
                spectatorBlitPreview.activeBlits.Add(spectatorActive);
            }
        }
    }

    private void EvaluateNotes(double beat)
    {
        for (int i = 0; i < notes.Count; i++)
        {
            var n = notes[i];
            if (n == null) continue;

            double halfJump = ComputeHalfJumpBeats(Math.Max(0.1f, n.njs), n.jumpOffset, EffectiveBpm);
            double total = halfJump * 2.0;
            double spawnBeat = n.beat - halfJump;
            double normalized = total <= 0.000001 ? 1d : (beat - spawnBeat) / total;
            bool noteActive = normalized >= 0d && (autoCutNotes ? beat < n.beat : normalized <= 1d);
            bool debrisActive = autoCutNotes && previewNoteDebris && n.debrisInstance != null && beat >= n.beat && beat <= n.beat + Math.Max(0.01f, noteDebrisDurationBeats);
            bool standardDebrisActive = autoCutNotes && previewNoteDebris && n.standardDebris != null && (!preferCustomNotePrefabs || !n.debrisPrefabHasVisibleRenderer) && beat >= n.beat && beat <= n.beat + Math.Max(0.01f, noteDebrisDurationBeats);

            bool customAvailable = n.instance != null && !string.IsNullOrEmpty(n.assetPath) && n.sourcePrefabHasVisibleRenderer;
            bool showCustom = noteActive && preferCustomNotePrefabs && customAvailable;
            // Never stack the fallback directly on top of a resolved custom prefab unless
            // the user explicitly disables custom visuals. This removes the "epilepsy"/
            // z-fighting seen when two notes occupy the exact same transform.
            bool showStandard = noteActive && (!showCustom || (!preferCustomNotePrefabs && forceStandardNoteFallback));
            if (n.instance != null && n.instance.activeSelf != showCustom) n.instance.SetActive(showCustom);
            if (n.standardVisual != null && n.standardVisual.activeSelf != showStandard) n.standardVisual.SetActive(showStandard);
            if (n.debugProxy != null)
            {
                bool proxyActive = noteActive && forceVisibleNoteProxy;
                if (n.debugProxy.activeSelf != proxyActive) n.debugProxy.SetActive(proxyActive);
            }
            if (n.debrisInstance != null && n.debrisInstance.activeSelf != debrisActive) n.debrisInstance.SetActive(debrisActive);
            if (n.standardDebris != null && n.standardDebris.activeSelf != standardDebrisActive) n.standardDebris.SetActive(standardDebrisActive);

            double visualT = Clamp01(normalized);
            double secondsPerBeat = 60.0 / Math.Max(0.001, EffectiveBpm);
            float jumpDistance = n.njs * (float)(secondsPerBeat * total);
            float z = (float)((0.5 - visualT) * jumpDistance);
            Vector3 pos = new Vector3((n.x - 1.5f) * 0.6f, 0.6f + n.y * 0.6f, z);
            Vector3 euler = n.prefabEuler;
            Vector3 scale = n.prefabScale == Vector3.zero ? Vector3.one : n.prefabScale;

            ApplyNoteAnimations(n, beat, visualT, ref pos, ref euler, ref scale);
            pos.z += noteHitPlaneZ;
            n.lastEvaluatedPosition = pos;
            n.lastEvaluatedEuler = euler;
            n.lastEvaluatedScale = scale;

            if (n.instance != null)
            {
                n.instance.transform.localPosition = pos;
                n.instance.transform.localEulerAngles = euler;
                n.instance.transform.localScale = scale;
            }
            if (n.standardVisual != null)
            {
                n.standardVisual.transform.localPosition = pos;
                n.standardVisual.transform.localEulerAngles = euler;
                n.standardVisual.transform.localScale = Vector3.one;
            }
            if (n.debugProxy != null)
            {
                n.debugProxy.transform.localPosition = pos;
                n.debugProxy.transform.localEulerAngles = euler;
                n.debugProxy.transform.localScale = Vector3.one * 0.5f;
            }

            Color noteColor = ResolveNoteColor(n);
            float dissolve = ResolveNoteDissolve(n, visualT);
            if (applyNoteColors || dissolve < 0.9999f)
            {
                Color oppositeColor = n.color == 0 ? rightNoteColor : leftNoteColor;
                if (n.instance != null) ApplyCustomNoteMaterialProperties(n.instance, noteColor, oppositeColor, dissolve);
                if (n.standardVisual != null) ApplyStandardNoteMaterialProperties(n.standardVisual, noteColor, oppositeColor);
                if (n.debugProxy != null) ApplyNoteMaterialProperties(n.debugProxy, noteColor, dissolve);
            }

            if (debrisActive && n.debrisInstance != null)
            {
                n.debrisInstance.transform.localPosition = n.lastEvaluatedPosition;
                n.debrisInstance.transform.localEulerAngles = n.lastEvaluatedEuler;
                n.debrisInstance.transform.localScale = n.debrisScale == Vector3.zero ? n.lastEvaluatedScale : n.debrisScale;
                float progress = (float)Clamp01((beat - n.beat) / Math.Max(0.01f, noteDebrisDurationBeats));
                ApplyNoteMaterialProperties(n.debrisInstance, noteColor, 1f - progress * 0.15f);
                PreviewParticleSystems(n.debrisInstance, progress * noteDebrisDurationBeats * 60f / Mathf.Max(0.001f, EffectiveBpm));
            }
            if (standardDebrisActive && n.standardDebris != null)
            {
                float progress = (float)Clamp01((beat - n.beat) / Math.Max(0.01f, noteDebrisDurationBeats));
                n.standardDebris.transform.localPosition = n.lastEvaluatedPosition + Vector3.forward * progress * 0.15f;
                n.standardDebris.transform.localEulerAngles = n.lastEvaluatedEuler + new Vector3(0f, 0f, CutDirectionAngle(n.direction));
                n.standardDebris.transform.localScale = Vector3.one;
                if (n.standardDebris.transform.childCount >= 2)
                {
                    Vector3 split = new Vector3(0.22f + progress * 0.38f, 0.05f + progress * 0.18f, 0f);
                    Transform a = n.standardDebris.transform.GetChild(0); Transform b = n.standardDebris.transform.GetChild(1);
                    a.localPosition = -split; b.localPosition = split;
                    a.localEulerAngles = new Vector3(progress * 40f, progress * 22f, progress * 25f);
                    b.localEulerAngles = new Vector3(-progress * 36f, -progress * 20f, -progress * 28f);
                }
                ApplyNoteMaterialProperties(n.standardDebris, noteColor, 1f);
            }
        }
    }

    private Color ResolveNoteColor(NoteBinding note)
    {
        Color fallback = note != null && note.color == 1 ? rightNoteColor : leftNoteColor;
        if (note == null || root == null) return fallback;
        Dictionary<string, object> noteJson = FindNoteJson(note);
        var cd = GetDict(noteJson, "customData");
        var custom = cd == null ? null : GetList(cd, "color");
        if (custom != null && custom.Count >= 3)
        {
            float r = (float)ToDouble(custom[0]);
            float g = (float)ToDouble(custom[1]);
            float b = (float)ToDouble(custom[2]);
            float a = custom.Count > 3 ? (float)ToDouble(custom[3]) : 1f;
            return new Color(r, g, b, a);
        }
        return fallback;
    }

    private float ResolveNoteDissolve(NoteBinding note, double normalizedLife)
    {
        Dictionary<string, object> noteJson = FindNoteJson(note);
        if (noteJson == null) return 1f;
        var cd = GetDict(noteJson, "customData");
        var anim = GetDict(cd, "animation");
        object raw;
        if (anim != null && anim.TryGetValue("dissolve", out raw)) return Mathf.Clamp01((float)EvalScalar(raw, normalizedLife, 1d));
        return 1f;
    }

    private Dictionary<string, object> FindNoteJson(NoteBinding note)
    {
        if (note == null || root == null) return null;
        var colorNotes = BeatSaberMapCompat.GetColorNotes(root);
        if (colorNotes == null) return null;
        if (note.mapIndex >= 0 && note.mapIndex < colorNotes.Count)
        {
            var indexed = colorNotes[note.mapIndex] as Dictionary<string, object>;
            if (indexed != null) return indexed;
        }
        for (int i = 0; i < colorNotes.Count; i++)
        {
            var d = colorNotes[i] as Dictionary<string, object>;
            if (d != null && Math.Abs(GetNumber(d, "b", -999d) - note.beat) < 0.0005 &&
                (int)GetNumber(d, "x", -1) == note.x && (int)GetNumber(d, "y", -1) == note.y &&
                (int)GetNumber(d, "c", -1) == note.color)
                return d;
        }
        return null;
    }

    private static void ApplyStandardNoteMaterialProperties(GameObject go, Color color, Color markerColor)
    {
        if (go == null) return;
        var renderers = go.GetComponentsInChildren<Renderer>(true);
        var block = new MaterialPropertyBlock();
        for (int i = 0; i < renderers.Length; i++)
        {
            var r = renderers[i]; if (r == null) continue;
            bool marker = string.Equals(r.gameObject.name, "Arrow", StringComparison.OrdinalIgnoreCase) || string.Equals(r.gameObject.name, "Dot", StringComparison.OrdinalIgnoreCase);
            Color c = marker ? markerColor : color;
            r.GetPropertyBlock(block);
            block.SetColor(Shader.PropertyToID("_Color"), c);
            block.SetColor(Shader.PropertyToID("_BaseColor"), c);
            r.SetPropertyBlock(block); block.Clear();
        }
    }

    private static void ApplyCustomNoteMaterialProperties(GameObject go, Color bodyColor, Color markerColor, float cutout)
    {
        if (go == null) return;
        var renderers = go.GetComponentsInChildren<Renderer>(true);
        var block = new MaterialPropertyBlock();
        for (int i = 0; i < renderers.Length; i++)
        {
            Renderer renderer = renderers[i];
            if (renderer == null) continue;
            string name = renderer.gameObject.name ?? string.Empty;
            bool marker = name.IndexOf("arrow", StringComparison.OrdinalIgnoreCase) >= 0 ||
                          name.IndexOf("dot", StringComparison.OrdinalIgnoreCase) >= 0;
            Color color = marker ? markerColor : bodyColor;
            renderer.GetPropertyBlock(block);
            Material[] mats = renderer.sharedMaterials;
            bool hasColor = false, hasBaseColor = false, hasCutout = false;
            for (int m = 0; m < mats.Length; m++)
            {
                Material mat = mats[m]; if (mat == null) continue;
                hasColor |= mat.HasProperty(ColorId);
                hasBaseColor |= mat.HasProperty(BaseColorId);
                hasCutout |= mat.HasProperty(CutoutId);
            }
            if (hasColor) block.SetColor(ColorId, color);
            if (hasBaseColor) block.SetColor(BaseColorId, color);
            if (hasCutout) block.SetFloat(CutoutId, Mathf.Clamp01(cutout));
            renderer.SetPropertyBlock(block);
            block.Clear();
        }
    }

    private static readonly int ColorId = Shader.PropertyToID("_Color");
    private static readonly int BaseColorId = Shader.PropertyToID("_BaseColor");
    private static readonly int CutoutId = Shader.PropertyToID("_Cutout");

    private static void ApplyNoteMaterialProperties(GameObject go, Color color, float cutout)
    {
        if (go == null) return;
        var renderers = go.GetComponentsInChildren<Renderer>(true);
        var block = new MaterialPropertyBlock();
        for (int i = 0; i < renderers.Length; i++)
        {
            var r = renderers[i];
            if (r == null) continue;
            r.GetPropertyBlock(block);
            var mats = r.sharedMaterials;
            bool hasColor = false, hasBaseColor = false, hasCutout = false;
            for (int m = 0; m < mats.Length; m++)
            {
                var mat = mats[m];
                if (mat == null) continue;
                hasColor |= mat.HasProperty(ColorId);
                hasBaseColor |= mat.HasProperty(BaseColorId);
                hasCutout |= mat.HasProperty(CutoutId);
            }
            if (hasColor) block.SetColor(ColorId, color);
            if (hasBaseColor) block.SetColor(BaseColorId, color);
            if (hasCutout) block.SetFloat(CutoutId, Mathf.Clamp01(cutout));
            r.SetPropertyBlock(block);
            block.Clear();
        }
    }

    private void PrepareObjectClocks()
    {
        foreach (var item in objects) if (item != null) GetObjectClock(item.instance);
        foreach (var note in notes) if (note != null)
        {
            GetObjectClock(note.instance);
            GetObjectClock(note.debrisInstance);
        }
        if (sabers == null) return;
        GameObject[] visuals = { sabers.leftCustom, sabers.rightCustom, sabers.leftFallback, sabers.rightFallback };
        foreach (var visual in visuals)
        {
            if (visual == null) continue;
            var geometry = visual.GetComponent<BeatSaberPreviewSaberGeometry>();
            if (geometry == null) geometry = visual.AddComponent<BeatSaberPreviewSaberGeometry>();
            geometry.AlignGrip();
            GetObjectClock(visual);
        }
    }

    private BeatSaberPreviewObjectClock GetObjectClock(GameObject instance)
    {
        if (instance == null) return null;
        BeatSaberPreviewObjectClock clock;
        if (objectClocks.TryGetValue(instance, out clock)) return clock;
        bool hasClock = instance.GetComponentInChildren<ParticleSystem>(true) != null ||
            instance.GetComponentInChildren<Animator>(true) != null || instance.GetComponentInChildren<Animation>(true) != null ||
            instance.GetComponentInChildren<TrailRenderer>(true) != null;
        clock = hasClock ? new BeatSaberPreviewObjectClock(instance) : null;
        objectClocks[instance] = clock;
        return clock;
    }

    private void PreviewParticleSystems(GameObject go, float seconds)
    {
        var clock = GetObjectClock(go);
        if (clock != null) clock.Sample(seconds);
    }

    private void EvaluateObjectClocks(double beat)
    {
        float secondsPerBeat = 60f / Mathf.Max(0.001f, EffectiveBpm);
        foreach (var item in objects) if (item != null && item.instance != null && item.instance.activeInHierarchy)
            PreviewParticleSystems(item.instance, (float)(beat - item.instantiateBeat) * secondsPerBeat);
        foreach (var note in notes) if (note != null && note.instance != null && note.instance.activeInHierarchy)
        {
            double spawn = note.beat - ComputeHalfJumpBeats(Math.Max(0.1f, note.njs), note.jumpOffset, EffectiveBpm);
            PreviewParticleSystems(note.instance, (float)(beat - spawn) * secondsPerBeat);
        }
        if (sabers != null)
        {
            PreviewParticleSystems(sabers.leftCustom, currentSeconds);
            PreviewParticleSystems(sabers.rightCustom, currentSeconds);
        }
    }

    private void RestoreObjectClocks()
    {
        foreach (var clock in objectClocks.Values) if (clock != null) clock.Dispose();
        objectClocks.Clear();
    }

    private void SetPreviewShaderTime(float seconds)
    {
        float delta = Mathf.Max(0f, seconds - previousShaderSeconds);
        Shader.SetGlobalVector("_VivifyPreviewTime", new Vector4(seconds / 20f, seconds, seconds * 2f, seconds * 3f));
        Shader.SetGlobalVector("_VivifyPreviewSinTime", new Vector4(Mathf.Sin(seconds / 8f), Mathf.Sin(seconds / 4f), Mathf.Sin(seconds / 2f), Mathf.Sin(seconds)));
        Shader.SetGlobalVector("_VivifyPreviewCosTime", new Vector4(Mathf.Cos(seconds / 8f), Mathf.Cos(seconds / 4f), Mathf.Cos(seconds / 2f), Mathf.Cos(seconds)));
        Shader.SetGlobalVector("_VivifyPreviewDeltaTime", new Vector4(delta, delta > 0f ? 1f / delta : 0f, delta, delta > 0f ? 1f / delta : 0f));
        previousShaderSeconds = seconds;
    }

    private void ApplyNoteAnimations(NoteBinding note, double beat, double normalizedLife, ref Vector3 pos, ref Vector3 euler, ref Vector3 scale)
    {
        Dictionary<string, object> noteJson = FindNoteJson(note);
        if (noteJson != null)
        {
            var cd = GetDict(noteJson, "customData");
            var anim = GetDict(cd, "animation");
            ApplyAnimationDictionary(anim, normalizedLife, ref pos, ref euler, ref scale, true);
            var wr = cd == null ? null : GetList(cd, "worldRotation");
            if (wr != null && wr.Count >= 3) euler += new Vector3((float)ToDouble(wr[0]), (float)ToDouble(wr[1]), (float)ToDouble(wr[2]));
        }

        if (note.tracks != null)
        {
            for (int t = 0; t < note.tracks.Length; t++)
            {
                List<Dictionary<string, object>> list;
                if (string.IsNullOrEmpty(note.tracks[t]) || !pathEventsByTrack.TryGetValue(note.tracks[t], out list)) continue;
                ApplyLatestPathTrack(list, beat, normalizedLife, ref pos, ref euler, ref scale);
            }
        }
    }

    private static void ApplyLatestPathTrack(List<Dictionary<string, object>> list, double beat, double normalizedLife, ref Vector3 pos, ref Vector3 euler, ref Vector3 scale)
    {
        int idx = FindLastEventIndex(list, beat);
        if (idx < 0) return;
        bool offsetPos = false, position = false, offsetRot = false, localRot = false, rotation = false, scl = false;
        for (int i = idx; i >= 0 && !(offsetPos && position && offsetRot && localRot && rotation && scl); i--)
        {
            var data = GetDict(list[i], "d"); if (data == null) continue;
            object v;
            if (!offsetPos && data.TryGetValue("offsetPosition", out v)) { pos += EvalVector3(v, normalizedLife, Vector3.zero); offsetPos = true; }
            if (!position && data.TryGetValue("position", out v)) { pos += EvalVector3(v, normalizedLife, Vector3.zero); position = true; }
            if (!offsetRot && data.TryGetValue("offsetWorldRotation", out v)) { euler += EvalVector3(v, normalizedLife, Vector3.zero); offsetRot = true; }
            if (!localRot && data.TryGetValue("localRotation", out v)) { euler += EvalVector3(v, normalizedLife, Vector3.zero); localRot = true; }
            if (!rotation && data.TryGetValue("rotation", out v)) { euler = EvalVector3(v, normalizedLife, euler); rotation = true; }
            if (!scl && data.TryGetValue("scale", out v)) { scale = Vector3.Scale(scale, EvalVector3(v, normalizedLife, Vector3.one)); scl = true; }
        }
    }

    private static void ApplyAnimationDictionary(Dictionary<string, object> anim, double t, ref Vector3 pos, ref Vector3 euler, ref Vector3 scale, bool offsets)
    {
        if (anim == null) return;
        object v;
        if (anim.TryGetValue("offsetPosition", out v)) pos += EvalVector3(v, t, Vector3.zero);
        if (anim.TryGetValue("position", out v)) pos = offsets ? pos + EvalVector3(v, t, Vector3.zero) : EvalVector3(v, t, pos);
        if (anim.TryGetValue("offsetWorldRotation", out v)) euler += EvalVector3(v, t, Vector3.zero);
        if (anim.TryGetValue("localRotation", out v)) euler += EvalVector3(v, t, Vector3.zero);
        if (anim.TryGetValue("rotation", out v)) euler = EvalVector3(v, t, euler);
        if (anim.TryGetValue("scale", out v)) scale = Vector3.Scale(scale, EvalVector3(v, t, Vector3.one));
    }

    private void SetAllNotesActive(bool active)
    {
        for (int i = 0; i < notes.Count; i++)
        {
            var n = notes[i];
            if (n == null) continue;
            if (n.instance != null) n.instance.SetActive(active && preferCustomNotePrefabs && n.sourcePrefabHasVisibleRenderer);
            if (n.standardVisual != null) n.standardVisual.SetActive(active && (!preferCustomNotePrefabs || !n.sourcePrefabHasVisibleRenderer));
            if (n.debugProxy != null) n.debugProxy.SetActive(active && forceVisibleNoteProxy);
            if (n.debrisInstance != null) n.debrisInstance.SetActive(false);
            if (n.standardDebris != null) n.standardDebris.SetActive(false);
        }
    }

    private void EvaluateBombs(double beat)
    {
        for (int i = 0; i < bombs.Count; i++)
        {
            var b = bombs[i]; if (b == null || b.instance == null) continue;
            double half = ComputeHalfJumpBeats(Math.Max(0.1f, b.njs), b.jumpOffset, EffectiveBpm);
            double spawn = b.beat - half;
            bool active = beat >= spawn && beat < b.beat;
            if (b.instance.activeSelf != active) b.instance.SetActive(active);
            if (!active) continue;
            double t = Clamp01((beat - spawn) / Math.Max(0.0001, half));
            float distance = b.njs * (float)(60.0 / Math.Max(0.001, EffectiveBpm) * half);
            b.instance.transform.localPosition = new Vector3((b.x - 1.5f) * 0.6f, 0.6f + b.y * 0.6f, (float)((1.0 - t) * distance));
            b.instance.transform.localRotation = Quaternion.Euler((float)(beat * 60.0), (float)(beat * 80.0), 0f);
        }
    }

    private void SetBombsActive(bool active) { for (int i = 0; i < bombs.Count; i++) if (bombs[i] != null && bombs[i].instance != null) bombs[i].instance.SetActive(active); }

    private void EvaluateObstacles(double beat)
    {
        for (int i = 0; i < obstacles.Count; i++)
        {
            var o = obstacles[i]; if (o == null || o.instance == null) continue;
            double half = ComputeHalfJumpBeats(Math.Max(0.1f, o.njs), o.jumpOffset, EffectiveBpm);
            double spawn = o.beat - half;
            bool active = beat >= spawn && beat <= o.beat + Math.Max(0.05, o.duration);
            if (o.instance.activeSelf != active) o.instance.SetActive(active);
            if (!active) continue;
            double secPerBeat = 60.0 / Math.Max(0.001, EffectiveBpm);
            float zHead = (float)((o.beat - beat) * o.njs * secPerBeat);
            float zLen = Mathf.Max(0.2f, (float)(o.duration * o.njs * secPerBeat));
            float xCenter = ((o.x + o.width * 0.5f) - 2f) * 0.6f;
            float yCenter = (o.y + o.height * 0.5f) * 0.6f;
            o.instance.transform.localPosition = new Vector3(xCenter, yCenter, zHead + zLen * 0.5f);
            o.instance.transform.localScale = new Vector3(Mathf.Max(0.2f, o.width * 0.6f), Mathf.Max(0.2f, o.height * 0.6f), zLen);
        }
    }

    private void SetObstaclesActive(bool active) { for (int i = 0; i < obstacles.Count; i++) if (obstacles[i] != null && obstacles[i].instance != null) obstacles[i].instance.SetActive(active); }

    private void EvaluatePathVisuals(double beat)
    {
        EvaluatePathList(arcs, beat, false);
        EvaluatePathList(chains, beat, true);
    }

    private void EvaluatePathList(List<PathVisualBinding> list, double beat, bool chain)
    {
        if (list == null) return;
        for (int i = 0; i < list.Count; i++)
        {
            var a = list[i]; if (a == null || a.line == null) continue;
            double pre = 3.0;
            bool active = beat >= a.beat - pre && beat <= a.tailBeat + 0.25;
            a.line.gameObject.SetActive(active);
            if (!active) continue;
            int count = chain ? Math.Max(2, a.slices) : 16;
            if (a.line.positionCount != count) a.line.positionCount = count;
            double secPerBeat = 60.0 / Math.Max(0.001, EffectiveBpm);
            for (int p = 0; p < count; p++)
            {
                float u = count <= 1 ? 0f : p / (float)(count - 1);
                double pb = a.beat + (a.tailBeat - a.beat) * u;
                float x = Mathf.Lerp((a.x - 1.5f) * 0.6f, (a.tailX - 1.5f) * 0.6f, u);
                float y = Mathf.Lerp(0.6f + a.y * 0.6f, 0.6f + a.tailY * 0.6f, u);
                if (!chain) y += Mathf.Sin(u * Mathf.PI) * 0.35f;
                float z = (float)((pb - beat) * 10f * secPerBeat);
                a.line.SetPosition(p, new Vector3(x, y, z));
            }
            ApplyLineColor(a.line, a.color == 0 ? leftNoteColor : rightNoteColor);
        }
    }

    private static void ApplyLineColor(LineRenderer line, Color c)
    {
        if (line == null) return;
        var block = new MaterialPropertyBlock(); line.GetPropertyBlock(block); block.SetColor(ColorId, c); block.SetColor(Shader.PropertyToID("_EmissionColor"), c * 2.5f); line.SetPropertyBlock(block);
    }

    private void SetPathVisualsActive(bool active)
    {
        for (int i = 0; i < arcs.Count; i++) if (arcs[i] != null && arcs[i].line != null) arcs[i].line.gameObject.SetActive(active);
        for (int i = 0; i < chains.Count; i++) if (chains[i] != null && chains[i].line != null) chains[i].line.gameObject.SetActive(active);
    }

    private void EvaluateSabers(double beat)
    {
        if (sabers == null) return;

        bool customLeftAvailable = previewCustomSabers && sabers.leftCustom != null;
        bool customRightAvailable = previewCustomSabers && sabers.rightCustom != null;
        bool fallbackLeft = sabers.leftFallback != null && (alwaysShowStandardSabers || !customLeftAvailable);
        bool fallbackRight = sabers.rightFallback != null && (alwaysShowStandardSabers || !customRightAvailable);

        if (sabers.leftCustom != null) sabers.leftCustom.SetActive(customLeftAvailable);
        if (sabers.rightCustom != null) sabers.rightCustom.SetActive(customRightAvailable);
        if (sabers.leftFallback != null) sabers.leftFallback.SetActive(fallbackLeft);
        if (sabers.rightFallback != null) sabers.rightFallback.SetActive(fallbackRight);

        if (customLeftAvailable) ApplyNoteMaterialProperties(sabers.leftCustom, leftNoteColor, 1f);
        if (customRightAvailable) ApplyNoteMaterialProperties(sabers.rightCustom, rightNoteColor, 1f);

        Transform leftMotion = sabers.leftMotionPivot != null ? sabers.leftMotionPivot : sabers.leftHand;
        Transform rightMotion = sabers.rightMotionPivot != null ? sabers.rightMotionPivot : sabers.rightHand;
        if (leftMotion == null || rightMotion == null) return;

        Vector3 leftTargetPos; Quaternion leftTargetRot;
        Vector3 rightTargetPos; Quaternion rightTargetRot;
        CalculateSaberTargetPose(beat, 0, leftMotion, out leftTargetPos, out leftTargetRot);
        CalculateSaberTargetPose(beat, 1, rightMotion, out rightTargetPos, out rightTargetRot);

        if (!autoSwingSabers)
        {
            leftTargetPos = Vector3.zero; rightTargetPos = Vector3.zero;
            leftTargetRot = Quaternion.identity; rightTargetRot = Quaternion.identity;
        }

        // Song-time curves are already smooth; frame-delta smoothing both delayed cuts
        // and gave different poses for pause, scrubbing and a slow render frame.
        leftMotion.localPosition = Vector3.zero;
        rightMotion.localPosition = Vector3.zero;
        leftMotion.localRotation = leftTargetRot;
        rightMotion.localRotation = rightTargetRot;
    }

    private void CalculateSaberTargetPose(double beat, int color, Transform motionPivot, out Vector3 targetPosition, out Quaternion targetRotation)
    {
        targetPosition = Vector3.zero;
        targetRotation = Quaternion.identity;
        NoteBinding previous = null, next = null;
        foreach (var note in notes)
        {
            if (note == null || note.color != color) continue;
            if (note.beat >= beat && (next == null || note.beat < next.beat)) next = note;
            if (note.beat < beat && (previous == null || note.beat > previous.beat)) previous = note;
        }

        double recovery = Math.Max(0.1d, saberSwingWindowBeats);
        double spawn = next != null ? next.beat - ComputeHalfJumpBeats(Math.Max(0.1f, next.njs), next.jumpOffset, EffectiveBpm) : double.PositiveInfinity;
        if (next != null && beat >= spawn)
        {
            Quaternion approachFrom = Quaternion.identity;
            double approachStart = spawn;
            if (previous != null && previous.beat >= spawn)
            {
                double followEnd = previous.beat + Math.Min(recovery * 0.5d, (next.beat - previous.beat) * 0.35d);
                if (beat < followEnd)
                {
                    float follow = (float)Clamp01((beat - previous.beat) / Math.Max(0.0001d, followEnd - previous.beat));
                    targetRotation = AimSaberAtNote(previous, motionPivot, 0.38f * Mathf.SmoothStep(0f, 1f, follow));
                    return;
                }
                approachStart = followEnd;
                approachFrom = AimSaberAtNote(previous, motionPivot, 0.38f);
            }
            double strikeWindow = Math.Min(Math.Max(0.06d, saberApproachWindowBeats * 0.35d), (next.beat - approachStart) * 0.35d);
            double strikeStart = next.beat - strikeWindow;
            if (beat < strikeStart)
            {
                float approach = Mathf.SmoothStep(0f, 1f, (float)Clamp01((beat - approachStart) / Math.Max(0.0001d, strikeStart - approachStart)));
                targetRotation = Quaternion.Slerp(approachFrom, AimSaberAtNote(next, motionPivot, -0.38f), approach);
            }
            else
            {
                float strike = Mathf.SmoothStep(0f, 1f, (float)Clamp01((beat - strikeStart) / Math.Max(0.0001d, strikeWindow)));
                targetRotation = AimSaberAtNote(next, motionPivot, Mathf.Lerp(-0.38f, 0f, strike));
            }
        }
        else if (previous != null && beat - previous.beat <= recovery)
        {
            float phase = (float)Clamp01((beat - previous.beat) / recovery);
            Quaternion through = AimSaberAtNote(previous, motionPivot, 0.38f * Mathf.SmoothStep(0f, 1f, Mathf.Clamp01(phase * 2f)));
            targetRotation = Quaternion.Slerp(through, Quaternion.identity, Mathf.SmoothStep(0f, 1f, Mathf.InverseLerp(0.5f, 1f, phase)));
        }
        targetRotation = Quaternion.Slerp(Quaternion.identity, targetRotation, Mathf.Clamp01(saberReach));
    }

    private Quaternion AimSaberAtNote(NoteBinding note, Transform pivot, float cutSide)
    {
        // Sample the authored hit pose, not the hidden fallback or a note that has
        // already moved behind the player. Do not translate the hilt to fake a reach.
        Vector3 position = new Vector3((note.x - 1.5f) * 0.6f, 0.6f + note.y * 0.6f, 0f);
        Vector3 euler = note.prefabEuler;
        Vector3 scale = note.prefabScale;
        ApplyNoteAnimations(note, note.beat, 0.5d, ref position, ref euler, ref scale);
        position.z += noteHitPlaneZ;
        Vector2 cut = CutDirectionVector(note.direction);
        if (cut.sqrMagnitude < 0.001f) cut = Vector2.up;
        position += Quaternion.Euler(euler) * new Vector3(cut.x, cut.y, 0f) * cutSide;
        Vector3 world = noteRoot != null ? noteRoot.TransformPoint(position) : position;
        Vector3 direction = pivot.parent != null ? pivot.parent.InverseTransformPoint(world) : world - pivot.position;
        if (direction.sqrMagnitude < 0.0001f) return Quaternion.identity;
        Vector3 up = Mathf.Abs(Vector3.Dot(direction.normalized, Vector3.up)) > 0.98f ? Vector3.forward : Vector3.up;
        return Quaternion.LookRotation(direction, up);
    }

    private static float CutDirectionAngle(int d)
    {
        switch (d) { case 0: return 0f; case 1: return 180f; case 2: return 90f; case 3: return -90f; case 4: return 45f; case 5: return -45f; case 6: return 135f; case 7: return -135f; default: return 0f; }
    }

    private static Vector2 CutDirectionVector(int d)
    {
        switch (d)
        {
            case 0: return Vector2.up;
            case 1: return Vector2.down;
            case 2: return Vector2.left;
            case 3: return Vector2.right;
            case 4: return new Vector2(-1f, 1f).normalized;
            case 5: return new Vector2(1f, 1f).normalized;
            case 6: return new Vector2(-1f, -1f).normalized;
            case 7: return new Vector2(1f, -1f).normalized;
            default: return Vector2.zero;
        }
    }

    private void SetSabersActive(bool active)
    {
        if (sabers == null) return;
        bool customLeft = active && previewCustomSabers && sabers.leftCustom != null;
        bool customRight = active && previewCustomSabers && sabers.rightCustom != null;
        if (sabers.leftCustom != null) sabers.leftCustom.SetActive(customLeft);
        if (sabers.rightCustom != null) sabers.rightCustom.SetActive(customRight);
        if (sabers.leftFallback != null) sabers.leftFallback.SetActive(active && (alwaysShowStandardSabers || !customLeft));
        if (sabers.rightFallback != null) sabers.rightFallback.SetActive(active && (alwaysShowStandardSabers || !customRight));
    }

    private void EvaluateGenericEnvironment(double beat)
    {
        if (genericEnvironment == null) return;
        bool enabled = environmentMode == EnvironmentPreviewMode.GenericBeatSaber || (environmentMode == EnvironmentPreviewMode.Auto && !detectedVivify);
        if (genericEnvironment.gameObject.activeSelf != enabled) genericEnvironment.gameObject.SetActive(enabled);
        if (enabled) genericEnvironment.Evaluate(beat, genericLightEvents != null ? genericLightEvents : basicEvents, colorBoostEvents);
    }

    private static void SortByBeat(List<object> list)
    {
        if (list == null) return;
        list.Sort(delegate(object a, object b)
        {
            var da = a as Dictionary<string, object>; var db = b as Dictionary<string, object>;
            return GetNumber(da, "b", 0d).CompareTo(GetNumber(db, "b", 0d));
        });
    }

    private static bool DetectVivify(List<object> events)
    {
        if (events == null) return false;
        for (int i = 0; i < events.Count; i++)
        {
            var e = events[i] as Dictionary<string, object>; if (e == null) continue;
            string t = GetString(e, "t");
            if (t == "InstantiatePrefab" || t == "AssignObjectPrefab" || t == "SetMaterialProperty" || t == "Blit" || t == "SetRenderingSettings" || t == "SetCameraProperty") return true;
        }
        return false;
    }

    private void UpdateCameraMode()
    {
        if (previewCamera != null) previewCamera.enabled = !useSpectatorCamera;
        if (spectatorCamera != null) spectatorCamera.enabled = useSpectatorCamera;
        if (blitPreview != null) blitPreview.enabled = !useSpectatorCamera && previewPostProcessing;
        if (spectatorBlitPreview != null) spectatorBlitPreview.enabled = useSpectatorCamera && previewPostProcessing && spectatorReceivesPostProcessing;
    }

    private static double ComputeHalfJumpBeats(float njs, float offset, float bpm)
    {
        double secPerBeat = 60.0 / Math.Max(0.001, bpm);
        double half = 4.0;
        while (njs * secPerBeat * half > 18.0) half /= 2.0;
        half += offset;
        if (half < 1.0) half = 1.0;
        return half;
    }

    private static Vector3 EvalVector3(object raw, double t, Vector3 fallback)
    {
        var v4 = EvalVector4(raw, t, new Vector4(fallback.x, fallback.y, fallback.z, 0f));
        return new Vector3(v4.x, v4.y, v4.z);
    }

    private static Vector4 EvalVector4(object raw, double t, Vector4 fallback)
    {
        var list = raw as List<object>;
        if (list == null) return fallback;
        if (list.Count >= 3 && IsNumber(list[0]) && IsNumber(list[1]) && IsNumber(list[2]))
            return new Vector4((float)ToDouble(list[0]), (float)ToDouble(list[1]), (float)ToDouble(list[2]), list.Count > 3 && IsNumber(list[3]) ? (float)ToDouble(list[3]) : fallback.w);

        var keys = new List<Key4>();
        for (int i = 0; i < list.Count; i++)
        {
            var k = list[i] as List<object>;
            if (k == null || k.Count < 2 || !IsNumber(k[0])) continue;
            int timeIndex = -1;
            for (int j = k.Count - 1; j >= 1; j--) if (IsNumber(k[j])) { timeIndex = j; break; }
            if (timeIndex < 1) continue;
            float x = (float)ToDouble(k[0]);
            float y = timeIndex > 1 && IsNumber(k[1]) ? (float)ToDouble(k[1]) : 0f;
            float z = timeIndex > 2 && IsNumber(k[2]) ? (float)ToDouble(k[2]) : 0f;
            float w = timeIndex > 3 && IsNumber(k[3]) ? (float)ToDouble(k[3]) : 0f;
            string easing = k.Count > timeIndex + 1 ? Convert.ToString(k[timeIndex + 1]) : "linear";
            keys.Add(new Key4(new Vector4(x, y, z, w), ToDouble(k[timeIndex]), easing));
        }
        if (keys.Count == 0) return fallback;
        keys.Sort((a, b) => a.time.CompareTo(b.time));
        if (t <= keys[0].time) return keys[0].value;
        if (t >= keys[keys.Count - 1].time) return keys[keys.Count - 1].value;
        for (int i = 0; i < keys.Count - 1; i++)
        {
            var a = keys[i]; var b = keys[i + 1];
            if (t < a.time || t > b.time) continue;
            double span = Math.Max(0.000001, b.time - a.time);
            float u = (float)((t - a.time) / span);
            u = Ease(u, b.easing);
            return Vector4.LerpUnclamped(a.value, b.value, u);
        }
        return keys[keys.Count - 1].value;
    }

    private static double EvalScalar(object raw, double t, double fallback)
    {
        if (IsNumber(raw)) return ToDouble(raw);
        var list = raw as List<object>;
        if (list == null) return fallback;
        if (list.Count > 0 && IsNumber(list[0]) && (list.Count == 1 || !(list[0] is List<object>))) return ToDouble(list[0]);
        var keys = new List<Key1>();
        for (int i = 0; i < list.Count; i++)
        {
            var k = list[i] as List<object>;
            if (k == null || k.Count < 2 || !IsNumber(k[0]) || !IsNumber(k[1])) continue;
            string easing = k.Count > 2 ? Convert.ToString(k[2]) : "linear";
            keys.Add(new Key1(ToDouble(k[0]), ToDouble(k[1]), easing));
        }
        if (keys.Count == 0) return fallback;
        keys.Sort((a, b) => a.time.CompareTo(b.time));
        if (t <= keys[0].time) return keys[0].value;
        if (t >= keys[keys.Count - 1].time) return keys[keys.Count - 1].value;
        for (int i = 0; i < keys.Count - 1; i++)
        {
            var a = keys[i]; var b = keys[i + 1];
            if (t < a.time || t > b.time) continue;
            float u = (float)((t - a.time) / Math.Max(0.000001, b.time - a.time));
            u = Ease(u, b.easing);
            return a.value + (b.value - a.value) * u;
        }
        return keys[keys.Count - 1].value;
    }

    private struct Key4 { public Vector4 value; public double time; public string easing; public Key4(Vector4 v, double t, string e) { value = v; time = t; easing = e; } }
    private struct Key1 { public double value; public double time; public string easing; public Key1(double v, double t, string e) { value = v; time = t; easing = e; } }

    private static float Ease(float t, string name)
    {
        t = Mathf.Clamp01(t);
        if (string.IsNullOrEmpty(name) || name == "linear" || name == "easeLinear") return t;
        string n = name.ToLowerInvariant();
        bool io = n.Contains("inout"); bool inn = !io && n.Contains("in"); bool outt = !io && n.Contains("out");
        Func<float, float> baseIn;
        if (n.Contains("sine")) baseIn = x => 1f - Mathf.Cos(x * Mathf.PI * 0.5f);
        else if (n.Contains("quad")) baseIn = x => x * x;
        else if (n.Contains("cubic")) baseIn = x => x * x * x;
        else if (n.Contains("quart")) baseIn = x => x * x * x * x;
        else if (n.Contains("quint")) baseIn = x => x * x * x * x * x;
        else if (n.Contains("expo")) baseIn = x => x <= 0f ? 0f : Mathf.Pow(2f, 10f * x - 10f);
        else if (n.Contains("circ")) baseIn = x => 1f - Mathf.Sqrt(Mathf.Max(0f, 1f - x * x));
        else if (n.Contains("back")) baseIn = x => { const float c1 = 1.70158f; return (c1 + 1f) * x * x * x - c1 * x * x; };
        else if (n.Contains("step")) return t >= 1f ? 1f : 0f;
        else return t;
        if (io) return t < 0.5f ? 0.5f * baseIn(t * 2f) : 1f - 0.5f * baseIn((1f - t) * 2f);
        if (outt) return 1f - baseIn(1f - t);
        if (inn) return baseIn(t);
        return t;
    }

    private static double Clamp01(double v) { return v < 0d ? 0d : (v > 1d ? 1d : v); }
    private static bool IsNumber(object v) { return v is byte || v is sbyte || v is short || v is ushort || v is int || v is uint || v is long || v is ulong || v is float || v is double || v is decimal; }
    private static double ToDouble(object v) { if (v == null) return 0d; try { return Convert.ToDouble(v, System.Globalization.CultureInfo.InvariantCulture); } catch { return 0d; } }
    private static bool GetBoolValue(object v, bool fallback) { if (v is bool) return (bool)v; if (IsNumber(v)) return Math.Abs(ToDouble(v)) > 0.000001; bool b; return bool.TryParse(Convert.ToString(v), out b) ? b : fallback; }
    private static object GetValue(Dictionary<string, object> d, string key) { object v; return d != null && d.TryGetValue(key, out v) ? v : null; }
    private static string GetString(Dictionary<string, object> d, string key) { object v = GetValue(d, key); return v == null ? "" : Convert.ToString(v); }
    private static double GetNumber(Dictionary<string, object> d, string key, double fallback) { object v = GetValue(d, key); return IsNumber(v) ? ToDouble(v) : fallback; }
    private static Dictionary<string, object> GetDict(Dictionary<string, object> d, string key) { return GetValue(d, key) as Dictionary<string, object>; }
    private static List<object> GetList(Dictionary<string, object> d, string key) { return GetValue(d, key) as List<object>; }
    private static string[] GetTrackNames(object v)
    {
        if (v == null) return new string[0];
        var list = v as List<object>;
        if (list == null) return new[] { Convert.ToString(v) };
        var result = new string[list.Count];
        for (int i = 0; i < list.Count; i++) result[i] = Convert.ToString(list[i]);
        return result;
    }

    private static string GetTrackName(object v)
    {
        if (v == null) return "";
        var list = v as List<object>;
        if (list != null && list.Count > 0) return Convert.ToString(list[0]);
        return Convert.ToString(v);
    }
    private static string NormalizePath(string path) { return string.IsNullOrEmpty(path) ? "" : path.Replace('\\', '/').Trim().ToLowerInvariant(); }
}
}
