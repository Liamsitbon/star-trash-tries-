using System;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Nexora.Editor
{
    // Synthetic GPU A/B only. Does not open/edit a map, capture user content,
    // load other projects or claim that Metal rendering proves Quest rendering.
    public static class NexoraRenderingProbe
    {
        public static void Run()
        {
            var scene = EditorSceneManager.NewPreviewScene();
            var previous = RenderTexture.active;
            Material video = null, solid = null;
            Texture2D source = null, readback = null;
            RenderTexture target = null;
            Mesh mesh = null;
            try
            {
                var shader = AssetDatabase.LoadAssetAtPath<Shader>(
                    "Assets/Nexora/Shaders/NexoraDome.shader");
                if (shader == null || !shader.isSupported)
                    throw new InvalidOperationException("Editor cannot render the source shader");
                video = new Material(shader) { renderQueue = 2501 };
                video.SetFloat("_VideoReady", 1);
                video.SetFloat("_Opacity", 1);
                video.SetFloat("_Brightness", 1);
                video.SetFloat("_Saturation", 1);
                video.SetFloat("_CameraAmount", 0);
                video.SetColor("_Tint", Color.white);
                source = new Texture2D(10, 4, TextureFormat.RGBA32, false, false)
                    { wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Point };
                for (var y = 0; y < 4; ++y)
                    for (var x = 0; x < 10; ++x)
                        source.SetPixel(x, y, new Color(x / 9f, y / 3f, (x % 3) / 2f, 1));
                source.Apply(false, false);
                video.mainTexture = source;
                mesh = new Mesh { name = "NexoraSyntheticQuad" };
                mesh.vertices = new[] {
                    new Vector3(-1,-1,2).normalized, new Vector3(1,-1,2).normalized,
                    new Vector3(1,1,2).normalized, new Vector3(-1,1,2).normalized };
                mesh.uv = new[] { Vector2.zero, Vector2.right, Vector2.one, Vector2.up };
                mesh.triangles = new[] { 0, 2, 1, 0, 3, 2 };
                mesh.RecalculateNormals();
                mesh.bounds = new Bounds(Vector3.zero, Vector3.one * 32);
                var dome = new GameObject("NexoraProbeQuad", typeof(MeshFilter), typeof(MeshRenderer));
                SceneManager.MoveGameObjectToScene(dome, scene);
                dome.GetComponent<MeshFilter>().sharedMesh = mesh;
                dome.GetComponent<MeshRenderer>().sharedMaterial = video;
                var cameraObject = new GameObject("NexoraProbeCamera", typeof(Camera));
                SceneManager.MoveGameObjectToScene(cameraObject, scene);
                var camera = cameraObject.GetComponent<Camera>();
                camera.enabled = false;
                camera.scene = scene;
                camera.clearFlags = CameraClearFlags.SolidColor;
                camera.backgroundColor = Color.black;
                camera.nearClipPlane = 0.1f;
                camera.farClipPlane = 30;
                camera.fieldOfView = 60;
                camera.allowHDR = false;
                target = new RenderTexture(128, 128, 24, RenderTextureFormat.ARGB32);
                target.Create();
                camera.targetTexture = target;
                readback = new Texture2D(128, 128, TextureFormat.RGBA32, false);
                Func<Color[]> capture = () => {
                    camera.Render();
                    RenderTexture.active = target;
                    readback.ReadPixels(new Rect(0,0,128,128), 0, 0, false);
                    return readback.GetPixels();
                };
                video.SetFloat("_RawSampling", 1);
                var raw = capture();
                video.SetFloat("_RawSampling", 0);
                video.SetFloat("_SimpleSampling", 1);
                var simple = capture();
                video.SetFloat("_SimpleSampling", 0);
                var full = capture();
                float rangeMin = 1, rangeMax = 0, simpleError = 0, fullError = 0;
                for (var i = 0; i < raw.Length; ++i)
                {
                    rangeMin = Mathf.Min(rangeMin, raw[i].r);
                    rangeMax = Mathf.Max(rangeMax, raw[i].r);
                    simpleError = Mathf.Max(simpleError, Mathf.Abs(simple[i].r - raw[i].r));
                    // Quantized UVs can move a sample across a test pattern edge.
                    fullError += Mathf.Abs(full[i].r - raw[i].r) / raw.Length;
                }
                if (rangeMax - rangeMin < 0.5f || simpleError > 0.02f || fullError > 0.03f)
                    throw new InvalidOperationException($"RGB A/B failed: range={rangeMax-rangeMin} simple={simpleError} fullMean={fullError}");

                var cube = GameObject.CreatePrimitive(PrimitiveType.Cube);
                SceneManager.MoveGameObjectToScene(cube, scene);
                cube.transform.position = new Vector3(0, 0, 3);
                solid = new Material(Shader.Find("Unlit/Color")) {
                    color = Color.green, renderQueue = 3000 };
                cube.GetComponent<MeshRenderer>().sharedMaterial = solid;
                video.EnableKeyword("NEXORA_RGBD_ON");
                video.SetFloat("_SimpleSampling", 1);
                video.SetFloat("_RgbdNear", 2);
                video.SetFloat("_RgbdFar", 8);
                video.SetFloat("_RgbdColorWidth", 0.8f);
                video.SetFloat("_DepthWrite", 1);
                Action<float> setDepth = code => {
                    for (var y = 0; y < 4; ++y)
                        for (var x = 0; x < 10; ++x)
                            source.SetPixel(x, y, x < 8 ? Color.blue : new Color(code, code, code, 1));
                    source.Apply(false, false);
                };
                const int center = 64 * 128 + 64;
                setDepth(0);
                var near = capture()[center];
                setDepth(1);
                var far = capture()[center];
                setDepth(0);
                video.SetFloat("_DepthWrite", 0);
                var noDepthWrite = capture()[center];
                if (near.b < 0.9f || near.g > 0.1f || far.g < 0.9f || noDepthWrite.g < 0.9f)
                    throw new InvalidOperationException($"RGBD occlusion A/B failed: near={near}, far={far}, noWrite={noDepthWrite}");
                cube.SetActive(false);
                video.SetFloat("_RawSampling", 1);
                video.SetFloat("_RgbdEdgeRepair", 0); // This quad is not an equirectangular sphere.
                Func<Color[],float> centroid = pixels => {
                    double sum=0, count=0;
                    for (int i=0;i<pixels.Length;++i)
                        if (pixels[i].b>.8f && pixels[i].r<.1f) { sum+=i%128; ++count; }
                    if (count<32) throw new InvalidOperationException("Depth translation probe lost geometry");
                    return (float)(sum/count);
                };
                setDepth(0); var nearOrigin = centroid(capture());
                setDepth(1); var farOrigin = centroid(capture());
                camera.transform.position = new Vector3(.15f,0,0);
                video.SetVector("_RgbdHead",new Vector4(.15f,0,0,0));
                setDepth(0); var nearShift = Mathf.Abs(centroid(capture())-nearOrigin);
                setDepth(1); var farShift = Mathf.Abs(centroid(capture())-farOrigin);
                video.SetFloat("_RgbdStrength",0);
                var zeroStrengthShift = Mathf.Abs(centroid(capture())-farOrigin);
                if (nearShift < farShift*2.5f || farShift < .5f || zeroStrengthShift > .6f)
                    throw new InvalidOperationException($"Depth-dependent translation failed: near={nearShift} far={farShift} zero={zeroStrengthShift}");
                Debug.Log($"NEXORA_DEPTH_TRANSLATION_OK api={SystemInfo.graphicsDeviceType} nearShiftPixels={nearShift} farShiftPixels={farShift} zeroStrengthPixels={zeroStrengthShift} sphereTransformUnchanged=true questStereoProven=false");
                Debug.Log($"NEXORA_EDITOR_PROBE_OK api={SystemInfo.graphicsDeviceType} rgbRange={rangeMax-rangeMin} simpleError={simpleError} fullMeanError={fullError} depthNear={near} depthFar={far} depthWriteOff={noDepthWrite} questRuntimeProven=false");
            }
            finally
            {
                RenderTexture.active = previous;
                EditorSceneManager.ClosePreviewScene(scene);
                if (target != null) { target.Release(); UnityEngine.Object.DestroyImmediate(target); }
                if (readback != null) UnityEngine.Object.DestroyImmediate(readback);
                if (source != null) UnityEngine.Object.DestroyImmediate(source);
                if (video != null) UnityEngine.Object.DestroyImmediate(video);
                if (solid != null) UnityEngine.Object.DestroyImmediate(solid);
                if (mesh != null) UnityEngine.Object.DestroyImmediate(mesh);
            }
        }
    }
}
