# Beat Saber Quest Visual Mods Workspace

Development workspace for experimental and porting work around advanced **Beat Saber Quest** visuals.

> This repository contains multiple related projects and test code. Some components are experimental and may not be ready for normal gameplay use.

## Projects

| Directory | Purpose |
| --- | --- |
| [`Vivify/`](./Vivify/) | Quest-side Vivify development and compatibility work |
| [`Nexora/`](./Nexora/) | Nexora development and integration work |
| [`NE-Fixed/`](./NE-Fixed/) | Noodle Extensions fixes and compatibility work |
| [`tests/`](./tests/) | Tests and experimental validation |
| [`.github/`](./.github/) | GitHub workflows and repository configuration |

## Project Goals

This workspace is used to investigate and improve advanced Beat Saber visual features on Quest, including:

- custom environments and cinematic visuals
- Noodle Extensions compatibility
- Vivify runtime behavior
- Nexora 360° video environments
- performance and stability testing
- map compatibility testing

## Repository Layout

```text
.
├── .github/
├── NE-Fixed/
├── Nexora/
├── Vivify/
├── tests/
├── .gitignore
└── README.md
```

## Status

This is primarily a **development workspace**, not a single finished mod release. Individual directories can have different build requirements, supported Beat Saber versions, and stability levels.

Before using or building a component, check its project files and configuration rather than assuming every directory targets the same version.

## Nexora

Nexora uses synchronized pre-rendered 360° video environments to create cinematic Beat Saber maps while reducing the amount of complex environment geometry that must be rendered in real time.

It remains a standalone Quest mod, not a PC mod or an automatic Vivify-map converter. Ordinary RGB media stays supported; authored RGBD is optional and experimental. Where available, the external-video menu pairs a local video with an installed song without importing or moving it; copying to a map is a separate confirmed action.

See the [Nexora overview and rendering limitations](./Nexora/Current-Source/README.md). In particular, the Direct Video path bypasses several shader effects, video frame rate is not headset refresh rate, and supported game versions/dependencies belong to each package's manifest and release notes rather than this overview.

## Development Notes

When contributing or testing changes:

1. Keep changes scoped to the relevant project directory.
2. Avoid committing generated build output or macOS metadata such as `.DS_Store`.
3. Test runtime changes on the intended Quest/Beat Saber version.
4. Document compatibility-breaking changes.
5. Keep experimental changes separate from known-working builds when possible.

## Disclaimer

These projects are unofficial Beat Saber modding projects and are not affiliated with or endorsed by Beat Games or Meta.
