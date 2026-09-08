using UnityEngine;
using UnityEngine.Playables;
using UnityEngine.Timeline;

namespace VivifyTimelinePreview
{

[System.Serializable]
public class BeatSaberEventAsset : PlayableAsset, ITimelineClipAsset
{
    [TextArea(2, 14)] public string rawJson;
    public string eventType;
    public string trackName;
    public double beat;
    public double durationBeats;
    public ClipCaps clipCaps { get { return ClipCaps.None; } }
    public override Playable CreatePlayable(PlayableGraph graph, GameObject owner) { return Playable.Create(graph, 0); }
}
}
