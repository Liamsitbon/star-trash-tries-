using UnityEngine.Timeline;

namespace VivifyTimelinePreview
{
    [TrackClipType(typeof(BeatSaberPreviewDriverAsset))]
    [TrackBindingType(typeof(BeatSaberVivifyPreviewController))]
    [TrackColor(0.95f, 0.55f, 0.15f)]
    public class BeatSaberPreviewDriverTrack : TrackAsset
    {
    }
}
