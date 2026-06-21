# Therion Studio

Therion Studio is a cross-platform Qt desktop editor for
[Therion](https://therion.speleo.sk/)  – cave surveying software.

## Motivation

Therion Studio aims to modernize the Therion GUI with a Qt-based, cross-platform application that
is easier to maintain and more comfortable on modern desktop systems, especially macOS.

## Features

- Multi-file Therion project editing with tabbed documents.
- Raw editor for `.th`, `.th2`, and `.thconfig` files.
- Syntax highlighting, auto-completion, and contextual help derived from the Therion Book.
- Synchronized Raw and Visual map editing for TH2 files.
- Visual TH2 map editor with source-linked points, lines, areas, scraps, and background images.
- Structured Block editor for more approachable `.th` and `.thconfig` authoring.
- Read-only `.lox` 3D viewer with layer controls, measurement, station hover details, and altitude coloring.
- Project structure and map-object navigation sidebars.
- Integrated Therion runner for current or project `.thconfig` compilation.
- Detached map window for multi-monitor workflows.
- Runtime light/dark appearance support.

## Screenshots

[![Visual TH2 map editor](docs/screenshots/map_editor_small.png 'Visual TH2 map editor')](docs/screenshots/map_editor.png)
[![Detached multi-window map editing](docs/screenshots/multi_window_mode_small.png 'Detached multi-window map editing')](docs/screenshots/multi_window.jpeg)
[![Project validation panel](docs/screenshots/validator_small.png 'Project validation panel')](docs/screenshots/validator.png)
[![Project search panel](docs/screenshots/search_small.png 'Project search panel')](docs/screenshots/search.png)
[![LOX 3D viewer](docs/screenshots/3d_viewer_small.png 'LOX 3D viewer')](docs/screenshots/3d_viewer.png)

## Status

The project is under active development. Its development has been orchestrated with significant help from AI-assisted software engineering tools.

See [`SPECIFICATION.md`](SPECIFICATION.md) for detailed
requirements.

## Installation

Installers and packages are published through
[GitHub Releases](https://github.com/ladislavb/therion-studio/releases). Packaging details and
local build instructions are maintained here:

- Windows installer notes: [`packaging/windows/README.md`](packaging/windows/README.md)
- Linux package notes (`.deb` + `AppImage`): [`packaging/linux/README.md`](packaging/linux/README.md)
- Homebrew tap and formula: [`ladislavb/homebrew-therion-studio`](https://github.com/ladislavb/homebrew-therion-studio)
- Source build instructions: [`docs/BUILDING.md`](docs/BUILDING.md)
- Release process checklist: [`docs/RELEASING.md`](docs/RELEASING.md)

Release tags use CalVer, for example:

```text
v2026.5.0
```

## Notes

Therion Studio does not bundle the external Therion compiler. Install Therion separately and
configure the executable path in Settings if the default `therion` command or platform fallback
detection is not enough.

## License

Therion Studio is licensed under the GNU General Public License v3.0 or later
(`GPL-3.0-or-later`). See [`LICENSE`](LICENSE).

Third-party notices are in
[`docs/THIRD_PARTY_NOTICES.md`](docs/THIRD_PARTY_NOTICES.md).
