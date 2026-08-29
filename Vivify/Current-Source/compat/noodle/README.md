# Noodle companion fixes for Murder Plot

Apply this to the Noodle Extensions Quest source you actually build/use:

```bash
python3 apply_noodle_murder_plot_fixes.py /path/to/NoodleExtensions
```

It makes two exact, conservative edits:

- note dissolve material selection: use the cutout-capable material whenever body or arrow visibility is `< 1`, including the fully hidden `0` boundary used heavily by Murder Plot;
- beatmap callback condition: add parentheses so an empty `beatmapOpt` cannot reach `.value()` merely because the beatmap pointer changed.

It also audits whether a V3 fake-object parser callback is present. It does **not** rewrite the fake-object injection lifecycle automatically, because branches differ and a guessed rewrite can duplicate thousands of fake notes. If runtime logcat still says `V3 fake objects were not pre-injected; using the compatibility fallback`, that lifecycle path should be fixed against the exact Noodle source branch.
