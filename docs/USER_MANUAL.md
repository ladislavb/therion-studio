# Therion Studio User Manual

Last updated: 2026-07-03

This guide covers everyday workflows in Therion Studio. It intentionally focuses on using the application, not on the full Therion language reference. Therion source syntax, command names, options, and serialized document content stay in canonical Therion form.

The application follows the operating system language when a bundled translation is available. English, Czech, and Slovak are included. Use `File -> Settings...` (`Preferences...` in the native macOS application menu) to override the application language. Language changes take effect after restarting Therion Studio.

## Contents

- [1. What Therion Studio Is](#1-what-therion-studio-is)
- [2. Main Window](#2-main-window)
- [3. Getting Started](#3-getting-started)
- [4. Text Editing](#4-text-editing)
- [5. Project Navigation and File Operations](#5-project-navigation-and-file-operations)
- [6. Visual Map Editing (`.th2`)](#6-visual-map-editing-th2)
- [7. Running Therion](#7-running-therion)
- [8. Settings](#8-settings)
- [9. Keyboard Shortcuts](#9-keyboard-shortcuts)
- [10. Help And About](#10-help-and-about)
- [11. Troubleshooting](#11-troubleshooting)

## 1. What Therion Studio Is

Therion Studio is a desktop editor for Therion cave-mapping projects. It provides:

- project file browsing
- generated output browsing
- text editing for `.th`, `.th2`, and Therion config files
- a read-only 3D viewer for compiled `.lox` artifacts
- a read-only report viewer for Therion SQL database export `.sql` files
- structure navigation for `survey`, `centerline`, `map`, and `scrap` objects
- visual map editing for `.th2` files
- an integrated Therion run console

Therion Studio does not include the external Therion compiler. Install Therion separately and configure the executable path in Settings if auto-detection is not enough.

### 1.1 Terminology

- `Project`: a folder Therion Studio is currently browsing, usually containing Therion source files, config files, backgrounds, and subfolders.
- `Config`: a Therion processing file such as `thconfig`, `thconfig.*`, or `*.thconfig`.
- `Survey`: Therion survey structure from `.th` source files.
- `Map`: a Therion `map ... endmap` object that references scraps.
- `Scrap`: one drawn map fragment inside a `.th2` file.
- `Point`, `Line`, `Area`: map objects stored inside a scrap.
- `Background`: a raster image, `.xvi`, or PocketTopo text export used as a drawing reference.
- `.xvi`: Therion's vector background/reference format, often generated from PocketTopo data.

## 2. Main Window

The main window contains:

- the menu bar (`File`, `Edit`, `View`, `Help`)
- a top command bar with project, save, editor, and map actions
- the left activity rail (`Structure`, `Outputs`, `Search`, `Validation`, `Compiler`) and a quick compile action
- document tabs in the center
- a status bar with map instructions, compile state, encoding, and map state

Common window actions:

- `File -> New Window` opens a new empty window. It does not copy the current project or open documents.
- `File -> New File -> Therion Source (.th)`, `Therion Map (.th2)`, or `Therion Config (.thconfig)` opens a new unsaved document in the active project. New `.th`, `.th2`, and `.thconfig` documents start with `encoding utf-8`. The toolbar `New Document` button opens the same choices. The first `Save` asks where to save it.
- `File -> Settings...` opens application settings.
- `View -> Expand Sidebar` / `Collapse Sidebar` shows or hides the left sidebar content.
- `View -> Expand Help`, `Expand Block Inspector`, or `Expand Map Inspector` controls the active right-side panel, depending on the current editor.
- `View -> Show Map Magnifier` / `Hide Map Magnifier` toggles the map magnifier overlay. This is only UI state and does not modify `.th2` files.
- `View -> Enter Full Screen` / `Exit Full Screen` toggles full-screen mode.

When a map pane is detached into a separate window, the main window can show both `Map Inspector` and `Help` controls because the visual map and raw source can be visible at the same time.

## 3. Getting Started

### 3.1 Quick Start

1. Start a new project with `File -> New Project -> Project from Template...` or `File -> New Project -> Empty Project...`, or open an existing project folder with `File -> Open Project...`.
2. Template projects are created from the bundled default template with `thconfig`, `index.th`, `surveys/survey1.th`, `scraps/scrap1.th2`, and an `output` directory for generated exports; Therion Studio opens the new project, selects its `thconfig` as the project `Target Config`, and opens `thconfig`, `index.th`, `surveys/survey1.th`, and `scraps/scrap1.th2` in tabs.
3. In `Compiler`, choose the `Target Config` (`thconfig`, `thconfig.*`, or `*.thconfig`) that should be used for project-level runs. If you are starting from scratch without a template, create a config file and at least one `.th` source file from `File -> New File`.
4. Open the `.th` source files that define surveys, centerline data, maps, and file references. Use `Raw` for direct Therion source editing or `Blocks` for supported structured edits.
5. Open an existing `.th2` map file from `Files`, or create one with `File -> New File -> Therion Map (.th2)`. Use `Visual` for map editing and `Raw` for direct `.th2` source edits.
6. In the map editor, insert a scrap before adding points, lines, or areas when the `.th2` file has no scrap yet. Draw map objects or use `Smart Area` to create a referenced Therion area from existing borders.
7. Make sure the config/source files reference the survey and map files needed for compilation, using normal Therion source syntax.
8. Save the documents, then run Therion from `Compiler` to check or export the project.

### 3.2 Open a Project

1. Select `File -> Open Project...`.
2. Choose the folder that contains the project's Therion files, config files, backgrounds, and subfolders.
3. Open documents from the `Files` pane.

The project folder chooser starts in your home folder when opened from `Open Project...`.

When no project is open, the welcome tab shows `New Empty Project`, `New Project from Template...`, and `Open Existing Project...` buttons, and the `Files` sidebar shows an empty state with the same project-opening action instead of browsing the computer filesystem. When a project is open but no document tab is active, the welcome tab offers opening a file from the sidebar.

`File -> New Project -> Project from Template...` opens a dialog with `Project Name` and `Location`, shows the resulting project folder path, and creates a project from the bundled default template. The dialog starts in the last successfully used project parent folder; if none is available, it starts in your `Documents` folder. The target project folder must not already contain files. The default template creates `thconfig`, `index.th`, `surveys/survey1.th`, `scraps/scrap1.th2`, and `output`; `index.th` defines the top-level `survey cave`, includes the survey and scrap files, and defines a map that references the scrap. The generated `thconfig` sources `index.th`, selects `cave`, and exports PDF and LOX outputs into `output`. After creation, Therion Studio opens `thconfig`, `index.th`, `surveys/survey1.th`, and `scraps/scrap1.th2`.

`File -> New Project -> Empty Project...` opens a dialog with `Project Name` and `Location`, shows the resulting project folder path, and starts in the last successfully used project parent folder or your `Documents` folder when no prior location is available. It creates the project folder and opens it as a new empty project. The target project folder must not already contain files. Use `File -> New File` or the `Files` pane context menu to add project files afterward.

The welcome tab and `File -> Recent Projects` list up to five recently opened projects. Select a project from either list to reopen it.

When a project is open, the welcome tab shows the active project name and path. It also lists up to ten recent files from that project; select a recent file to reopen it. The same project-scoped list is available from `File -> Recent Files`.

### 3.3 Open Documents

- Double-click a file in `Files`.
- `.th2` files open in the map editor.
- `.lox` files open in the 3D viewer.
- Therion SQL database export `.sql` files open in the SQL report viewer.
- Generated `.pdf` map or atlas outputs open in the system default application.
- `.th`, `thconfig`, `*.thconfig`, `thconfig.*`, `.log`, `.txt`, and ordinary text files open in the text editor.
- Unsupported files, such as images or PDF, show an `Unsupported file` message with `Open in External App`.

The 3D viewer is read-only. Its toolbar provides `Reset`, `Fit`, `Orthogonal Projection`, `Top View`, `Side View`, `Rotate Left`, `Rotate Right`, `Auto Rotate`, `Measure`, and `Export 3D Image` controls, left-drag orbit navigation, right- or middle-drag pan navigation, mouse-wheel zoom, arrow-key navigation, and rotate buttons that spin the view around the blue/world axis. When the 3D viewer tab is active, left and right arrow keys rotate around the world Z axis in 5-degree steps; up and down arrow keys adjust camera tilt in 5-degree steps. The rotate toolbar buttons use the same 5-degree step. `Fit` frames the loaded scene bounds using the current camera orientation and viewport shape. `Orthogonal Projection` switches between perspective and orthographic viewport projection without changing the scene data. The `Auto Rotate` toolbar button uses a play icon when stopped and a square stop icon while running; it rotates the view around the world Z axis. The `Export 3D Image` download action saves the current viewport as a PNG file using the current camera, visible layers, overlays, model coloring, and background. The export dialog lets you choose the image resolution from the current viewport size, 1920-pixel-wide, 3840-pixel-wide, or a custom size, with the current viewport aspect ratio locked by default to avoid distorted exports. The inspector panel follows the active application color palette, lets you switch `Model Coloring` between `Altitude` and `None`, adjust `Rotation Speed`, and precisely set `Compass`, `Tilt`, `Distance`, and `Focal Length`; focal length is disabled in orthographic projection because it only affects perspective projection. The `Scene Settings` section appears after `Layers` and lets you switch `Background` between `Black` and `White`, toggle `Show Bounding Box`, `Show Head-Up Display`, and `Show Title & Stats`; `Show Head-Up Display` controls the altitude legend, compass, view-angle indicator, and scale bar. `Altitude` is the default and colors centerline and meshes by model Z coordinate. `None` renders meshes in one neutral solid color adjusted for the selected background. Mesh surfaces use smoothed per-vertex lighting so cave shape remains readable on the selected viewport background. The layer controls use clean names without visible item counts, show centerline sublayers for shot classes found in the loaded `.lox` file, and show station sublayers only when the scene contains multiple entrance, fixed, or other station classes. By default, underground centerline, meshes, and surfaces are visible; stations, labels, surface centerline shots, splay shots, duplicate shots, and hidden shot sublayers start hidden. The top-level `Stations` checkbox is the master station-marker switch; station sublayers only narrow the visible station categories when `Stations` is enabled. Station markers and labels are shown only for stations attached to currently visible centerline shots, but `Labels` is independent from station marker visibility and can show names even when `Stations` is disabled. Station markers and fully qualified station labels are automatically decluttered in dense views. Hover a station attached to a visible centerline shot to see its full reference and model Z coordinate in a floating detail card near the cursor, even if station labels or markers are hidden. When an opened `.lox` file is regenerated on disk, the viewer reloads the scene automatically and preserves the current camera and inspector settings where practical. The viewport overlays `Underground Passages Length` and `Underground Depth` computed from underground centerline shots only, while the measurement tool is a toggle: click the ruler button, then click two stations on visible centerline shots to inspect their 3D distance, azimuth, and vertical difference; click the ruler button again or press `Esc` to leave measurement mode. The 3D canvas draws a red bounding box around the loaded 3D scene extent when bounds are available, and does not draw standalone world X/Y/Z axis guide lines. It overlays an altitude legend only when model coloring is set to `Altitude` and the head-up display is visible. Top View and Side View preserve the current camera heading. The compass uses the current camera heading in top and side views; the compass, right-side view-angle semicircle, and scale bar remain visible when scene bounds are valid, with the scale bar rendered as a simple line with end ticks beside that control cluster. The altitude legend includes multiple intermediate elevation labels. The view-angle needle moves through the upper half for above-scene views and through the lower half for below-scene views.

The SQL report viewer is read-only. Open a Therion `export database` `.sql` file from the `Files` pane to load it into an in-memory SQLite database. Import and query work runs in the background; while a new export is importing, query and preset controls remain unavailable and the status line shows the active import. Opening or reloading another export, running a newer query, or closing the report cancels superseded active database work. Read-only queries have a ten-second execution deadline; an expired query reports an error and the report remains usable for another query. The query editor above the result table runs one read-only `SELECT` or `WITH` query. The right sidebar has `Presets` and `Schema` sections: built-in presets fill the query editor and run predefined analyses for overview with explorer and surveyor counts plus total depth, survey lengths, exploration and surveying by person, yearly activity, recent activity, continuation stations, lead flags, entrances, and depth by survey, while schema lists imported tables and columns. Use the `Custom` preset section to save the current query as a user preset, rename a selected custom preset, or delete it; custom presets are stored in application settings and do not modify the opened `.sql` file. Mutating SQL statements are rejected, and very large result sets are capped in the viewer to keep the application responsive. The toolbar `Export CSV` download action writes the currently displayed result table to a CSV file. The save dialog suggests a project-specific timestamped filename such as `therion-studio-report-babice-20260704-143215.csv`.

### Outputs

Use the left `Outputs` rail item to browse generated Therion export artifacts in the open project. Outputs are grouped as `Model`, `Map / Atlas`, and `Database`. Entries show the filename for unique outputs; if multiple outputs in the same group share a filename, each duplicate adds its project-relative folder in parentheses, for example `map.pdf (_output)`. The list refreshes when a Therion run finishes and when project output files change on disk. Activate `.lox` outputs to open the internal 3D viewer, `.sql` outputs to open the SQL report viewer, and `.pdf` outputs to open them in the system default application. Therion `.3d` model outputs are not listed until Therion Studio provides a supported viewer or import workflow for them.

### 3.4 Create and Manage Files

Use `File -> New File` to create an unsaved `.th`, `.th2`, or `.thconfig` document in the active project and choose its path on first save. Right-click in the `Files` pane to create folders, create saved `.th`, `.th2`, and `.thconfig` files directly in the project, rename items, duplicate files, delete items, or open `.th2` files directly in the map editor.

Rename is blocked when the target file or folder is open in a document tab. Folder deletion is also blocked while related tabs are open. Deleting one open file asks for confirmation, closes its tab after any unsaved-change prompt is resolved, and then deletes the file.

### 3.5 Save Changes

- `File -> Save` saves the active tab.
- If the active tab has not been saved yet, `File -> Save` opens `Save As`.
- `File -> Save All` saves all modified tabs.
- Closing a dirty tab asks whether to save, discard, or cancel.
- If an open file changes on disk while its Therion Studio tab has no unsaved edits, Therion Studio reloads it automatically. If the tab has unsaved edits, Therion Studio asks whether to reload from disk or keep the in-memory version.

## 4. Text Editing

### 4.1 Raw and Blocks Modes

For `.th` and Therion config files:

| Mode | Use |
|---|---|
| `Raw` | Direct source editing with syntax highlighting, search, replace, autocomplete, and contextual help. |
| `Blocks` | Structured editing of supported commands and blocks, with the same command help metadata as Raw mode. |

New `.th` and Therion config tabs open in the default editor selected in Settings. `Raw` is the default. Switching to `Blocks` does not insert missing `encoding` directives and does not rewrite the source until you make an explicit edit.

The `Blocks` toolbox is filtered by document type:

| Document | Toolbox content |
|---|---|
| `.th` | Commands from the Therion Book chapter `Creating data files`. `.th2` map-object commands such as `scrap`, `point`, `line`, and `area` are hidden because they are edited in the map editor. |
| `thconfig`, `thconfig.*`, `*.thconfig` | Commands from `Processing data`, such as `source`, `select`, and `export`. |

For `.th2` files:

| Mode | Use |
|---|---|
| `Raw` | Direct text editing of the `.th2` source. |
| `Visual` | Visual map editing with canvas and inspector. |

### 4.2 Text Editing Features

- line numbers and active-line highlight
- command, option, value, and path autocomplete while typing; command suggestions are filtered for the current document type (`.th`, `.th2`, or Therion config)
- `Ctrl+Space` to open autocomplete manually
- In Raw mode, the top document toolbar shows `Format Document`. It is an explicit action: it replaces leading indentation with one tab per Therion block level, uses a tab display width of four spaces, and can be undone in one step. It does not run automatically and leaves blank rows, code-block body rows, line endings, and encoding unchanged.
- contextual help for the current command or option; Raw and Blocks show the same complete command help, positional arguments stay in documented order, options are listed alphabetically, and the help panel is titled with the current command or selected help target
- Raw editor diagnostics list issues such as malformed option tokens, unknown commands or options, commands used in the wrong document type or block context, missing arguments, unclosed blocks, duplicate IDs, unresolved object references, area references to lines missing from the current scrap, portable-path warnings for backslash separators in supported path tokens, and empty `line`/`area` object blocks inside scraps. Project validation additionally reports missing files referenced by `input` and `source` plus map/scrap/join/station references that the project index cannot resolve unambiguously, including `point station -name` references that do not resolve to survey data and `join` references to named line-point marks that are missing from the resolved line. `point ... station` objects without `-name` are allowed; they are not validated against survey stations. Quoted `input` and `source` paths, `./` prefixes, and Windows-style backslash separators are resolved like the same unquoted forward-slash relative paths, but `Apply Fix` can convert backslash path tokens to portable `/` separators; the preview shows the full source line with the selected path replaced. Opening or restoring a project, saving or editing a project document, and internal or external changes to project `.th`, `.th2`, or `thconfig` files refresh project validation in the background without switching panels, and `Validate Project` in the `Validation` panel manually refreshes all `.th`, `.th2`, and `thconfig` files in the open project. Project validation uses the same `Target Config` as the Structure and Compiler panes; if that target path is removed or renamed, Therion Studio clears the stale target and re-runs project discovery. Background refreshes keep the previous visible findings and rail severity until the new result is ready. Project-level findings appear first under `Project` and selecting them shows detail without opening a source file. The Validation rail icon, findings list, and Raw editor diagnostics distinguish warning-only results from results that contain errors. Inline wave underlines and subtle backgrounds in Raw mode are editor projections of the same validation findings shown in the panel or tooltip; normal syntax colors only identify token roles. The Help panel continues to show command documentation. Activate a file-level finding to jump to the affected raw source line, or use `Apply Fix` when the selected finding shows an explicit safe source edit. Applying a fix refreshes any open editor projection of that document, including the Map editor view for `.th2` files. Unclosed `.th2` map block fixes preview the insertion target, then insert the missing `endscrap`, `endline`, or `endarea` at the next safe map command boundary or at the end of the file. Empty scrap-object fixes show the source block that will be removed, remove only that selected block, and are never applied silently.
- `Export Markdown...` in the `Validation` panel saves the currently visible validation result as a Markdown report with a summary, grouped findings, source snippets, and safe-fix previews when available. The save dialog suggests a project-specific timestamped filename such as `therion-studio-validation-babice-20260704-143215.md`.
- a `Selection` inspector view in Blocks mode for editing the selected block header and supported inline options; the first panel is titled with the selected Therion command and shows its source line
- when no block is selected, the Blocks `Selection` inspector view shows `No block selected.`; when the fixed root `encoding` card is selected, it shows the command and encoding value as read-only text
- find and replace from the `Edit` menu
- `File -> Import -> Import PocketTopo Text...` is shown only when an existing or unsaved `.th` text document is active, and imports a PocketTopo Therion export (`.txt`) at the cursor as Therion `centreline` blocks
- a `File` inspector view with a panel titled by the current file name, full path, copy-path action, on-disk size, last-modified timestamp, current encoding, and UTF-8 conversion for non-UTF-8 files

### 4.3 Blocks Data Rows

In `Blocks` mode, `data ...` blocks can be edited through a table based on the active data header. Empty body lines are ignored when the table opens, so spacing in the source does not become fake measurement data.

## 5. Project Navigation and File Operations

### 5.1 Project Search

Open the Search activity from the left rail or press `Command/Ctrl+Shift+F`. Enter literal text, choose `Whole word` or `Case sensitive` when needed, and press `Enter` or `Search` to scan the current project.

Project search scans Therion text sources (`.th`, `.th2`, and Therion config files), includes unsaved edits from open tabs, and lists matches grouped by file with line and column locations. Double-click a file or match row to open the file at the matching text with the inline find bar ready for next/previous navigation. `.th2` matches open as map-editor documents with the Raw workspace active, so you can switch back to the visual map editor afterward.

### 5.2 Project Navigation Pane

The `Structure` activity opens the project navigation pane with a compact `Files` / `Survey` / `Map` selector.

`Files` shows the project folder tree and supports file navigation and file/folder context-menu actions. `Survey` shows the survey namespace and definition hierarchy for `survey`, `map`, and `scrap` objects and recognizes both Therion centerline spellings: `centreline` and `centerline`. `Map` shows map composition: top-level maps, child maps, and referenced scraps.

`Survey` and `Map` remember their expanded and collapsed tree state separately while the same project remains open.

Select a row to inspect it in the tree. Double-click a source row, or select it and press `Enter`, to open its source document and navigate to the matching line.

Within each parent, rows are grouped as surveys, maps, then scraps, and each group is sorted alphabetically by the displayed name. Warning rows appear after the project objects they relate to.

The index uses the selected `Target Config` when it points inside the opened project. Without an explicit target config, Therion Studio uses the first root config found in this priority order: `thconfig`, `thconfig.thconfig`, `main.thconfig`, `index.thconfig`, then `<project_name>.thconfig` where `<project_name>` is the opened project folder name. If none of those exists and exactly one named root config exists (`*.thconfig` or `thconfig.*`), that file is used. If several config files are possible, choose the intended `Target Config` in the `Compiler` pane. Renaming, deleting, creating, or externally changing project source/config files refreshes the shared project index; if the selected `Target Config` no longer exists, it is cleared so discovery can run again.

In the `Survey` view, maps and scraps remain under the survey namespace where they are defined, even when another map references them as part of map composition. In the `Map` view, maps and scraps are shown under the maps that reference them. Unresolved or ambiguous map composition references appear as warning rows that navigate to the source line. The Therion compiler remains the authoritative validator for export behavior.

## 6. Visual Map Editing (`.th2`)

### 6.1 Modes and Inspector

`Visual` mode contains:

- the map canvas
- inspector selector views: `Selection`, `Objects`, `Backgrounds`, `File`

`Raw` mode remains available for direct source editing.

`Visual` and `Raw` are two views of the same `.th2` source. Visual edits write Therion commands back to the file. Raw remains available for direct editing and advanced Therion constructs. Switching views does not create a second document.

Non-UTF-8 files are opened with a concrete source encoding when it can be resolved, including common Central European legacy encodings such as ISO-8859-2. When such a file is saved, Therion Studio keeps the resolved source encoding unless you explicitly convert the file to UTF-8 from the `File` inspector.

### 6.2 Navigation

Use navigation controls before drawing or editing map objects.

| Action | Control |
|---|---|
| Pan | Hold `Space` and drag with the left mouse button, hold `Ctrl` and drag with the left mouse button, or drag with the right mouse button. Precision devices such as trackpads and Apple Magic Mouse pan with two-finger or surface scrolling. |
| Zoom in / out | Use the toolbar `Zoom In` / `Zoom Out` buttons, use a non-precision mouse wheel, or hold `Command/Ctrl` while scrolling. |
| Fit map geometry | Use `Fit` to fit the drawn map objects into the viewport. |
| Fit map geometry and backgrounds | Use `Fit With Background` to include raster and `.xvi` background layers in the fitted viewport. |
| Pan with scrollbars | Use the horizontal and vertical scrollbars when they are visible. |

`Space` temporarily switches the map canvas to pan while it is held, without leaving the current map tool. `Ctrl` + left drag and right-button drag use the same pan behavior. A right-click, or `Ctrl` + left click without dragging, opens the map object context menu instead.

Therion Studio treats precision scrolling devices as pan controls by default, so trackpads and devices such as Apple Magic Mouse can pan horizontally and vertically. A conventional mouse wheel zooms by default.

### 6.3 Main Map Tools

| Tool group | Actions |
|---|---|
| Navigation | `Zoom In`, `Zoom Out`, `Fit`, `Fit With Background` |
| Selection and drafting | `Select`, `Complete Draft` |
| Insertion | `Insert Scrap`, `Point`, `Line`, `Freehand`, `Area`, `Smart Area` |

These map controls are enabled when the embedded `.th2` workspace is in `Visual` mode. In `Raw` mode, the controls stay visible but inactive because the raw source editor is active instead of the map canvas. When the map pane is detached, use the toolbar in the detached map window for map zooming and drawing.

The map canvas uses a stable light paper-style surface in both light and dark application modes. Toolbars, tabs, and inspectors follow the system appearance, but raster backgrounds, `.xvi` references, and map symbols are not inverted or tinted for dark mode.

### 6.4 Insert Objects

- `Point`: click once in the map.
- `Line`: click vertices, then press `Enter` or `Complete Draft`.
- `Area`: click vertices, then press `Enter` or `Complete Draft`.
- `Smart Area`: click inside a closed face made by existing lines in the same scrap, review the preview, use `[` / `]` to switch alternatives when multiple faces match, then press `Enter` or `Complete Draft`.
- `Freehand`: press, drag, and release to insert a simplified Bezier line.
- `Insert Scrap`: creates a new scrap immediately, selects it in `Selection` and `Objects`, then lets you edit its ID/projection before adding points, lines, freehand lines, or areas.

Completing a map insertion keeps the current map position and zoom. Use `Fit` or `Fit With Background` when you want to reframe the viewport.

On supported pen-tablet and stylus devices, stylus tap and drag are accepted as primary canvas input for selecting objects and inserting points, line vertices, area vertices, and freehand strokes. Mouse and trackpad input remain available without changing tools.

Starting `Point`, `Line`, `Freehand`, or `Area` activates `Inspector -> Selection` before the first point or vertex is placed. Set type, subtype, ID, point name, label text, or supported point value there before committing the new object. The editor remembers recent type/subtype choices separately for new points, lines, and areas, uses the latest choice as the next default, persists those recent choices across application restarts, and shows them as compact quick buttons in `Recent` below the symbol preview. During new line or area drawing, `Options` stays available and `Line Point` controls the next vertex before it is placed; `<<`, `Smooth (-smooth)`, and `>>` control the next vertex's Bezier handles and smooth state, and `Subtype` writes a line-point subtype override from that vertex onward. `line slope` also exposes its supported line-point `Orientation (-orientation)` and `Left size (-l-size)` controls before each click. Empty line-point subtype leaves that vertex without a subtype override, and line-point subtype is cleared for each new line or area draft. After a point, line, or area is committed while its drawing tool stays active, `Selection` remains on the next pending object so type, subtype, recent choices, and other draft fields are still editable for repeated drawing. Single-key map drawing shortcuts are ignored while a text field, combo editor, spin box, or raw source editor has focus. Repeated `point station` insertion advances the next `Name (-name)` by incrementing the last numeric station segment before any `@survey` suffix, and also advances trailing station-letter suffixes such as `1a -> 1b` and `1z -> 1aa`; changing the pending point type away from `station` clears that pending station name instead of carrying it into other point types. If a scrap or an object inside a scrap was selected when you started the tool, the new object is inserted into that scrap; the pending metadata line shows the target scrap ID. Use `Insert into` to choose a different existing target scrap before committing. When the file has no scrap yet, `Selection` shows a highlighted notice that the new object will create the draft scrap before the first object is committed.

When a `.th2` has no XTherion `xth_me_area_adjust` metadata, the first committed map insertion writes stable `xth_me_area_adjust` and `xth_me_area_zoom_to` header lines so later drawing does not remap after the new geometry appears.

Existing Therion `area ... endarea` blocks that reference `line -id ...` borders render from lines in the same scrap. Border lines may be open; when their intersections form a closed face, Therion Studio fills that face without changing the referenced line source.

`Smart Area` creates that referenced-area form instead of drawing new border geometry. Confirmation may add missing IDs to the referenced boundary lines so the new `area ... endarea` block can refer to them, but it does not change their geometry. After confirmation the map returns to Select mode. Press `Esc` to cancel the preview.

While placing points or drafting lines and areas, nearby object vertices are highlighted as snap candidates. The active snap target is highlighted more strongly; Bezier control handles remain free-form.

When the map editor is active, the left side of the status bar shows the current workflow instruction, including drawing steps and Enter/Esc completion hints. The compact map mode badge uses `M: Select` or `M: Insert`.

While drafting a line or area:

- click to add a straight vertex
- press-drag-release while placing a vertex to pull that vertex's Bezier handle pair, matching XTherion-style curve entry
- drag visible Bezier control handles before committing to refine the draft curve
- click the first line vertex again to finish a closed line (`-close on`); click the first area vertex again to commit the area draft
- closed lines render the final segment back to the first vertex, including two-point closed Bezier curves
- double-click while drafting a line to add the clicked vertex, commit the line, and return to Select mode
- press `Backspace` or `Delete` to remove the last draft vertex
- press `Esc` to commit a sufficiently complete line or area draft and return to Select mode; incomplete drafts are canceled

### 6.5 Common Map Workflows

- Create a new map: insert a scrap, set its ID/projection in `Selection`, then choose point, line, freehand, area, or Smart Area tools.
- Draw a wall: choose `Line`, set type `wall` before the first vertex if needed, click vertices, drag while placing a vertex to create Bezier handles, then press `Enter`.
- Create a referenced area: choose `Smart Area`, click inside the intended closed face, use `[` / `]` if several candidates are available, then press `Enter`.
- Edit a line shape: switch to `Select`, click the line, then click a vertex or Bezier handle and drag or edit the `Line Point` controls.
- Add drawing reference data: open `Backgrounds`, add a raster image, `.xvi`, or PocketTopo `.txt`, then adjust visibility, position, opacity, or Gamma.

### 6.6 Source Changes From Map Actions

- Point tools write `point ...` commands inside the target scrap.
- Line and freehand tools write `line ... endline` blocks. Freehand strokes are simplified into Bezier coordinate rows while preserving proportionally more anchors for curved or detailed strokes.
- Manual Area writes a generated closed `line border -id ... -close on` and an `area ... endarea` block that references that border line.
- Smart Area writes an `area ... endarea` block that references existing boundary lines. It may add missing `-id` values required for those references, but it does not change existing line geometry.
- Background insertion writes XTherion-compatible image metadata such as `##XTHERION## xth_me_image_insert`. The first map insertion in a file without XTherion view metadata may also write `xth_me_area_adjust` and `xth_me_area_zoom_to`.

### 6.7 Edit Geometry

Select a map object or one of its vertices/control handles in the canvas. The `Selection` inspector then shows the relevant controls, including the source line and enclosing scrap ID.

Map objects keep their normal rendered colors while editing. In select mode, the map canvas uses a crosshair cursor; the object under the cursor hotspot is highlighted in cyan before selection, and the selected object is highlighted in red.

Selection is shared between the map canvas, the `Objects` pane, and the `Selection` inspector. Selecting an area also highlights its referenced border lines. Selecting a line vertex or Bezier handle switches the inspector to the corresponding line-point controls.

Right-click a map object or line vertex without dragging to open a context menu with common XTherion-style actions. Empty canvas right-clicks do not open this menu. The menu mirrors the available `Selection` inspector groups, such as type/subtype choices, editable object fields, `Options`, the complete available `Line Point` panel, `Line Point Actions`, and `Object Actions`; free text or numeric editors are opened and focused in the inspector. On macOS, the trackpad secondary click, such as a two-finger click, opens the same menu. If the menu is already open, another secondary click on a different object or vertex retargets the menu to that new selection and moves it to the latest click position.

For lines and area borders:

- select a vertex to edit its line-point details
- drag a line or area vertex near another object to reveal that object's vertices as snap candidates; the active snap target is highlighted more strongly, and Bezier handles remain unsnapped
- right-click a line segment and use `Insert Point Here` to split the nearest segment at the click position
- use `Insert Before` / `Insert After` to add vertices near the selected vertex
- use `Extend Before` / `Extend After` at line endpoints to continue an existing line
- use `Delete` / `Backspace` to remove the selected line vertex; if no line vertex is selected, `Delete` / `Backspace` deletes the selected object
- when deleting a line vertex that has additional line-point options (for example `altitude .` or `subtype ...`), Therion Studio asks for confirmation before deletion
- use `<<` and `>>` to enable or remove incoming/outgoing Bezier handles
- drag Bezier handles directly on the canvas to reshape the curve

When a map drag moves a point, vertex, or Bezier handle from integer coordinates to a fractional position, Therion Studio writes the new fractional coordinates with additional decimal precision so repeated edits and smooth Bezier tangents do not visibly degrade.

If a line is used as an area border, some destructive line actions are blocked; select or edit the owning area instead. Deleting an area removes only the `area ... endarea` block and keeps referenced border lines in the source.

### 6.8 Edit Object Properties

In `Inspector -> Selection`, you can edit properties for selected `Scrap`, `Point`, `Line`, or `Area` objects.

- `Scrap` shows ID in the main section, `Projection` in `Options`, and a separate `Scrap Scale` section for XTherion/Therion-compatible `-scale [...]` calibration values.
- `Point`, `Line`, and `Area` keep the main section focused on identity and preview (`ID`, `Type`, `Subtype`, `Preview`). Additional editable fields such as `Name (-name)`, `Text (-text)`, `Value (-value)`, clipping, alignment, and projection appear in `Options`. Choose the empty subtype value, or `No subtype` in the context menu, to remove an existing `-subtype`.
- Inline Therion subtype syntax such as `wall:debris` renders the same as the equivalent explicit `-subtype debris` form in both the map canvas and the Selection preview.
- `Edit All Object Settings...` at the end of `Options` opens the full catalog-backed option editor for the selected `scrap`, `point`, `line`, or `area` command. Positional attributes such as point `x`/`y`, line `type`, and scrap `id` are shown as protected attribute rows, while `-id`, `-text`, `-orientation`, and other options remain editable option rows.
- `point label` and `line label` expose `Text (-text)`. Point labels render near the point; line labels render along the label line path, so the line controls the text length and orientation.
- supported point types such as `height`, `passage-height`, `altitude`, `dimensions`, and `date` expose `Value (-value)`. Bracketed Therion values such as `[fix 1300]` are preserved.
- Point types that support `-orientation` show an orientation override and a draggable orientation handle. Station names stay screen-aligned for readability.
- Point objects expose `Align (-align)` when selected. Choose `Default` to remove the explicit align option, or choose a Therion alignment such as `top-left`, `center`, or `bottom-right`.
- Line, area, and point objects that support Therion clipping expose `Disable clipping (-clip off)`. Turning it off removes the explicit `-clip` option instead of writing `-clip on`.
- selected line vertices expose dedicated `Line Point` controls for supported per-vertex options. `Subtype` is shown for line types with segment subtypes, `Orientation (-orientation)` and `Left size (-l-size)` are shown for slope line points, and `Altitude (auto)` is shown for wall line points and writes `altitude .`.
- selected line vertices also expose an `Additional line-point options` editor in `Selection` for remaining per-vertex standalone options such as `altitude`, `subtype`, `direction`, or `adjust`, so these can be edited without switching to Raw mode. Rows managed by visible dedicated controls are hidden from this editor.
- `Additional line-point options` edits are applied automatically when the field loses focus (no separate Apply/Clear buttons).
- line vertices with `altitude` or `subtype` line-point rows display a subtle metadata ring even when the line is not selected, plus a stronger ring around the active vertex handle. The vertex tooltip still previews all additional line-point rows.

The `Preview` row shows how the selected or pending object will look. The preview uses a light map-like background even in dark mode.

### 6.9 Objects and Backgrounds

In `Inspector -> Objects`, you can select objects, reorder objects by drag/drop, toggle visibility in the current view, and delete objects with confirmation. By default, manually collapsed scraps stay collapsed across object-tree refreshes and text-cursor navigation. Selecting a map object in a collapsed scrap expands that scrap so the selected object is visible. Turn on `Auto-collapse/expand Scraps` in this tab when you want the inspector to keep only the selected object's scrap expanded and collapse the other scraps automatically.

In `Inspector -> Backgrounds`, you can:

- add, remove, and reorder raster, SVG, `.xvi`, or PocketTopo `.txt` background layers
- show/hide individual layers
- edit layer position and opacity
- edit layer X/Y scale and rotation; `Lock proportions` keeps X and Y scale equal by default
- set a rotation pivot by clicking `Set Pivot` and then clicking the desired center in the map; for raster and SVG layers the click may be inside or outside the visible background, the selected layer's pivot is shown in the map while `Backgrounds` is active, and `Reset Pivot` restores the default pivot
- adjust `Gamma` for raster layers (`.xvi` and SVG use fixed Gamma)

The map canvas can pan to the full extent of every visible background layer, including portions moved or transformed outside the original map frame. Moving, scaling, or rotating a layer does not change map-object coordinates.
When a visible background is the only drawing content, it remains the drawing surface without the empty-document canvas or its no-geometry message over it.

Raster background layers keep their full image resolution, so they stay sharp as you zoom into the map instead of becoming blurry. Very large scans are bounded to a high internal display size to limit memory use.

Therion Studio also reads and writes Mapiah `##MAPIAH## image_insert_v1` background metadata for `format=xvi`, `format=raster`, and `format=svg` layers, including rotation, X/Y scale, and pivot. This allows rotated PocketTopo/XVI, raster, or SVG background references created in Mapiah to appear in the map editor, and lets Therion Studio upgrade a simple XTherion background reference to Mapiah metadata when you rotate, scale, or set the pivot for a layer. SVG references render as SVG layers using Mapiah intrinsic size and source viewBox metadata for placement. When you add an SVG background, Therion Studio derives that metadata from the SVG root `width`, `height`, and `viewBox` attributes.

A background is restored as map content only when the current TH2 source declares it through XTherion or Mapiah metadata. Local session settings may restore its display state, but cannot add a background to an otherwise unrelated or empty TH2 file.

When you add a PocketTopo Therion export (`.txt`) as a map background, Therion Studio asks for XVI scale, resolution, grid spacing, and plan or extended-elevation projection. It writes a generated `_p.xvi` or `_e.xvi` file next to the PocketTopo export, adds that `.xvi` as the background layer, and stores XTherion-compatible image metadata in the `.th2` source.

Therion Studio does not generate a separate metric grid. Use background layers, especially `.xvi`, for reference grid content.

### 6.10 Detached Map Window

Use `Separate Map` to move the visual map pane into its own window, for example on a second monitor. Use `Return Map` or close the detached map window to reattach it.

## 7. Running Therion

Open the `Compiler` pane from the activity rail. The pane description identifies it as the place to run Therion for the current project or active config.

### 7.1 Main Fields

| Field | Meaning |
|---|---|
| `Arguments` | Additional arguments for the current session. |
| `Run Target` | `Current Config` or `Project Config`. |
| `Target Config` | Config file for project-level runs. |
| `Working Directory Override` | Optional override for the run directory. |

Set the Therion executable path in `File -> Settings...`. If no explicit path is set, Therion Studio tries `therion` and platform auto-detection.

Before starting a compile, Therion Studio saves all modified open document tabs. If any open document cannot be saved, the compile is canceled and the runner is not started.

The selected target config is shown as a prominent file name with its full path below it. Starting a new compile clears the previous compiler output before Therion is launched, then prints the command and working directory at the top of the new output.

Therion runs non-interactively from Studio. If Therion prints a prompt such as `Press ENTER to Exit!` after an error, Studio closes the process input stream so the run can finish without pressing `Stop`.

Closing a project clears `Target Config` and `Working Directory Override` because those paths are project-specific. Renaming or deleting the selected `Target Config` inside the project also clears the stale target and refreshes project discovery. The Therion executable path is a global preference. Additional arguments are session-only.

### 7.2 Actions

- `Run Therion`
- `Stop`
- `Clear Output`
- `Copy Output`

The status bar shows compile state with the compact `C:` prefix: `C: Idle`, `C: Running`, `C: OK`, or `C: Failed`. `C: OK` is used only when the Therion process exits successfully and Studio does not detect known output-write errors in stderr, such as `warning -- error writing`.

## 8. Settings

Open settings from `File -> Settings...` or `Preferences...` on macOS.

Settings include:

- application language (`System Default`, English, Czech, Slovak)
- Therion executable path
- default editor for newly opened `.th` and Therion config tabs (`Raw` or `Blocks`)
- automatic full-project validation after project, document, and file changes
- troubleshooting logs for 24 hours, with actions to open the log folder or clear existing logs

Troubleshooting log changes take effect after restarting Therion Studio. Logs are written to the application log folder, rotated automatically, and the UI preference expires after 24 hours so logging cannot stay enabled indefinitely by accident. When enabled, the log includes startup, session-restore, document-restore, and project-open timing checkpoints that can help diagnose slow application launch. Developer environment overrides such as `THERION_STUDIO_ENABLE_LOG=1` remain explicit launch-time overrides and are not controlled by the 24-hour UI preference.

## 9. Keyboard Shortcuts

Use `Command` on macOS and `Ctrl` on Windows/Linux unless the platform menu shows a different native shortcut.

| Action | Shortcut |
|---|---|
| New window | `Command/Ctrl+N` |
| Open project | `Command/Ctrl+O` |
| Save | `Command/Ctrl+S` |
| Save all | shown in the `File` menu |
| Close all tabs | `Command/Ctrl+Shift+W` |
| Quit | `Command/Ctrl+Q` |
| Undo | `Command/Ctrl+Z` |
| Redo | `Command/Ctrl+Shift+Z` or platform default |
| Find | `Command/Ctrl+F` |
| Search in project | `Command/Ctrl+Shift+F` |
| Find and replace | platform default replace shortcut |
| Close find/replace bar | `Esc` when the find/replace bar is open |
| Switch to Raw editor | `Command/Ctrl+top-row 1` |
| Switch to Blocks editor for `.th` / config, or Visual editor for `.th2` | `Command/Ctrl+top-row 2` |
| Manual completion popup (text editor) | `Ctrl+Space` |
| Pan map | `Space` + left mouse drag; `Ctrl` + left mouse drag; right mouse button drag; precision scrolling device such as trackpad or Apple Magic Mouse |
| Zoom map | Toolbar `Zoom In` / `Zoom Out`; non-precision mouse wheel; `Command/Ctrl+scroll` |
| Fit map geometry | Toolbar `Fit` |
| Fit map geometry and backgrounds | Toolbar `Fit With Background` |
| Start point drawing | `P` |
| Start line drawing | `L` |
| Start area drawing | `A` |
| Complete the current map draft | `Enter` |
| Cancel map insertion/drawing | `Esc` |
| Delete the selected map object or selected line point; while drawing, delete the last draft point | `Delete` / `Backspace` |
| Toggle selected line reverse | `R` |
| Toggle selected line closed; while drawing a line, complete it as closed | `C` |
| Toggle selected or next line point smooth | `S` |
| Toggle selected or next line point previous control handle | `,` |
| Toggle selected or next line point next control handle | `.` |

Map-editor drawing and source-edit operations keep the most recent 200 undo steps per map editor tab.

## 10. Help And About

- `Help -> User Manual` or `User Manual` on the Welcome tab opens the localized manual in the application. The manual viewer keeps a table of contents visible on the left, supports the table-of-contents links in this document, and includes in-manual search with previous/next navigation. The manual search field is focused when the manual opens, and `Command/Ctrl+F` focuses it again.
- `Help -> About Therion Studio` shows version, build label, Qt version, platform, GitHub repository, license, maintainer, and third-party notices. On macOS, About is also available from the native application menu.

## 11. Troubleshooting

### 11.1 Therion executable not found

Fix:

- set a valid full path in `File -> Settings... -> Therion executable`
- verify executable permissions

### 11.2 Config cannot be resolved

Fix:

- set `Target Config`, or
- open the desired Therion config and run using `Current Config`, or
- verify the working directory / override path

### 11.3 Rename or delete is blocked

Fix:

- close tabs that reference the target file or folder, then retry

### 11.4 Map background looks wrong

Fix:

- select the layer in `Backgrounds`
- check layer visibility, position, opacity, and Gamma
- for `.xvi`, verify the `.xvi` file and referenced `.th2` are from the same coordinate context
- Mapiah rotated/scaled background metadata is supported for `format=xvi`, `format=raster`, and `format=svg`; SVG backgrounds require Mapiah intrinsic size and source viewBox metadata
- a raster layer that still looks soft when zoomed in has reached the resolution of its source image; use a higher-resolution scan for more detail

### 11.5 I can zoom but cannot pan the map

Fix:

- hold `Space` and drag with the left mouse button
- hold `Ctrl` and drag with the left mouse button
- drag with the right mouse button, or use trackpad / precision-device scrolling
- hold `Command/Ctrl` while scrolling if your precision device is currently zooming
- use the horizontal and vertical scrollbars when visible

### 11.6 Area is not visible

Fix:

- verify the `area ... endarea` block is inside the same `scrap` as the referenced `line -id ...` borders
- check that every referenced ID exists and is unique in that scrap
- for open border lines, check that their intersections form a closed face
- check layer/object visibility in `Objects`

### 11.7 Smart Area cannot find a candidate

Fix:

- click inside a face bounded by lines in the same scrap
- zoom in and click away from line intersections or ambiguous edges
- ensure the intended boundary lines intersect or connect closely enough to form a closed face
- if more than one candidate is shown, use `[` / `]` to choose the intended one before confirming

### 11.8 A line cannot be deleted

Fix:

- if the line is referenced by an area, delete or edit the area first
- deleting an area removes only the `area ... endarea` block and keeps referenced border lines

### 11.9 Project search opened a `.th2` match in Raw

Fix:

- this is expected for text search results so the matching source line is visible
- use the editor mode switch or `Command/Ctrl+top-row 2` to return to the visual map editor
