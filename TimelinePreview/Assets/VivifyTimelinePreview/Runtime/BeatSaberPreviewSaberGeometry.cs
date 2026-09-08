using System;
using UnityEngine;

namespace VivifyTimelinePreview
{
    [DisallowMultipleComponent]
    public sealed class BeatSaberPreviewSaberGeometry : MonoBehaviour
    {
        [SerializeField, HideInInspector] private bool aligned;
        [SerializeField, HideInInspector] private Vector3 tipInMeshSpace;

        public void AlignGrip()
        {
            if (aligned) return;
            Bounds hilt = new Bounds(), blade = new Bounds();
            bool haveHilt = false, haveBlade = false;
            foreach (var filter in GetComponentsInChildren<MeshFilter>(true))
            {
                var renderer = filter.GetComponent<Renderer>();
                var mesh = filter.sharedMesh;
                if (renderer == null || mesh == null || (!mesh.isReadable && !Application.isEditor)) continue;
                if (filter.name.IndexOf("guide", StringComparison.OrdinalIgnoreCase) >= 0)
                {
                    renderer.enabled = false;
                    continue;
                }
                var vertices = mesh.vertices;
                var materials = renderer.sharedMaterials;
                for (int s = 0; s < mesh.subMeshCount && s < materials.Length; s++)
                {
                    string label = (filter.name + " " + (materials[s] != null ? materials[s].name : "")).ToLowerInvariant();
                    bool isHilt = label.Contains("hilt") || label.Contains("handle") || label.Contains("grip");
                    bool isBlade = label.Contains("blade");
                    if (!isHilt && !isBlade) continue;
                    foreach (int index in mesh.GetIndices(s))
                    {
                        Vector3 v = transform.InverseTransformPoint(filter.transform.TransformPoint(vertices[index]));
                        if (isHilt) { if (!haveHilt) hilt = new Bounds(v, Vector3.zero); else hilt.Encapsulate(v); haveHilt = true; }
                        if (isBlade) { if (!haveBlade) blade = new Bounds(v, Vector3.zero); else blade.Encapsulate(v); haveBlade = true; }
                    }
                }
            }
            if (!haveHilt || !haveBlade) return;
            Vector3 axis = (blade.center - hilt.center).normalized;
            if (axis.sqrMagnitude < 0.5f) return;
            // Put the actual grip centre at the motion pivot, and its blade along +Z.
            Quaternion rotation = Quaternion.FromToRotation(Vector3.Scale(axis, transform.localScale).normalized, Vector3.forward);
            transform.localRotation = rotation;
            transform.localPosition = -(rotation * Vector3.Scale(hilt.center, transform.localScale));
            float extent = Vector3.Dot(new Vector3(Mathf.Abs(axis.x), Mathf.Abs(axis.y), Mathf.Abs(axis.z)), blade.extents);
            tipInMeshSpace = blade.center + axis * extent;
            aligned = true;
            foreach (var trail in GetComponentsInChildren<TrailRenderer>(true))
                if (trail.name == "Preview Saber Trail") trail.transform.position = transform.TransformPoint(tipInMeshSpace);
        }
    }
}
