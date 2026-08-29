# Vivify Quest 0.5.25 verification matrix

1. Enable Multiview safety and launch 42 Flux. Confirm both eyes show the same scene; neither eye is black or white-only.
2. Confirm the unsafe Bloom pyramid is skipped but safe camera effects and secondary-camera isolation still run.
3. Launch Murder Plot with safety enabled. Confirm the KickWow, GaussianBlur, ChromaticAberration, and Vignette chain is filtered and repeated rows do not build up.
4. Launch You, Aether, and Overclocked. Confirm visuals match the safety-disabled stereo path except for effects on the explicit denylist.
5. Exit a Vivify map, select another song, and confirm Beat Saber does not crash.
6. Disable Multiview safety and confirm the complete unfiltered renderer is restored without any hidden stereo overrides.
