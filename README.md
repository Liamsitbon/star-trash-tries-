# Beat Saber Quest Visual Mods Workspace

Development workspace for experimental and porting work around advanced **Beat Saber Quest** visuals.

> This repository contains multiple related projects and test code. Some components are experimental and may not be ready for normal gameplay use.

## Projects

| Directory | Purpose |
| --- | --- |
| [`Vivify/`](./Vivify/) | Quest-side Vivify development and compatibility work |
| [`Nexora/`](./Nexora/) | Nexora development and integration work |
| [`NE-Fixed/`](./NE-Fixed/) | Noodle Extensions fixes and compatibility work |
| [`Cinema/`](./Cinema/) | Cinema-related Quest development/porting work |
| [`tests/`](./tests/) | Tests and experimental validation |
| [`.github/`](./.github/) | GitHub workflows and repository configuration |

## Project Goals

This workspace is used to investigate and improve advanced Beat Saber visual features on Quest, including:

- custom environments and cinematic visuals
- Noodle Extensions compatibility
- Vivify runtime behavior
- Nexora 360° video environments
- Cinema/video functionality
- performance and stability testing
- map compatibility testing

## Repository Layout

```text
.
├── .github/
├── Cinema/
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

The official standalone Nexora source is maintained separately:

[Nexora Official Source](https://github.com/Liamsitbon/Nexora-Official-Source)

Nexora uses synchronized pre-rendered 360° video environments to create cinematic Beat Saber maps while reducing the amount of complex environment geometry that must be rendered in real time.

## Development Notes

When contributing or testing changes:

1. Keep changes scoped to the relevant project directory.
2. Avoid committing generated build output or macOS metadata such as `.DS_Store`.
3. Test runtime changes on the intended Quest/Beat Saber version.
4. Document compatibility-breaking changes.
5. Keep experimental changes separate from known-working builds when possible.

## Disclaimer

These projects are unofficial Beat Saber modding projects and are not affiliated with or endorsed by Beat Games or Meta.
