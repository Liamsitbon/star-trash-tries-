# Nexora white-video diagnostic (0.3.3)

This is instrumentation, not a fix for the reported white dome.

1. Install the 0.3.3 Quest QMOD, then close Beat Saber normally.
2. In `/sdcard/ModData/com.beatgames.beatsaber/Configs/nexora.json`, set only
   `debugLogging` to `true`, preserving the other settings. Keep `fileLogging`
   enabled. Nexora reads these settings on startup.
3. Start Dynasty's Nexora difficulty. When the white dome is visible, press the
   normal in-game Pause button and leave the headset awake briefly.
4. Collect `/sdcard/ModData/com.beatgames.beatsaber/Logs/Nexora.log` before the next
   app launch (the log is replaced on launch). Restore `debugLogging` to `false`
  after closing the game to finish the diagnostic session.

The read-only helper can summarize the current log without downloading media,
installing anything, enabling debugging or restarting Beat Saber:

```sh
python3 Nexora/Current-Source/scripts/check_video_log.py --adb /opt/homebrew/bin/adb
```

Or use `--log /path/to/Nexora.log`. `no_pixel_evidence` is expected for old builds
or without a Debug-mode pause probe; a decoder callback alone is not a pass.

Only one attempt per loaded video is allowed. It samples five 8x8 patches along
the decoded texture's horizon, logs RGB statistics and material/texture instance
IDs, then destroys its temporary CPU texture. It never changes playback, replaces
the shader, clears the video target, forces stereo keywords or saves video frames.
With debugging disabled, there is no pixel readback. The diagnostic does not run
while the song is playing or while the headset/app is suspended.

`ReadPixels` synchronizes the GPU, so even this small opt-in operation can briefly
delay the pause menu. It is deliberately not a recurring performance monitor.
The active render target is restored, including on exceptions. A CPU sentinel
detects a readback that failed without throwing; that result is inconclusive.

Interpretation:

- `target`, `playerTarget`, and `materialTexture` should identify the same texture.
  `material` and `rendererMaterial` should also match. A mismatch narrows the
  problem to binding or material replacement, not decoder capability.
- Uniform white **samples**, with correct bindings, put suspicion on the video
  destination/copy path. They do not prove the whole frame is white or identify
  the responsible decoder operation; compare the source media at `videoTime`.
- Varied source-colored samples but a white view put suspicion downstream of the
  decoded target: shader sampling, material state or camera compositing.
- No sample line, an unsupported layout or an inconclusive readback is not proof
  of either a decoder failure or success.

The currently inspected Dynasty MP4 has visible scenery at 2 seconds. Its DAT's
1.25-second opacity animation explains the observed fade duration, not its color.

The implementation follows Unity's documented [RenderTexture readback and active
target restoration](https://docs.unity3d.com/2021.3/Documentation/ScriptReference/RenderTexture-active.html)
and [ReadPixels CPU-readback and synchronization contract](https://docs.unity3d.com/2021.3/Documentation/ScriptReference/Texture2D.ReadPixels.html).
This does not establish actual Quest rendering success until the headset test is
performed.
