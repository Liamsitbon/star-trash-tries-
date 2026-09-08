#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

namespace VivifyTimelinePreview
{
public static class BeatSaberProceduralFactory
{
    public static GameObject CreateStandardNote(Transform parent, int color, int direction, Color left, Color right)
    {
        GameObject root = new GameObject(color == 0 ? "Standard Left Note" : "Standard Right Note");
        root.transform.SetParent(parent, false);

        GameObject body = GameObject.CreatePrimitive(PrimitiveType.Cube);
        body.name = "Body";
        body.transform.SetParent(root.transform, false);
        // Beat Saber gameplay notes occupy an exact 0.5 m cube. Keep the root at
        // scale one so path/track animation remains independent of note geometry.
        body.transform.localScale = new Vector3(0.5f, 0.5f, 0.5f);
        RemoveCollider(body);
        AssignMaterial(body.GetComponent<Renderer>(), color == 0 ? left : right, true, color == 0 ? "Left Note" : "Right Note");

        if (direction == 8)
        {
            GameObject dot = GameObject.CreatePrimitive(PrimitiveType.Sphere);
            dot.name = "Dot";
            dot.transform.SetParent(root.transform, false);
            dot.transform.localPosition = new Vector3(0f, 0f, -0.255f);
            dot.transform.localScale = new Vector3(0.16f, 0.16f, 0.04f);
            RemoveCollider(dot);
            AssignMaterial(dot.GetComponent<Renderer>(), Color.white, true, "Dot");
        }
        else
        {
            GameObject arrow = new GameObject("Arrow");
            arrow.transform.SetParent(root.transform, false);
            arrow.transform.localPosition = new Vector3(0f, 0f, -0.255f);
            arrow.transform.localRotation = Quaternion.Euler(0f, 0f, DirectionAngle(direction));
            var mf = arrow.AddComponent<MeshFilter>();
            mf.sharedMesh = CreateArrowMesh();
            var mr = arrow.AddComponent<MeshRenderer>();
            mr.sharedMaterial = MakeMaterial(Color.white, true, "Note Arrow");
        }
        return root;
    }

    public static GameObject CreateStandardNoteDebris(Transform parent, int color, Color left, Color right)
    {
        GameObject root = new GameObject(color == 0 ? "Standard Left Note Debris" : "Standard Right Note Debris");
        root.transform.SetParent(parent, false);
        Color c = color == 0 ? left : right;
        for (int i = 0; i < 2; i++)
        {
            GameObject half = GameObject.CreatePrimitive(PrimitiveType.Cube);
            half.name = i == 0 ? "Half A" : "Half B";
            half.transform.SetParent(root.transform, false);
            half.transform.localPosition = new Vector3(i == 0 ? -0.1275f : 0.1275f, 0f, 0f);
            half.transform.localScale = new Vector3(0.245f, 0.5f, 0.5f);
            RemoveCollider(half);
            AssignMaterial(half.GetComponent<Renderer>(), c, true, "Note Debris");
        }
        root.SetActive(false);
        return root;
    }

    public static GameObject CreateBomb(Transform parent)
    {
        GameObject root = new GameObject("Standard Bomb"); root.transform.SetParent(parent, false);
        GameObject sphere = GameObject.CreatePrimitive(PrimitiveType.Sphere); sphere.name = "Bomb"; sphere.transform.SetParent(root.transform, false); sphere.transform.localScale = Vector3.one * 0.46f; RemoveCollider(sphere);
        AssignMaterial(sphere.GetComponent<Renderer>(), new Color(0.08f, 0.08f, 0.1f, 1f), true, "Bomb");
        for (int i = 0; i < 8; i++)
        {
            GameObject spike = GameObject.CreatePrimitive(PrimitiveType.Cube); spike.name = "Spike"; spike.transform.SetParent(root.transform, false); RemoveCollider(spike);
            float a = i * 45f * Mathf.Deg2Rad; spike.transform.localPosition = new Vector3(Mathf.Cos(a), Mathf.Sin(a), 0f) * 0.29f; spike.transform.localRotation = Quaternion.Euler(0f, 0f, -i * 45f); spike.transform.localScale = new Vector3(0.18f, 0.05f, 0.05f);
            AssignMaterial(spike.GetComponent<Renderer>(), new Color(0.12f, 0.12f, 0.14f, 1f), true, "Bomb Spike");
        }
        return root;
    }

    public static GameObject CreateWall(Transform parent, Color color)
    {
        GameObject go = GameObject.CreatePrimitive(PrimitiveType.Cube); go.name = "Standard Wall"; go.transform.SetParent(parent, false); RemoveCollider(go);
        Shader wallShader = Shader.Find("VivifyTimelinePreview/GenericTransparentPreview");
        Material m = wallShader != null ? new Material(wallShader) : MakeMaterial(color, false, "Wall");
        m.name = "__Preview Wall";
        m.hideFlags = HideFlags.HideAndDontSave;
        if (m.HasProperty("_Color")) m.SetColor("_Color", color);
        m.SetInt("_SrcBlend", (int)UnityEngine.Rendering.BlendMode.SrcAlpha);
        m.SetInt("_DstBlend", (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha);
        m.SetInt("_ZWrite", 0);
        m.renderQueue = 3000;
        go.GetComponent<Renderer>().sharedMaterial = m;
        return go;
    }

    public static GameObject CreateSaber(Transform parent, Color color, string name)
    {
        GameObject root = new GameObject(name);
        root.transform.SetParent(parent, false);

        // Beat Saber-ish handle: dark grip + colored emitter ring.
        GameObject handle = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
        handle.name = "Handle";
        handle.transform.SetParent(root.transform, false);
        handle.transform.localPosition = new Vector3(0f, 0f, 0.13f);
        handle.transform.localRotation = Quaternion.Euler(90f, 0f, 0f);
        handle.transform.localScale = new Vector3(0.055f, 0.13f, 0.055f);
        RemoveCollider(handle);
        AssignMaterial(handle.GetComponent<Renderer>(), new Color(0.055f, 0.06f, 0.075f, 1f), false, "Saber Handle");

        GameObject emitter = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
        emitter.name = "Emitter";
        emitter.transform.SetParent(root.transform, false);
        emitter.transform.localPosition = new Vector3(0f, 0f, 0.29f);
        emitter.transform.localRotation = Quaternion.Euler(90f, 0f, 0f);
        emitter.transform.localScale = new Vector3(0.072f, 0.028f, 0.072f);
        RemoveCollider(emitter);
        AssignMaterial(emitter.GetComponent<Renderer>(), color, true, "Saber Emitter");

        // Outer colored blade and smaller white core. Both only use built-in shaders,
        // so they stay red/blue even if source Vivify saber shaders fail on Metal.
        GameObject blade = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
        blade.name = "Blade";
        blade.transform.SetParent(root.transform, false);
        blade.transform.localPosition = new Vector3(0f, 0f, 0.82f);
        blade.transform.localRotation = Quaternion.Euler(90f, 0f, 0f);
        blade.transform.localScale = new Vector3(0.034f, 0.53f, 0.034f);
        RemoveCollider(blade);
        AssignMaterial(blade.GetComponent<Renderer>(), color, true, "Saber Blade");

        GameObject core = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
        core.name = "Blade Core";
        core.transform.SetParent(root.transform, false);
        core.transform.localPosition = new Vector3(0f, 0f, 0.82f);
        core.transform.localRotation = Quaternion.Euler(90f, 0f, 0f);
        core.transform.localScale = new Vector3(0.014f, 0.525f, 0.014f);
        RemoveCollider(core);
        AssignMaterial(core.GetComponent<Renderer>(), new Color(0.96f, 0.98f, 1f, 1f), true, "Saber Core");

        GameObject tip = GameObject.CreatePrimitive(PrimitiveType.Sphere);
        tip.name = "Tip";
        tip.transform.SetParent(root.transform, false);
        tip.transform.localPosition = new Vector3(0f, 0f, 1.35f);
        tip.transform.localScale = Vector3.one * 0.055f;
        RemoveCollider(tip);
        AssignMaterial(tip.GetComponent<Renderer>(), color, true, "Saber Tip");
        return root;
    }

    public static BeatSaberGenericEnvironment CreateGenericEnvironment(Transform parent)
    {
        GameObject root = new GameObject("Generic Beat Saber Environment"); root.transform.SetParent(parent, false);
        var env = root.AddComponent<BeatSaberGenericEnvironment>();
        List<Renderer> back = new List<Renderer>(), rings = new List<Renderer>(), left = new List<Renderer>(), right = new List<Renderer>(), center = new List<Renderer>(), extra = new List<Renderer>(), lane = new List<Renderer>(), geometry = new List<Renderer>();

        GameObject floor = Cube(root.transform, "Stage", new Vector3(0f, -0.08f, 28f), new Vector3(7f, 0.12f, 70f), new Color(0.015f, 0.018f, 0.025f, 1f), false); geometry.Add(floor.GetComponent<Renderer>());
        for (int i = 0; i < 5; i++)
        {
            float x = (i - 2) * 0.6f;
            GameObject line = Cube(root.transform, "Lane " + i, new Vector3(x, 0.015f, 24f), new Vector3(0.018f, 0.018f, 58f), new Color(0.08f, 0.2f, 0.45f, 1f), true); lane.Add(line.GetComponent<Renderer>());
        }
        for (int z = 6; z <= 54; z += 6)
        {
            GameObject cross = Cube(root.transform, "Grid " + z, new Vector3(0f, 0.012f, z), new Vector3(5.2f, 0.015f, 0.025f), new Color(0.04f, 0.08f, 0.16f, 1f), true); lane.Add(cross.GetComponent<Renderer>());
        }
        for (int i = 0; i < 10; i++)
        {
            float z = 4f + i * 5.5f;
            GameObject l = Cube(root.transform, "Left Laser " + i, new Vector3(-3.5f, 1.8f, z), new Vector3(0.08f, 3.5f, 0.08f), env.leftColor * 0.2f, true); left.Add(l.GetComponent<Renderer>());
            GameObject r = Cube(root.transform, "Right Laser " + i, new Vector3(3.5f, 1.8f, z), new Vector3(0.08f, 3.5f, 0.08f), env.rightColor * 0.2f, true); right.Add(r.GetComponent<Renderer>());
        }
        for (int i = 0; i < 5; i++)
        {
            float z = 12f + i * 9f;
            CreateRing(root.transform, z, 3.4f + i * 0.15f, rings, new Color(0.05f, 0.12f, 0.3f, 1f));
        }
        for (int i = 0; i < 9; i++)
        {
            float x = (i - 4) * 0.7f;
            GameObject b = Cube(root.transform, "Back Light " + i, new Vector3(x, 2.8f + Mathf.Abs(i - 4) * 0.15f, 58f), new Vector3(0.18f, 4.5f, 0.18f), Color.white * 0.08f, true); back.Add(b.GetComponent<Renderer>());
        }
        GameObject c1 = Cube(root.transform, "Center Light", new Vector3(0f, 1.4f, 18f), new Vector3(0.08f, 2.8f, 0.08f), Color.white * 0.08f, true); center.Add(c1.GetComponent<Renderer>());
        GameObject e1 = Cube(root.transform, "Extra Light L", new Vector3(-2.2f, 0.5f, 8f), new Vector3(0.05f, 1f, 0.05f), env.leftColor * 0.15f, true); extra.Add(e1.GetComponent<Renderer>());
        GameObject e2 = Cube(root.transform, "Extra Light R", new Vector3(2.2f, 0.5f, 8f), new Vector3(0.05f, 1f, 0.05f), env.rightColor * 0.15f, true); extra.Add(e2.GetComponent<Renderer>());

        env.backLights.renderers = back.ToArray(); env.ringLights.renderers = rings.ToArray(); env.leftLights.renderers = left.ToArray(); env.rightLights.renderers = right.ToArray(); env.centerLights.renderers = center.ToArray(); env.extraLights.renderers = extra.ToArray(); env.laneGlow = lane.ToArray(); env.staticGeometry = geometry.ToArray(); env.SetIdle();
        return env;
    }

    public static LineRenderer CreateArcLine(Transform parent, Color color, string name)
    {
        GameObject go = new GameObject(name); go.transform.SetParent(parent, false); var lr = go.AddComponent<LineRenderer>(); lr.positionCount = 16; lr.useWorldSpace = false; lr.widthMultiplier = 0.045f; lr.sharedMaterial = MakeMaterial(color, true, name + " Material"); lr.numCapVertices = 3; return lr;
    }

    private static void CreateRing(Transform parent, float z, float radius, List<Renderer> output, Color color)
    {
        for (int i = 0; i < 20; i++)
        {
            float a = (i / 19f) * Mathf.PI;
            float x = Mathf.Cos(a) * radius; float y = Mathf.Sin(a) * radius + 0.25f;
            GameObject seg = Cube(parent, "Ring Segment", new Vector3(x, y, z), new Vector3(0.05f, 0.38f, 0.05f), color, true); seg.transform.localRotation = Quaternion.Euler(0f, 0f, -a * Mathf.Rad2Deg); output.Add(seg.GetComponent<Renderer>());
        }
    }

    private static GameObject Cube(Transform parent, string name, Vector3 pos, Vector3 scale, Color color, bool emission)
    {
        GameObject go = GameObject.CreatePrimitive(PrimitiveType.Cube); go.name = name; go.transform.SetParent(parent, false); go.transform.localPosition = pos; go.transform.localScale = scale; RemoveCollider(go); AssignMaterial(go.GetComponent<Renderer>(), color, emission, name); return go;
    }

    private static void AssignMaterial(Renderer r, Color color, bool emission, string name)
    {
        if (r != null) r.sharedMaterial = MakeMaterial(color, emission, name);
    }

    private static Material MakeMaterial(Color color, bool emission, string name)
    {
        // Prefer a tiny built-in unlit shader for preview geometry. It is much more
        // robust in old Unity 2019 projects and avoids pink fallback geometry when
        // the project's Standard shader variant is unavailable/broken.
        Shader sh = BeatSaberPreviewMaterials.FindUsable("VivifyTimelinePreview/GenericPreview", "Unlit/Color", "Sprites/Default");
        if (sh == null) throw new InvalidOperationException("No supported preview shader was imported. Reimport VivifyTimelinePreview/Shaders before creating the rig.");
        Material m = new Material(sh); m.name = "__Preview " + name; m.hideFlags = HideFlags.HideAndDontSave;
        if (m.HasProperty("_Color")) m.SetColor("_Color", color);
        if (emission && m.HasProperty("_EmissionColor")) { m.EnableKeyword("_EMISSION"); m.SetColor("_EmissionColor", color * 2.5f); }
        return m;
    }

    private static Mesh CreateArrowMesh()
    {
        Mesh m = new Mesh(); m.name = "__Preview Note Arrow";
        m.vertices = new[] { new Vector3(0f, 0.19f, 0f), new Vector3(-0.16f, -0.06f, 0f), new Vector3(-0.055f, -0.06f, 0f), new Vector3(-0.055f, -0.19f, 0f), new Vector3(0.055f, -0.19f, 0f), new Vector3(0.055f, -0.06f, 0f), new Vector3(0.16f, -0.06f, 0f) };
        m.triangles = new[] { 0,1,2, 0,2,5, 0,5,6, 2,3,4, 2,4,5 };
        m.RecalculateBounds(); return m;
    }

    private static float DirectionAngle(int d)
    {
        switch (d) { case 0: return 0f; case 1: return 180f; case 2: return 90f; case 3: return -90f; case 4: return 45f; case 5: return -45f; case 6: return 135f; case 7: return -135f; default: return 0f; }
    }

    private static void RemoveCollider(GameObject go) { var c = go != null ? go.GetComponent<Collider>() : null; if (c != null) UnityEngine.Object.DestroyImmediate(c); }
}
}
#endif
