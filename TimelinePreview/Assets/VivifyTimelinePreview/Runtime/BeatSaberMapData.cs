using UnityEngine;

namespace VivifyTimelinePreview
{
public class BeatSaberMapData : ScriptableObject
{
    [TextArea(6, 20)] public string json;
    [TextArea(3, 12)] public string lightshowJson;
    public string sourceName;
    public string sourceDatPath;
    public string sourceLightshowPath;
    public string sourceInfoPath;
    public string songFilename;
    public string environmentName;
    public string detectedFormat = "unknown";
    public float bpm = 70f;
    public float defaultNjs = 10f;
    public float defaultJumpOffset = 0f;
    public double maxBeat;
    public float defaultTimelineOffsetSeconds;
}
}
