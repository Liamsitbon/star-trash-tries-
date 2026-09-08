using UnityEngine;
using UnityEngine.Playables;
using UnityEngine.Timeline;

namespace VivifyTimelinePreview
{
    [System.Serializable]
    public class BeatSaberPreviewDriverAsset : PlayableAsset, ITimelineClipAsset
    {
        public BeatSaberMapData mapData;
        public ClipCaps clipCaps { get { return ClipCaps.None; } }

        public override Playable CreatePlayable(PlayableGraph graph, GameObject owner)
        {
            var playable = ScriptPlayable<BeatSaberPreviewDriverBehaviour>.Create(graph);
            playable.GetBehaviour().mapData = mapData;
            return playable;
        }
    }
}
