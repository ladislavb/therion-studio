# Therion Studio User Manual

Last updated: 2026-05-26

This guide covers end-user workflows in Therion Studio.

## 1. What Therion Studio Is

Therion Studio is a desktop editor for Therion cave-mapping projects. It provides:

- project file browsing
- text editing for `.th`, `.th2`, and `thconfig`
- structure navigation (`Survey` / `Map` / `Scrap`)
- visual map editing for `.th2` files
- integrated Therion run console

## 2. Main Window

The app window has four main areas:

- menu bar (`File`, `Edit`, `Map`, `View`, `Window`, `Help`)
- top command bar with quick actions (`Open Project`, `Close Project`, `Save`, editor/map tools)
- left sidebar with activity icons (`Files`, `Structure`, `Compiler`) and a `Compile` action
- center tab area for documents
- bottom status bar (app state, compile state, encoding, and map status)

The main window opens at a usable desktop size and clamps restored session geometry only enough
to avoid unusably narrow windows while still allowing common half-screen layouts.

## 3. Getting Started

### 3.1 Open a Project

1. Select `File -> Open Project...`.
2. Choose your project folder.
3. Use the `Files` pane to open documents.

When no project is open, the `Welcome` tab shows an `Open Project...` button.
When a project is open but no document tab is active, the `Welcome` tab shows `Open file from sidebar`.

### 3.2 Open Documents

- Double-click a file in `Files`.
- `.th2` files open in the map editor.
- Supported text files open in the text editor (`.th`, `.thconfig`, `.log`, `.txt`, and generic plain-text files).
- Unsupported files (for example images or PDF) show an `Unsupported file` message with `Open in External App`.

### 3.3 Save Changes

- Use `File -> Save` for the active tab.
- Use `File -> Save All` to save all modified tabs.

## 4. Text Editing

### 4.1 Modes

For `.th` and `thconfig` files:

- `Raw` mode: direct text editing with syntax highlighting, find/replace, and autocomplete
- `Blocks` mode: structured editing of supported commands and blocks

For `.th2` files:

- `Raw` mode: text editing
- `Visual` mode: map editing with canvas + inspector

### 4.2 Useful Text Features

- line numbers and active-line highlight
- command/option/value autocomplete while typing
- `Ctrl/Cmd+Space` to open autocomplete manually
- find and replace via `Edit` menu
- unsupported file encodings can be converted to UTF-8 with confirmation

## 5. Structure and File Operations

### 5.1 Structure Pane

The `Structure` pane shows detected `Survey`, `Map`, and `Scrap` hierarchy from open content. Selecting an item opens/synchronizes to its source location.

### 5.2 Project Tree Actions

In the `Files` pane you can:

- create files/folders
- rename files/folders
- duplicate files
- delete files/folders
- open `.th2` directly in map editor

If a target file/folder is currently open in tabs, rename/delete is blocked for safety.

## 6. Visual Map Editing (`.th2`)

### 6.1 Modes and Inspector

`Visual` mode includes:

- map canvas
- inspector tabs: `Selection`, `Objects`, `Backgrounds`

`Raw` mode stays available for direct source editing.

### 6.2 Main Map Tools

Toolbar actions include:

- zoom: `Zoom In`, `Zoom Out`, `Fit`, `Fit With Background`
- selection/drafting: `Select`, `Complete Draft`
- insertion: `Insert Scrap`, `Point`, `Line`, `Freehand`, `Area`

Canvas rendering is zoom-aware: geometry strokes and edit handles are reduced at distant zoom levels so overview remains readable. Point objects are drawn as circular dots by default, but survey stations (`station` points) are uniquely rendered as small yellow triangles pointing upwards, making them easily distinguishable.
Line objects render as a single styled stroke; there is no separate secondary detail-stroke layer.
Area fills support configurable pattern overlays from the bundled `resources/map_object_styles/*.json` style files via `fill_pattern` (`hatch`, `cross_hatch`, `dots`) with spacing/angle/stroke or dot parameters.
User style overrides are loaded after bundled styles from the `map_object_styles` directory in the application data location. On macOS this is `~/Library/Application Support/Therion Studio/map_object_styles/`. Override files are partial JSON objects named by object kind and Therion type, for example `area.water.json`, `line.wall.json`, or `line.wall.presumed.json`; `area.json`, `line.json`, and `point.json` override defaults for that kind. Default files are applied first, then type files, then subtype files, so the most specific override wins.

### 6.3 Drawing Basics

- `Point`: click to insert a point
- `Line`: click vertices, then press `Enter` or `Complete Draft`; click the first vertex again to finish as a closed line (`-close on`)
- `Area`: click vertices, then press `Enter` or `Complete Draft`
- `Freehand`: press-drag-release to insert a line

While drafting line/area:

- `Backspace`/`Delete` removes the last draft vertex
- `Esc` exits insert mode (and commits only when draft is sufficiently complete)

### 6.4 Selection and Object Editing

In `Inspector -> Selection`, you can edit properties for selected `Scrap`, `Point`, `Line`, or `Area`, including common fields like ID/type/subtype where supported.

In `Inspector -> Objects`, you can:

- select objects
- reorder objects by drag/drop
- toggle visibility in the current view
- delete objects (with confirmation)

### 6.5 Detached Map Window

Use `Separate Map` to move the visual map pane into its own window (for multi-monitor workflows). Use `Return Map` or close that window to reattach.

### 6.6 Background Images and Grid

In `Inspector -> Backgrounds`, you can:

- show/hide metric grid and adjust spacing
- add/remove/reorder background layers (raster images and `.xvi`)
- adjust per-layer visibility, position, and opacity
- adjust gamma for raster layers (`.xvi` uses fixed gamma)

## 7. Running Therion

Open the `Compiler` pane from the activity rail.

### 7.1 Main Fields

- `Executable`
- `Arguments`
- `Run Target` (`Current Config` or `Project Config`)
- `Target Config`
- `Working Directory Override`

### 7.2 Actions

- `Run Therion`
- `Stop`
- `Clear Output`
- `Copy Output`

The status bar shows compile state (`Idle`, `Running`, `OK`, `Failed`).

## 8. Keyboard Shortcuts

Platform key substitution follows Qt defaults (`Ctrl` on Windows/Linux, `Command` on macOS where applicable).

| Action | Shortcut |
|---|---|
| New window | Platform default `QKeySequence::New` |
| Open project | Platform default `QKeySequence::Open` |
| Save | Platform default `QKeySequence::Save` |
| Save all | Platform default `QKeySequence::SaveAs` |
| Close app | Platform default `QKeySequence::Quit` |
| Undo | Platform default `QKeySequence::Undo` |
| Redo | Platform default `QKeySequence::Redo` |
| Find | Platform default `QKeySequence::Find` |
| Find and replace | Platform default `QKeySequence::Replace` |
| Completion popup (text editor) | `Ctrl+Space` |
| Close all tabs | `Ctrl/Cmd+Shift+W` |
| Open current `.th2` in map editor | `Ctrl/Cmd+Alt+Shift+G` |
| Split selected map line segment | `Insert` or `I` |
| Delete selected map middle anchor | `Delete` or `Backspace` |
| Toggle selected map vertex smooth/corner | `S` |

## 9. Troubleshooting

### 9.1 Therion executable not found

Symptom:

- error that the Therion executable path is missing or not executable

Fix:

- set a valid full path in `Compiler -> Executable`
- verify executable permissions

### 9.2 Config cannot be resolved

Symptom:

- compile target/config cannot be resolved

Fix:

- set `Target Config`, or
- open the desired `.thconfig` and run using `Current Config`, or
- verify working directory/override path

### 9.3 “Open Current Document in Map Editor” is disabled

Symptom:

- map-open command is unavailable

Fix:

- activate a `.th2` tab first

### 9.4 Rename/delete is blocked in project tree

Symptom:

- rename or delete action is refused

Fix:

- close tabs that reference the target file/folder, then retry
