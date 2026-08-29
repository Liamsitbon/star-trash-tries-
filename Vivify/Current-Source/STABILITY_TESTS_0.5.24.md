# Vivify Quest 0.5.24 verification matrix

- [ ] Play Murder Plot, exit normally, select another song, and start it without a crash.
- [ ] Repeat after pausing/resuming before exit.
- [ ] 42 Flux eclipse scene: no white frame and no permanent full-screen moon occlusion.
- [ ] 42 Flux NotesCam/TV/Tile sections: camera-isolated visual effects appear.
- [ ] Murder Plot: no horizontal repeated-note trails and no doubled stock/custom notes.
- [ ] Start both maps from a later practice time.
- [ ] Seek backward once, then finish or exit the song.

Expected log markers:

- `Vivify ResetRuntime: transition=true`
- `Vivify CreateScreenTexture registered as transient Multiview texture`
- `Vivify Blit accepted by transient Multiview path`
- `Vivify Blit skipped ... unsafe Multiview chain` only for unsupported effects
