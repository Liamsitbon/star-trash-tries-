using UnityEngine.Playables;

namespace VivifyTimelinePreview
{
    public class BeatSaberPreviewDriverBehaviour : PlayableBehaviour
    {
        public BeatSaberMapData mapData;

        public override void ProcessFrame(Playable playable, FrameData info, object playerData)
        {
            var controller = playerData as BeatSaberVivifyPreviewController;
            if (controller == null) return;
            if (controller.mapData == null && mapData != null) controller.mapData = mapData;
            controller.EvaluateAtSeconds((float)playable.GetTime());
        }
    }
}
