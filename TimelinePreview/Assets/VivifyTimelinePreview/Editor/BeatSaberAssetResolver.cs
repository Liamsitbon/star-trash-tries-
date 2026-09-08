#if UNITY_EDITOR
using System;
using System.IO;
using UnityEditor;
using UnityEngine;

namespace VivifyTimelinePreview
{

public static class BeatSaberAssetResolver
{
    public static string ResolveAssetPath(string beatSaberPath, string typeFilter)
    {
        if (string.IsNullOrEmpty(beatSaberPath)) return null;
        string normalized = beatSaberPath.Replace('\\', '/').TrimStart('/');
        if (normalized.StartsWith("assets/", StringComparison.OrdinalIgnoreCase))
            normalized = "Assets/" + normalized.Substring(7);
        else if (!normalized.StartsWith("Assets/", StringComparison.OrdinalIgnoreCase))
            normalized = "Assets/" + normalized;

        UnityEngine.Object exact = AssetDatabase.LoadAssetAtPath<UnityEngine.Object>(normalized);
        if (exact != null) return normalized;

        string filename = Path.GetFileNameWithoutExtension(normalized);
        string[] guids = AssetDatabase.FindAssets(filename + (string.IsNullOrEmpty(typeFilter) ? "" : " t:" + typeFilter));
        string normalizedLower = normalized.ToLowerInvariant();
        string best = null;
        for (int i = 0; i < guids.Length; i++)
        {
            string candidate = AssetDatabase.GUIDToAssetPath(guids[i]).Replace('\\', '/');
            if (candidate.Equals(normalized, StringComparison.OrdinalIgnoreCase)) return candidate;
            if (candidate.ToLowerInvariant().EndsWith(normalizedLower.Substring("assets/".Length))) best = candidate;
            else if (best == null && string.Equals(Path.GetFileName(candidate), Path.GetFileName(normalized), StringComparison.OrdinalIgnoreCase)) best = candidate;
        }
        return best;
    }

    public static T Load<T>(string beatSaberPath, string typeFilter) where T : UnityEngine.Object
    {
        string p = ResolveAssetPath(beatSaberPath, typeFilter);
        return string.IsNullOrEmpty(p) ? null : AssetDatabase.LoadAssetAtPath<T>(p);
    }
}
}
#endif
