# Therion Studio Qt Reimplementation Specification

## 1. Purpose

This document defines the functional scope and implementation requirements for reimplementing Therion Studio as a cross-platform desktop application using Qt.

It is intended to be implementation-grade: the requirements below should be specific enough that a Qt team can reproduce the current workflows and expected user-facing behavior without reverse-engineering the Swift source.

This document is written as a software requirements specification (SRS): normative requirements use mandatory language, acceptance criteria are testable, and implementation notes are clearly separated from requirements.

Conventions used in this document:

- "shall" means mandatory behavior
- "should" means a strong recommendation, not a hard requirement
- "may" means optional behavior
- "MVP" means the minimum feature set required for the first cross-platform release
- "Post-MVP" means functionality that can be scheduled after the first usable Qt release

The target platforms are:

- macOS
- Windows
- Linux

The reimplementation should preserve the current product behavior and user workflows while replacing the native SwiftUI/AppKit implementation with a Qt-based desktop application.

## 2. Product Summary

Therion Studio is a desktop application for working with Therion cave-survey projects. It combines a project browser, a Therion text editor, a graphical TH2 map editor, a structure sidebar, and a Therion command runner.

The application is primarily used to:

- browse Therion project files and folders
- edit Therion source files with syntax highlighting and assistance
- visually inspect and edit TH2 map data
- inspect compiled 3D `.lox` artifacts in a read-only viewer tab
- inspect Therion SQL database exports in a read-only report tab
- review survey structure and map object relationships
- run the Therion command-line tool and inspect its output

### 2.1 Editing Model Principles

- Therion project artifacts shall remain plain-text source documents (`.th`, `.th2`, `thconfig`, `*.thconfig`, and `thconfig.*`) as the canonical source of truth.
- All editor surfaces shall serialize user-visible changes back into plain-text Therion source without introducing a separate binary/project-only model.
- Shared source projections shall include a lossless logical-command layer over physical source lines so continuation lines, block body rows, command rows, and source ranges are interpreted consistently by validation, syntax assistance, structured Blocks, Map projections, and project indexing.
- The TH2 visual map workspace shall be treated as a specialized editing surface for geometry-intensive `.th2` workflows that are difficult to author directly in raw text.
- The structured Blocks workspace should prioritize approachability and guided editing for newer users by reducing syntax burden while preserving explicit source round-trip behavior.
- Raw text editing shall remain available for expert/direct editing and for operations that require exact line/token-level control.

## 3. Functional Scope

### 3.1 Project Browser

The application shall provide a project browser for navigating a Therion project tree.

Required capabilities:

- open a project folder
- create a new empty project folder from `File -> New Project -> Empty Project...`
- create a new project from a bundled template when no project exists or from `File -> New Project -> Project from Template...`
- display project files and subfolders
- recognize common Therion-related file types such as `.th`, `.th2`, `thconfig`, `*.thconfig`, and `thconfig.*`, plus compiled 3D `.lox` artifacts and Therion SQL database export `.sql` files
- provide an `Outputs` project navigation surface for generated Therion export artifacts grouped by export kind
- open files in editor tabs
- opening a `.lox` file shall create a read-only 3D viewer tab with Reset, Fit, orthogonal-projection toggle, Top/Side view, rotate-left/rotate-right, automatic-rotation, measurement, and PNG image export controls, plus orbit, pan, zoom, arrow-key yaw/tilt navigation, precise camera-orientation, distance, and focal-length adjustment, and world-blue-axis rotation navigation; Fit shall frame the current 3D scene bounds using the active camera orientation and viewport aspect ratio
- the 3D viewer shall let the user switch model coloring between altitude-based coloring and no model coloring
- the 3D viewer shall color centerline shots using the same altitude-based or uncolored palette as the meshes
- the 3D viewer shall render a bounding box around the loaded 3D scene extent
- the 3D viewer shall let the user switch the scene background between black and white without changing altitude-based model coloring
- the 3D viewer shall show a screen-space compass and scale bar when scene bounds are available, and shall show the altitude legend only when model coloring is set to Altitude
- opening a Therion SQL database export `.sql` file shall create a read-only report tab that loads the export into an in-memory database, places a read-only `SELECT` query editor above the result table, offers predefined centreline-analysis query presets, exposes the imported table schema, and can export the current result table to CSV without modifying the source export file
- support file selection, multi-tab workflows, and recent project reopening
- preserve folder expansion state where practical

Project-template requirements:

- The MVP shall include a bundled default project template stored as application resources under `project_templates/default`.
- The default template shall materialize a plain-text project containing `thconfig`, `index.th`, `surveys/survey1.th`, `scraps/scrap1.th2`, and an `output` directory for generated exports.
- `index.th` shall define a top-level `survey cave`, include `surveys/survey1.th` and `scraps/scrap1.th2`, and define a plan map containing the generated scrap.
- `thconfig` shall use `index.th` as source, select the top-level survey/map target, and define PDF and LOX exports.
- Creating a project from the default template shall use a project-creation dialog with explicit `Project Name` and `Location` inputs, shall preview the resulting project folder path, should default the location to the last successfully used project parent directory or the platform documents directory when no prior project-creation location is available, shall require the target project folder to be absent or empty, shall materialize only template-declared files and directories, shall open the generated project, shall select the generated `thconfig` as the project runner target config, and shall open the primary source/map files declared by the template.
- Creating an empty project shall use the same project-creation dialog with explicit `Project Name` and `Location` inputs, shall preview the resulting project folder path, should default the location to the last successfully used project parent directory or the platform documents directory when no prior project-creation location is available, shall require the target project folder to be absent or empty, shall create that folder, and shall open it as the active project without creating template files.
- The welcome tab shown when no project is open shall expose `New Empty Project`, `New Project from Template...`, and `Open Existing Project...` actions.

### 3.2 Text Editor

The application shall provide a text editor for Therion source files.

Required capabilities:

- open multiple documents in tabs
- edit Therion text files
- syntax highlighting for Therion syntax
- code folding for structured blocks
- completion suggestions for Therion commands, options, and values
- search and replace within the current file and, where feasible, across the project
- provide an inline find/replace bar with next/previous navigation, replace current/all, and common search options
- show contextual Therion help/documentation for the token or command at the current caret location when metadata is available
- source command/option/value metadata from a versioned structured catalog generated from Therion documentation snapshots, with deterministic fallback when optional metadata is unavailable
- caret/selection synchronization with the map editor when a TH2 file is open
- show document path and text encoding status for the active file
- allow explicit conversion of non-UTF-8 text documents to UTF-8
- preserve unsaved edits and document state across tab changes
- provide an editor-surface mode switch between raw text editing and a structured block-canvas view for supported Therion source files
- keep raw text as the canonical source of truth; any structured-view mutation shall be applied through source edits with immediate source synchronization
- when the active document supports structured Blocks mode (`.th` or Therion config files) in the main window, the `Raw`/`Blocks` mode selector shall be hosted in the full-width document command toolbar row directly above the tab strip

Structured block-canvas requirements:

- the structured mode shall be available for supported file types and shall remain optional per file type
- initial scope shall support `.th` and Therion config documents (`thconfig`, `*.thconfig`, and `thconfig.*`)
- the structured mode shall expose a toolbox of compatible Therion block/command templates that can be inserted via drag and drop into the canvas
- the structured-mode toolbox shall filter command templates by document domain using generated Therion Book metadata: `.th` documents shall show commands from "Creating data files", Therion config documents shall show commands from "Processing data", and `.th` toolbox templates shall not expose `.th2` map-object commands (`scrap`, `point`, `line`, `area`) because those are edited through the TH2 map editor
- the structured-mode toolbox shall provide a scope filter with `Auto` as default; `Auto` shall derive insertion scope from the currently selected canvas block context and manual scope selection shall persist until changed by the user.
- the structured-mode toolbox shall include a first-class `comment` insertion item that inserts full-line comments in source
- the structured mode shall render parsed structure cards in source order with parent-child nesting for supported directives
- inside `map ... endmap` blocks, non-command body lines that reference a scrap or nested map shall be rendered as first-class `Object Reference` cards even when the referenced object cannot yet be resolved
- `Object Reference` cards shall serialize as the original single map-body line containing the target object name, not as a Therion command token
- `Object Reference` cards shall be insertable only inside `map` blocks, shall support drag/drop reordering with other map children, and shall preserve unresolved target names for round-trip safety
- map-body commands such as `break` shall be exposed in the structured-mode toolbox from generated command-catalog context metadata, not from Block Editor hardcoded command lists
- catalog-backed leaf commands without positional arguments, such as map `break`, shall not require or display a synthetic value field in Block Details
- in Therion config structured mode, top-level configuration directives such as `select`, `export`, and `unselect` shall be rendered as leaf cards at document root when present in source.
- directives that support both inline and block forms (for example `source`) shall open nested scope only in explicit block form; inline single-line form shall remain a leaf card.
- in structured mode for `.th` and Therion config documents, an existing `encoding` directive shall be treated as a fixed document-root directive; opening a file or switching editor modes shall not auto-insert a missing `encoding ...` line or otherwise mutate source text, and an existing `encoding` card shall not be insertable from toolbox, movable, or deletable.
- The Blocks inspector view that edits the currently selected block shall be titled `Selection` to align with the Map editor inspector convention.
- The first section in the Blocks `Selection` view shall use the selected command token or object-reference label as its section title and shall show source location as a compact `Source line N` metadata line instead of a generic `Block Details` / `Command: ...` header.
- When no canvas block is selected, the Blocks `Selection` view shall show an explicit empty state message (`No block selected.`) rather than an empty panel.
- When the fixed document-root `encoding` card is selected in Blocks mode, the Blocks `Selection` view shall show its command title, source line, and encoding value in non-editable read-only text form; interacting with that value shall not mark the document dirty.
- Block Details for editable blocks shall expose an always-visible optional inline comment field that maps to end-of-line Therion comments and preserves comments on line rewrites.
- Block Details shall auto-commit inline field and option edits on editing completion or focus-out; it shall not require or expose a separate `Apply` button.
- Block Details shall expose only workflow-specific action buttons that cannot be represented as safe inline fields; for MVP this means `Edit Data Rows...` for `data` block body rows. The Blocks inspector shall not introduce a separate `Actions` section for these controls.
- structured block cards should visually indicate presence of inline comment and expose the comment text on hover.
- selecting or configuring a structure card shall mutate the underlying source text through the same safe-edit pipeline used by raw mode
- when no canvas block is selected, the third column shall keep the Blocks inspector visible with its explicit empty selection state while toolbox-command contextual preview remains available through the `Help` view.
- in Blocks mode, contextual help focus shall stay at command/parameter level while editing options; selecting an option row shall not permanently replace command-level help with option-only help.
- dragging a structure card in the canvas should reorder the corresponding source block; for container directives, reordering shall move the full block span including nested lines
- dropping a toolbox command below the lowest canvas block shall append it at document top-level end; dropping above the first block shall insert at document start.
- when a toolbox drop lands in an incompatible nested scope, insertion should auto-promote to the nearest valid ancestor scope (or root) instead of failing immediately.
- toolbox insertion templates shall not prefill example/sample argument values; inserted directives shall start with empty parameters for explicit user entry.
- container blocks should render explicit paired boundary guides in the canvas: a vertical connector from the container header toward closure and a visible closure boundary marker (end-pair intent).
- drag/drop targeting should treat the container boundary marker as a valid `after block` drop target, while dropping on the container body should still support `inside block` insertion.
- block-canvas presentation shall react to runtime application appearance changes (light/dark and palette/style updates) without requiring tab reload or application restart.
- centerline-oriented configuration flows should support quick insertion of common child commands (for example `team`, `explo-date`, and empty `data` header skeletons) without requiring manual raw-text typing
- data-block configuration should separate header editing and row editing: Block Details edits the `data ...` header (`style` + `readings order`), while the row editor dialog focuses on body rows/directives using the active header as schema
- data-block header editing in the Block Details pane should expose separate `style` and `readings order` fields and shall serialize them back as `data <style> <readings order>`
- data-block `readings order` editing should use tokenized tag semantics (add token chips, remove individual chips) so completion accepts/replaces only the active token and preserves previously entered readings tokens
- data-block `readings order` tag editing shall prevent duplicate chips and shall not double-insert a token when completion popup acceptance keys (`Enter`/`Tab`) are used
- data-block `readings order` tag editing shall remain usable in narrow inspectors: existing tokens shall render as removable pill chips that wrap within the available field width, and the add-token input shall stay on a separate row after the chips instead of clipping horizontally
- data-block `readings order` tag chips shall be removable only through their explicit remove control; pressing Backspace in an empty add-token input shall not remove an existing readings token
- Block Details option editing should provide catalog-backed suggestions for option keys/values while remaining free-form to allow unknown but valid Therion options.
- when catalog metadata defines explicit allowed values for an option, Block Details validation shall reject values outside that set before source rewrite.
- when catalog metadata defines fixed option value arity greater than one, Block Details shall present one parameter field per required value label and shall enforce exact value count validation before source rewrite.
- when a fixed-arity option is edited through parameter fields, serialization shall preserve Therion token boundaries (including quoting of values containing spaces) so round-tripped options remain arity-correct.
- data-block configuration should render measurement rows in a table derived from the active `data ...` field definition so row editing follows the declared column schema
- data-block row editor should not duplicate a second editable header/column-definition input; it should use the currently active `data ...` header as the single source of row-column schema
- data-block row editor shall ignore blank body lines that contain no data tokens and no comment so source spacing is not parsed as editable measurement data
- data-block row editor should expose a trailing `Comment` column for every row (measurement or directive) so inline row comments are first-class editable data
- data-block row editor should support explicit comment-only rows and preserve them as standalone comment lines in serialized source
- data-block row editor should provide inline directive suggestions/templates in the `Directive` column to reduce typing errors
- row-type-specific editability shall be enforced in the row editor (`Directive` locked for `data`/`comment` rows, measurement-value columns locked for `directive`/`comment` rows)
- row editor should expose a row-type selector with explicit `Data` / `Directive` / `Comment` values
- when existing row tokens no longer align with active data schema, row editor should present an explicit schema-mismatch warning before committing row edits
- data-block row editor should present data-field column labels in a consistent humanized form while preserving raw Therion token values during serialization
- data-block table presentation should auto-size visible column widths from the active field definition and should provide enter-key row flow where pressing Enter in the last column advances to a new row at column one
- data-block configuration should support mixed ordering of measurement and non-tabular directive rows (for example `extend ...`) in one structured row sequence without introducing one dedicated UI button per possible Therion command
- data-block editor dialog should auto-expand horizontally up to available host/screen space so all configured table columns remain visible when practical
- operations that require exact text caret semantics (for example line/column navigation and find/replace focus jumps) shall switch to raw mode automatically
- The text editor shall provide an explicit PocketTopo Therion export (`.txt`) import action that inserts converted Therion `centreline` content into the active open `.th` text document at the current cursor position.
- PocketTopo text import shall be exposed from `File -> Import -> Import PocketTopo Text...` only when the active document is a `.th` text document, shall be undoable as a single text insertion, and shall not silently create a new `.th` document, replace the active document, or open the `.txt` export as a project source file.

### 3.3 TH2 Map Editor

The application shall provide a graphical editor for TH2 map documents.

Required capabilities:

- render the current TH2 document as an editable 2D map
- support an embedded map workspace inside the main editor for TH2 files
- support a mode-based TH2 workspace in the main editor with explicit `Raw` and `Visual` modes
- support detaching and reattaching the map pane into a dedicated top-level window without losing synchronization with the source document
- support the main TH2 object types:
  - scraps
  - lines
  - points
  - areas
- support object selection, hover feedback, and visibility toggles; selected map objects shall use a red interaction overlay, hover-before-selection feedback shall use an azure/cyan interaction overlay without changing the normal object style color, and the map canvas shall use a crosshair cursor in select mode so the visible cursor hotspot matches hit testing
- support dragging and reordering objects where the document model allows it
- support direct editing of map geometry
- support undo and redo for all map mutations
- keep the graphical selection synchronized with the text source where possible
- keep the text editor selection synchronized with the graphical selection where possible
- support zooming, panning, and viewport restoration
- support an XTherion-style map canvas context menu on right-click or platform secondary-click gestures such as macOS two-finger trackpad click for the current map selection; repeated secondary-clicks while the context menu is open shall retarget the menu to the newly hit object or vertex and show it at the latest click position
- support fitting the viewport to geometry only or to geometry plus background layers
- support background imagery and sketch references used by TH2 documents, including multiple layers and `.xvi` vector references
- `.xvi` vector background references loaded from `##XTHERION## xth_me_image_insert` metadata shall follow XTherion placement semantics: when a referenced root station resolves in the `.xvi` station table, the station shall be aligned to the metadata base position; otherwise the metadata base position shall anchor the `.xvi` grid origin.
- `.xvi` vector background references shall apply the XTherion placement offset in TH2 model coordinates and then render `XVIgrid`, `XVIshots`, and `XVIsketchlines` through the same TH2 scene projection as map geometry; they shall not be independently fitted like raster preview imagery.
- `.xvi` station table rows shall be interpreted using XTherion field semantics, where the first two fields are station coordinates and the third field is the station name used for root-station matching; when duplicate station names occur, root-station placement shall use the first matching station table row, matching XTherion.
- support freehand line drawing with simplified bezier output
- support automatic input behavior for mouse, touchpad, stylus, and platform touch gestures

Map editor editing workflows should include, at minimum:

- point placement and point property edits
- line drawing and line vertex manipulation
- area editing and area assignment
- scrap-level object grouping
- object deletion and reorder operations
- line closing/opening where applicable
- background layer insertion and adjustment
- finishing an in-progress line or area draft from the toolbar or equivalent command surface
- selection of individual vertices or control points when editing geometry
- point, line, and area insertion shall offer viewport-scale snapping to nearby vertices or point anchors from other map objects, shall highlight candidate vertices on the nearby object for orientation, and shall highlight the active snap target while snapping is offered; Bezier control handles remain free-form
- dragging an existing line or area vertex shall offer viewport-scale snapping to nearby vertices or point anchors from other map objects, shall highlight candidate vertices on the nearby object for orientation, and shall highlight the active snap target while snapping is offered; Bezier control handles remain free-form
- context-menu access to common selected-object and selected-line actions, mirroring the available `Selection` inspector groups for type/subtype edits, editable object fields, `Geometry`, the complete available `Line Point` panel, `Line Point Actions`, and `Object Actions`; unavailable groups and actions shall be omitted from the menu rather than shown disabled, and free text or numeric editors shall be opened/focused in the inspector rather than embedded in the native menu

### 3.4 Object Settings / Inspector Panels

The application shall expose object-specific settings for selected map objects.

Required capabilities:

- inspect and edit scrap properties
- inspect and edit line properties
- inspect and edit point properties
- inspect and edit area properties
- inspector fields for `scrap`, `point`, `line`, and `area` shall be metadata-driven from the Therion command catalog (same option schema used by structured block details) rather than hardcoded per-command forms
- show object-specific identifiers and metadata
- reflect the current selection in the settings panel
- validate required fields and prevent invalid edits where necessary
- for point symbols and selected line-point anchors, the inspector shall provide explicit orientation override controls with enable/disable state and degree input constrained to canonical `0..<360`; enabling the override or changing the degree value shall immediately update the source and map canvas without a separate apply action, including explicit value `0`, and shall preserve the currently selected point or line anchor after the source rewrite
- orientation controls shall be gated by command-metadata applicability (type/subtype constraints from the generated Therion command catalog, including source-derived option exclusions) and shall not be shown for unsupported object variants
- explicit point `-orientation` shall rotate the rendered point symbol around its anchor using Therion orientation convention (`0` north/up, `90` east/right); style-driven labels shall remain screen-aligned by default, except label styles that opt into orientation-driven text rotation
- selecting a point symbol that supports `-orientation` shall show a draggable orientation handle in the map canvas when the point has an explicit `-orientation` value, including `0`; dragging the handle shall write the point command's `-orientation` option using the same source rewrite path as the inspector
- for selected `line slope` anchor vertices, the inspector shall provide `l-size` controls with enable/disable state and positive numeric input; changing either state or value shall immediately update the source and map canvas without a separate apply action, and shall serialize the option as an XTherion-compatible line-point row attached to the selected coordinate
- line vertices with attached `altitude` or `subtype` line-point rows shall show a subtle always-visible metadata ring at the vertex and a stronger metadata ring around the active vertex handle when the line-point handle is shown; other line-point rows such as `smooth`, `orientation`, `l-size`, `direction`, and `adjust` shall remain editable/visible where supported but shall not trigger that vertex metadata ring

### 3.5 Project Navigation Sidebar

The application shall provide a project navigation sidebar that combines project files and Therion structure projections.

Required capabilities:

- display project files and folders in a `Files` tab
- display only surveys, maps, and scraps in the Structure sidebar hierarchy
- allow navigation through the project hierarchy
- show selection state and the active document context
- provide a readable overview of the current project structure
- preserve user-expanded/collapsed tree state independently for the `Survey` and `Map` structure views across project-structure refreshes and view switches while the same project remains open

### 3.6 Therion Runner and Console Output

The application shall provide a way to execute the Therion command-line tool and inspect its output.

Required capabilities:

- configure or discover the Therion executable path
- run Therion from the current project context
- show the selected config name/path, effective working directory, project-config working-directory override, and additional command-line options
- expose Therion executable path configuration in the Settings dialog rather than the Compiler sidebar
- order the primary compiler setup fields as arguments, run target, target config, and working-directory override
- show the resolved config path directly below the target-config selector and the effective working directory directly below the working-directory override as subdued helper text, without separate section headers
- allow clearing the project-config working-directory override so the runner returns to automatic selected-config-folder behavior
- provide a folder chooser beside the working-directory override and place the override reset action below the effective working-directory value
- capture stdout, stderr, and exit status
- display command output in an application console view
- expose run status and allow copying console output
- allow clearing the visible console output without changing runner settings or the last compiler status
- optionally reveal the working directory or output directory in the platform file manager after a successful run
- show actionable error messages when execution fails
- keep the UI responsive while Therion is running

### 3.7 Session and State Persistence

The application shall preserve the user workflow across launches where practical.

Required capabilities:

- reopen the last project
- restore the last project using a platform-appropriate persisted access mechanism where the operating system requires it
- maintain a recent-projects list with up to five most recently opened project folders
- expose recent projects on the welcome surface when no project is open and in the `File` menu
- restore previously opened tabs
- restore the active tab where possible
- preserve editor state such as selection and viewport state where appropriate
- persist help/inspector visibility where they materially affect the active workflow
- persist user-visible preferences that affect navigation and editing behavior

### 3.8 Detailed Behavioral Rules

The rules below define the expected day-to-day interaction model. If a later requirement is more specific than an earlier one, the more specific rule wins.

#### 3.8.1 Project Browser Behavior

- Clicking a file row shall open supported files in the editor area and mark the opened document as active; unsupported file types shall not open in the internal editor and shall show an explicit unsupported-file prompt with an option to open externally.
- Clicking a folder row shall select that folder and toggle its expanded state.
- Clicking a folder row that is already selected shall collapse it and clear the folder selection.
- Clicking empty space in the project browser shall clear the current file and folder selection.
- The project browser shall preserve folder expansion state for the lifetime of the open project.
- The project root context menu shall offer creation actions for new folders, `.th` files, `.th2` files, and `thconfig` files.
- A supported Therion file row shall be shown with a Therion document icon; an unsupported file shall use a generic document icon.
- A `.th2` file shall offer an explicit "Open in Map Editor" action.
- A `.lox` file shall open in a read-only 3D viewer tab and shall not be offered for map editing.
- A Therion SQL database export `.sql` file shall open in a read-only SQL report tab and shall not be offered for text editing by default.
- The SQL report tab shall import Therion SQL export statements into an in-memory SQLite database, reject unsupported import statements, validate that the expected Therion export tables are present, and report import failures without modifying the original `.sql` file.
- SQL report import and query execution shall run outside the UI thread. A newer file load or query request shall cancel superseded active work and shall supersede older pending presentation results so stale schema or table data cannot replace the current report state. Read-only report queries shall have a bounded execution deadline, and closing the report tab shall interrupt active database work so teardown remains bounded.
- The SQL report tab shall provide predefined query presets for survey overview including explorer and surveyor counts plus total depth, survey lengths, exploration by person, surveying by person, survey and exploration activity by year, recent activity, continuation stations, lead flags by survey, entrances, and depth by survey where the imported schema supports them.
- Built-in SQL report presets shall be data-driven from bundled application resources rather than hardcoded directly in UI code.
- Activating a SQL report preset shall copy that preset query into the visible query editor and run it, so the user can inspect and modify the SQL before running a related custom query.
- The SQL report tab shall allow users to save, rename, and delete user-defined SQL query presets without modifying the opened `.sql` export file.
- User-defined SQL report presets shall be stored as per-user application settings and shall be shown separately from bundled built-in presets.
- The SQL report tab shall show query presets and imported schema in a right-side sidebar with separate `Presets` and `Schema` sections.
- The SQL report tab shall allow custom queries only when they are a single read-only `SELECT` or `WITH` query, shall reject mutating statements, shall cap displayed result rows to keep the UI responsive, and shall present results through a read-only table model rather than constructing per-cell editable widgets.
- The SQL report tab shall expose CSV export as the relevant toolbar action for SQL report documents, shall export the currently displayed result table as CSV on explicit user request, and shall prefill the export filename with the project name and a timestamp.
- The left activity rail shall provide an `Outputs` pane for generated Therion export artifacts grouped by export kind: supported model outputs (`.lox`), map/atlas outputs (`.pdf`), and database outputs (`.sql`).
- The `Outputs` pane shall identify artifacts by canonical file path and shall not deduplicate by filename; files with the same name in different directories shall remain separate entries. Unique filenames should be displayed as filenames, while duplicate filenames shall include project-relative folder context such as `name.ext (relative/folder)`.
- Project output discovery shall reuse shared project file-discovery traversal and skip rules rather than maintaining a separate ad hoc recursive scan in the UI.
- The `Outputs` pane shall refresh after a Therion run finishes and after relevant project filesystem changes so newly exported artifacts become visible without reopening the project.
- Activating an output artifact shall open supported artifacts with the appropriate internal viewer where available (`.lox` model viewer, `.sql` report viewer) and shall open `.pdf` outputs with the system default application. Unsupported model formats such as Therion `.3d` shall not be shown in `Outputs` until the application provides a supported viewer or import workflow for them.
- When an opened `.lox` file changes on disk, the 3D viewer shall reload the scene automatically where practical and shall preserve the current camera and inspector settings rather than refitting the scene unless the user explicitly invokes Fit or Reset.
- The 3D viewer inspector shall expose a model-coloring control that allows switching between altitude-based coloring and no coloring. Altitude-based coloring shall be the default. The inspector shall expose a Scene Settings section after Layers with a background-color control and visibility toggles for the bounding box, the head-up display, and the title/statistics overlay, all enabled by default.
- The 3D viewer inspector shall expose compass heading, camera-tilt, camera-distance, focal-length, and automatic-rotation speed controls, shall follow the active application color palette, and the 3D viewer toolbar shall expose one play/stop toggle that starts and stops rotation around the world Z axis. The focal-length control shall be disabled while orthographic projection is active because it only affects perspective projection.
- The 3D viewer toolbar shall expose an orthogonal-projection toggle that switches the viewport between perspective projection and orthographic projection without changing loaded scene data.
- The 3D viewer inspector layer controls shall display clean layer names without visible item counts, shall expose data-driven centerline sublayer checkboxes for shot classes present in the loaded `.lox` scene, and shall expose disjoint station sublayer checkboxes only when the scene contains multiple present entrance, fixed, or other station classes. Centerline sublayers shall default to underground shots visible and surface, splay, duplicate, and hidden shots hidden. The top-level Stations and Labels layers shall default hidden, while Meshes and Surfaces shall default visible. The top-level Stations checkbox shall be the master switch for station marker rendering, while station sublayer checkboxes shall only narrow the rendered station categories when the master switch is enabled. Station markers and station labels shall be shown only for stations attached to currently visible centerline shots. Station labels shall use a single independent layer checkbox and shall not require station markers to be visible.
- The 3D viewer shall apply the selected model-coloring mode to centerline shots as well as meshes. The no-coloring mode shall render meshes with one solid light-gray color on black backgrounds and a dark neutral color on white backgrounds.
- The 3D viewer mesh renderer shall use smoothed per-vertex lighting so mesh shape remains readable on the selected viewport background.
- The 3D viewer image export shall save the current 3D viewport to a PNG file using the current camera, visible layers, overlay visibility, model coloring, and background settings. The export dialog shall let the user choose an explicit image resolution, including the current viewport size, 1920-pixel-wide and 3840-pixel-wide presets, and a custom size, and shall preserve the current viewport aspect ratio by default to avoid distorted exports.
- The 3D viewer toolbar shall expose a measurement toggle with a ruler icon that activates station-to-station measurement.
- The 3D viewer viewport shall draw a visible wireframe bounding box for the current scene extent when the scene has valid bounds.
- The 3D viewer viewport shall not draw standalone world X/Y/Z axis guide lines over the scene.
- The 3D viewer viewport shall show a compass, view-angle indicator, and scale bar as screen-space overlays when scene bounds are available, with the compass and view-angle indicator placed together below the altitude legend area and the scale bar positioned beside that control group. The altitude legend shall be part of the head-up display, shall be shown only when model coloring is set to Altitude and the head-up display is visible, and shall show multiple intermediate labels between the scene minimum and maximum elevations. The view-angle indicator shall render as a right-side semicircle split by a horizontal center line, and its needle shall move in the upper half for above-scene views and in the lower half for below-scene views.
- The 3D viewer viewport shall show the full station reference and model Z coordinate when the pointer hovers a station marker.
- The 3D viewer station markers and fully qualified station labels shall use automatic screen-space decluttering so visible station annotations do not overlap heavily in dense views while station hover and measurement picking remain available for stations attached to currently visible centerline shots even when station markers or labels are hidden.
- The 3D viewer viewport shall show an Underground Passages Length and Underground Depth summary overlay for the loaded scene, derived from underground centerline shots only and excluding surface, splay, and duplicate shots as well as surface geometry.
- Unsupported files shall offer an "Open Externally" action.
- File rows shall offer duplicate, rename, and delete actions; deleting an open file shall ask for confirmation, close the corresponding open document after any unsaved-change close prompt is resolved, and then delete the file.
- Folder rows shall offer rename and delete actions.

#### 3.8.2 Text Editor Behavior

- Each tab shall represent one open document.
- Reopening a document that is already open shall activate its existing tab rather than creating a duplicate tab.
- Unsaved edits shall remain in memory when the user switches tabs.
- A dirty tab shall visibly indicate unsaved changes.
- Save shall write only the active document.
- Save All shall write every dirty open document.
- When an open document changes on disk and the in-memory document is clean, the application shall reload it from disk automatically where practical.
- When an open document changes on disk while the in-memory document is dirty, the application shall prompt before discarding in-memory edits and shall allow the user to keep the in-memory version.
- The editor shall preserve caret position, selection, and folding state per document where possible.
- Therion syntax highlighting shall reflect the current token type, not only the file extension.
- Completion suggestions shall be context-sensitive and based on the current Therion syntax position.
- Search and replace shall support the current document and, where practical, the broader project.
- Find and Replace commands shall reveal an inline search bar in the editor area rather than requiring a separate modal dialog.
- The search bar shall support next, previous, replace current, replace all, whole-word matching, and case-sensitive matching.
- The user shall be able to hide the search bar without closing the current document, including by pressing `Esc`
  while focus is in the search bar or the active text editor.
- Global project search shall be available from a dedicated left activity-rail Search pane and from `Command/Ctrl+Shift+F`.
- Global project search shall search the open project for literal text in Therion text sources, including `.th`, `.th2`, and Therion config files.
- Global project search shall support whole-word and case-sensitive matching.
- Global project search shall decode on-disk source files using the same encoding rules as the text editor so result snippets preserve declared non-UTF-8 text.
- Global project search shall include unsaved in-memory edits from open editor tabs when producing results.
- Global project search results shall be grouped by file and shall show at least the source line, column, and a short line preview for each match.
- Activating a global search result shall open `.th` and Therion config files in raw text-editing mode, and shall open `.th2` files as map-editor documents with the raw source workspace active, move the caret to the matching source location, and reveal the inline find bar with the same search term highlighted.
- Global project search shall run asynchronously or otherwise avoid blocking the UI thread for normal project sizes.
- The editor shall display 1-based source line numbers in a left gutter that stays synchronized with document scroll position.
- For active document tabs, the application shall show a full-width document command toolbar directly above the tab strip.
- The document command toolbar shall include, from the left, `Save`, a visual separator, `Undo` and `Redo`.
- While a Therion source document is in its Raw workspace, the document command toolbar shall also expose `Format Document` after `Redo`; it shall be represented as an icon-only control with an accessible name and tooltip.
- `Save`, `Undo`, `Redo`, and `Format Document` in the document command toolbar shall be represented as icon-only controls with accessible names/tooltips.
- Main-window document command toolbars shall not draw their own bottom border; separation below the toolbar shall come from the native tab/content frame or the embedded editor content separator.
- Main-window document chrome shall use one continuous left divider between sidebar content and the editor area; the document command toolbar, file tabs, and document content shall align to that divider without duplicate embedded-canvas left borders.
- Main-window file tabs shall use native platform tab rendering and shall sit on the `QTabWidget` editor/canvas frame without custom tab-bar geometry overrides.
- The main window shall provide a usable default size and shall clamp restored session geometry to a usable minimum size so stale or platform-specific saved geometry cannot produce an unusably narrow window, while still permitting common half-screen layouts.
- When a TH2 document is active, the document command toolbar shall include these left-side groups in order: `Zoom In`, `Zoom Out`, `Fit`, `Fit With Background`; then `Select`, `Complete Draft`; then a visual separator; then `Insert Scrap`, `Point`, `Line`, `Freehand`, `Area`, `Smart Area`.
- When an embedded TH2 document is in `Raw` mode, the main-window map zoom and map-tool controls shall remain visible but disabled because they operate on the visual map surface.
- For `.th` and Therion config documents, the `Raw`/`Blocks` mode selector shall be shown in this document command toolbar instead of a dedicated in-content mode row.
- The Settings dialog shall allow choosing the default editor mode for newly opened `.th` and Therion config documents: `Raw` or `Blocks`; the default shall be `Raw`.
- The default text-editor mode preference shall apply only when opening a new `.th` or Therion config tab and shall not modify document source merely because the tab initially opens in Blocks mode.
- `Format Document` shall be an explicit, single undoable source transaction. It shall normalize only leading indentation of structurally parsed Therion rows to one literal tab per open block depth, preserve blank rows, comments and their content, code-block body rows, original line endings, and document encoding, and shall not run automatically when opening, saving, switching modes, or editing a document.
- The application shall show the active document's current text encoding as a compact status-bar value tied to the active document context.
- When the active document is open in the map editor, the status area shall reserve the leftmost, expanding status-bar position for the current map workflow instruction, such as selection guidance, active drawing-mode steps, draft completion keys, and recent map-edit status messages.
- Text, Blocks, and TH2 Visual inspector surfaces shall provide a `File` inspector view with a document panel titled by the active file name; the panel shall show the active document's full path, a copy-path action, on-disk size, last-modified timestamp, current text encoding, and any non-UTF-8 conversion warning/action.
- Shared document inspector views such as `File` shall be composed through a common document-inspector implementation so Raw, Blocks, and TH2 Visual surfaces do not duplicate layout, styling, or metadata behavior.
- When the active document is open in the map editor, the status area shall also show the current map interaction mode in a distinct color badge: `M: Select` shall be green and `M: Insert` shall be red.
- When a file is opened in a non-UTF-8 encoding, the editor shall expose an explicit conversion action to UTF-8 in the `File` inspector view.
- The text editor shall provide a contextual help/documentation panel that shows Therion command summaries, arguments, accepted values, options, and related keywords when metadata is available for the token or item at the caret. Argument and option signatures shall be visually distinguished from explanatory text while preserving canonical Therion spelling. Arguments shall remain in catalog-defined positional order, while options shall be listed alphabetically by option signature.
- The help/documentation panel shall be collapsible and resizable and shall not disturb the active editor selection when it is shown or hidden.
- In the text-editor workspace, the contextual help panel shall be presented as a persistent right-side inspector column with spacing/padding consistent with the structured Blocks workspace side inspector.
- Raw editor diagnostics shall run through the shared source validator and project validation pipeline, display findings in a persistent left-side `Validation` navigation panel, report catalog-backed syntax issues such as unknown commands/options, unknown option values, missing arguments, malformed option tokens, and unclosed blocks, and allow activating a finding to navigate to the affected raw source line. The main document toolbar shall not expose a separate `Validate Document` action; validation findings shall surface through live/editor diagnostics and the `Validation` panel. The validator shall treat Therion data/reference rows inside line, area, map, and centerline blocks, plus raw rows inside `code ... endcode` blocks, as block content rather than standalone commands, and shall validate command options from logical command lines so line-continuation markers do not count as option values. Command option boundary detection, embedded option/value token normalization, source token ranges, duplicate-row preservation for validation, and logical option value counts shall be owned by shared core command-line parsing logic rather than separate validator or highlighter heuristics. Raw editor validation tooltips shall be projections of cached shared validator diagnostics for the current document revision rather than independently recomputing catalog/token errors. Raw editor contextual help shall remain command and token documentation and shall not be replaced by validation finding details. Raw editor invalid-token underlines and diagnostic backgrounds shall use validator diagnostic source ranges for token-level findings where those ranges are available. Inside raw `code ... endcode` blocks, parent-context commands before the matching `endcode` shall diagnose the active code block as unclosed rather than being treated as raw code content. Source validation, syntax highlighting, completion, contextual help, Blocks projections, and Map projections shall progressively consume a shared parsed source document model rather than maintaining divergent block/context classification rules. Ambiguous commands that can be written as one-line commands or multiline blocks, such as `revise`, shall not be treated as block openers without a dedicated logical-command model. Catalog-derived zero-value option arity shall not by itself be treated as a validation error because catalog extraction may use it for unknown or free-form option values. Catalog option aliases expressed in signatures such as `output/o <file>` shall be recognized as equivalent option tokens. Catalog option families expressed as wildcard metadata such as `-layout-xxx` shall not be accepted as literal source options, but shall match concrete options whose suffix is known on the referenced `layout` command, such as `-layout-scale` for layout option `-scale`, without producing unknown-option diagnostics. The `Validation` panel shall also provide a `Validate Project` action that asynchronously scans all `.th`, `.th2`, and `thconfig` files in the open project, including unsaved in-memory contents from open project documents, validates missing `input` and `source` file references using the same source-reference resolution as the project index, uses the same explicitly selected project target config as the Structure and Compiler workflows when that target resolves inside the opened project, projects project-index map composition diagnostics for unresolved or ambiguous map/scrap references, groups project-level findings first under a `Project` group, and groups file-level findings by file. Activating a project-level finding shall select it and show its detail without opening a source file or external application. Opening or restoring a project, saving a document inside the open project, editing an already project-scoped open document, and adding, deleting, renaming, or changing project `.th`, `.th2`, or `thconfig` files on disk shall invalidate shared project scan caches and refresh project validation in the background without forcing the `Validation` panel open. Automatic background validation refreshes shall preserve the previous visible findings and rail severity indicator until the newer validation result is ready. The Validation activity-rail button shall visibly indicate when the current validation result contains problems and shall distinguish warning-only results from results that contain errors. The `Validation` panel finding list and inline editor diagnostics shall visually distinguish warnings from errors while preserving readable contrast in light and dark appearance modes. Project validation shall suppress uncertain unknown-command findings for `thconfig` files until a dedicated thconfig command catalog is available. Post-MVP, project validation should become a live, debounced, cancellable diagnostic projection over the open project and open in-memory documents so findings update automatically when source text, files, project structure, or catalog/source-model metadata changes; at that point a manual project-validation action should be removed or reduced to an explicit refresh/re-run control. Automatic fixes shall be exposed only when they can be presented as explicit user-selected per-finding source edits through `Apply Fix`; a fix shall be applied only when the source snapshot still matches the snapshot that produced it, and successful fixes shall refresh open projections of the changed document without requiring an additional finding activation.
- The `Validation` panel shall allow exporting the currently visible validation result to a Markdown report containing a summary, grouped findings, source snippets, and safe-fix previews when available, and shall prefill the export filename with the project name and a timestamp.
- The validator shall be the single source of editor and project diagnostics. The syntax highlighter shall classify normal token categories and shall render validator-provided diagnostic ranges, but it shall not independently create warnings or errors. Any diagnostic-colored underline/background shown in Raw mode shall correspond to a validator diagnostic available through the `Validation` panel or a validation tooltip, except for transient stale projections while a newer background validation run is still pending.
- Project validation shall detect deterministic Therion project consistency problems including missing `input`/`source` references, unknown commands/options, invalid command document type or block context, malformed or duplicate option tokens, missing option values, unknown enum argument/option values when the catalog provides positional metadata, missing command arguments, unclosed blocks, duplicate explicit object IDs within their Therion namespace, unresolved or ambiguous map/scrap references, unresolved or ambiguous `join` references, unresolved or ambiguous station references in supported contexts, and area boundary references to line IDs not present in the current scrap. `.th2` `point ... station` objects shall be validated against survey stations only when they include a `-name <station>` reference. Closed `line ... endline` and `area ... endarea` blocks inside `scrap` blocks that contain no non-blank body rows shall be reported as warning diagnostics with an explicit `Apply Fix` action that removes only the empty object block; deletion fixes shall show the source block to remove and label the action as removal rather than presenting an empty replacement line. The validator shall not silently delete these objects and shall not offer this cleanup for centerline, survey, or comment-containing block bodies. Rules with incomplete catalog/source metadata shall be conservative and avoid false positives.
- Source-reference resolution for `input` and `source` shall treat quoted path tokens, leading `./` relative paths, and Windows-style backslash separators as equivalent to normal forward-slash relative paths. The validator shall warn on path tokens that use backslash separators in source-reference commands and supported path options such as `export -output` / `-o`, and shall offer an explicit `Apply Fix` source edit that converts only the affected path token to portable `/` separators while previewing the full source line with the replacement applied.
- Unclosed `.th2` map blocks shall offer an explicit `Apply Fix` action when the missing close directive can be inserted
  conservatively: `scrap` fixes shall insert `endscrap` before the next `scrap` command or at end of file, and `line` /
  `area` fixes shall insert `endline` / `endarea` before the next `point`, `line`, `area`, `endscrap`, or at end of file.
  These fixes shall preserve the document line-ending style, show the planned insertion target before the user applies
  the fix, and shall not be applied silently.
- Dash-prefixed free-text option values, including quoted point labels such as `-text "-21 m"`, shall remain values and shall not be reinterpreted as new command options.
- Raw and Blocks modes shall use the same contextual-help renderer and metadata scope for command-level help, including syntax, arguments, accepted values, options, summaries, and related keyword data where available.
- Contextual help inspector content shall be wrapped in a standard inspector panel whose header reflects the current command, validation context, or special help target; the help content shall use the available panel height without adding a redundant inner `Contextual Help` heading.
- Inspector views shall use one outer view-level vertical scrollbar for overflow content; inner content widgets such as contextual help renderers shall not introduce a second nested scrollbar when they are hosted inside an inspector view.
- When a TH2 file is open, the text editor selection shall stay synchronized with the graphical map selection.
- When map/object selection reveals a source location in the text editor, the corresponding source line shall be visibly highlighted in the editor viewport.
- When the text cursor is on a `scrap` or `endscrap` directive, the map selection shall include all selectable map objects that belong to that scrap block.

#### 3.8.3 TH2 Map Editor Behavior

- The map editor shall render the currently open TH2 document as an editable two-dimensional workspace.
- The map workspace shall maintain sufficient contrast for geometry strokes, handles, labels, and grid lines in both light and dark system appearance modes.
- When a TH2 document is active in the main window, the embedded workspace shall provide explicit `Raw` and `Visual` modes.
- When a TH2 document is active in the main window, the `Raw`/`Visual` mode selector shall be hosted in the right-aligned controls of the full-width document command toolbar above the tab strip rather than in a dedicated row inside the tab content.
- When a TH2 document is active in the main window, map-pane detach/reattach (`Separate Map` / `Return Map`) shall be provided in the same document command toolbar control area as the `Raw`/`Visual` mode selector.
- Right-aligned document command toolbar controls such as `Raw`, `Visual`, `Blocks`, and map-pane detach/reattach shall be compact square icon-only controls with accessible names and tooltips; map-pane detach shall use the screen-share icon and map-pane return shall use the monitor-x icon.
- Embedded TH2 `Visual` mode shall align its canvas/inspector content edge with the same thin top separator used by Raw and Blocks editor content under the main file tabs.
- TH2 `Visual` inspector views shall use the shared segmented inspector selector and shall not override platform tab geometry or tab shape.
- When a TH2 map editor is presented outside the main tab strip (for example in a detached dedicated map-editor window), the top command toolbar shall omit `Visual`/`Raw` mode switching and keep only actions relevant to the detached visual workspace.
- Detached map-window command toolbars shall draw a top separator below the native titlebar area and one bottom separator above the detached map canvas/inspector content.
- In embedded `Visual` mode, the workspace shall present the graphical map canvas together with a right-side map inspector.
- The right-side map inspector in `Visual` mode shall provide selector views for `Selection`, `Objects`, `Backgrounds`, and `File`.
- The right-side map inspector shall use the selector labels as its primary heading and shall not add a redundant standalone `Inspector` title above the selector.
- The right-side map inspector shall not draw a separate outer pane border around selector content; the active view content shall align horizontally with the segmented selector.
- The `Selection` and `Backgrounds` views shall use the same framed-section pattern, with each section heading placed inside its box rather than as a standalone label above the box.
- The `Objects` view shall provide source-linked object-tree navigation grouped by scrap.
- The `Selection` view shall provide selection details/settings editing for the currently selected map object.
- When no map object or pending insert object is selected, the `Selection` view shall show an explicit framed empty state consistent with the Blocks `Selection` inspector and shall not infer an editable selection from the current text cursor line.
- For selected or pending-insert `point`, `line`, and `area` objects, the `Selection` view shall show a `Preview` row next to the object quick-edit fields, using the same map object style catalog resolution as the canvas renderer. The preview tile shall use a map-like light preview surface in both light and dark themes so dark map symbols remain readable without inverting their configured colors. Area previews shall use the available preview tile area for fill-pattern readability, including deterministic dot-pattern jitter when configured. The preview shall preserve readability with preview-only contrast treatment when necessary; this shall not alter the configured style colors used by the canvas renderer.
- Map hit testing shall use screen-space stroke tolerance for line bodies so line selection remains precise across zoom levels; clicking inside an area fill shall select the area unless the click is on a higher-priority handle/vertex or within the visible line-stroke hit tolerance.
- Activating `Point`, `Line`, `Freehand`, or `Area` insertion shall activate the `Selection` view before the first point or vertex is placed and shall expose pending object fields for type, subtype, ID, point name, and supported point value where applicable; edits to those fields shall not mutate source text until the new object is inserted, and the inserted command shall use the pending values.
- Activating `Point`, `Line`, `Freehand`, or `Area` insertion while a map object or scrap is selected shall target the selected object's enclosing scrap for the inserted source text; the pending insert metadata shall show the target scrap identifier before the write occurs. When existing scraps are available, the pending insert controls shall provide an `Insert into` target-scrap selector so the user can choose a different existing target scrap before committing the new object. If no existing scrap can be resolved, the insertion workflow may create and target the default draft scrap as a safe fallback, and the Selection inspector shall explicitly state that the new object will create that scrap with a visually distinct notice.
- Activating `Insert Scrap` shall immediately create a new scrap block, select the newly created scrap source line, select the corresponding item in the `Objects` tree, and keep the `Selection` view active so the user can edit the new scrap ID/projection before inserting points, lines, freehand lines, or areas into it. Scrap insertion shall not show target-scrap metadata or an `Insert into` selector because scraps are not inserted inside other scraps.
- When the selected object is a `scrap`, the `Selection` view shall expose manual scrap scale editing as a separate `Scrap Scale` section below the basic scrap identity/projection section. The scale section shall edit XTherion/Therion-compatible 8-parameter `-scale` calibration values, including picture point 1/2 in pixels, real point 1/2, unit, and an action that writes the resulting `-scale [...]` option to the selected scrap command.
- In embedded `Raw` mode, the workspace shall present the source text editor together with the contextual help inspector and no embedded map pane.
- The embedded graphical map pane shall stay dedicated to map editing and shall not include a separate persistent map-help panel.
- The user shall be able to detach the current TH2 session into a dedicated map editor window without creating a separate document state.
- Embedded and detached map presentations shall remain synchronized with the same selection, undo history, and underlying text document.
- When the map pane is detached, the main tab shall continue showing the raw source editor while the detached map window presents the graphical map plus the same right-side map inspector for the same TH2 session.
- When the map pane is detached, main-window TH2 map zoom/drawing/selection tool groups shall be hidden because those commands apply to the detached map window; the main document toolbar shall retain only document-level actions and the map-pane return/focus control.
- When the map pane is detached, map zoom and `M: Select`/`M: Insert` mode indicators shall be shown in the detached map window status bar and shall not remain duplicated in the main-window status area.
- On first display, scrap nodes shall default to expanded state.
- If a map object is selected elsewhere in the application, the corresponding scrap shall expand automatically so the object remains visible in the list.
- Clicking a map object row shall select that object, update the map selection, and reveal the corresponding source location in the text editor when the source line is known.
- Selecting an `area` object shall also visually select/highlight its referenced border `line` object(s) so the area boundary ownership is visible in the canvas.
- Clicking an already selected map object row again shall clear the current map/object selection.
- Clicking the visibility icon on a map object row shall toggle that object's visibility without changing the current selection.
- Each source-linked map object row shall expose compact visibility and delete icon actions.
- Delete actions from the object tree shall ask for confirmation, shall remove the corresponding source command span through the same safe source-edit path used by structured block deletion, and shall participate in document undo/redo.
- A `line` block referenced by an `area` block shall not be deletable as a standalone map object because that would leave the area with a dangling border reference.
- When a referenced border `line` is selected, the selected-object details shall explain which area(s) use it, shall provide a direct select/jump affordance to the referencing area, and shall disable standalone object deletion with an explanatory tooltip/status message.
- Deleting an `area` block shall remove only the selected `area ... endarea` source span; referenced border `line` blocks shall be preserved even when no remaining area references them.
- Dragging the handle on a map object row shall start a move or reorder operation.
- Dropping onto another object shall insert the dragged object before or after the target depending on the drop placement.
- Dropping onto a scrap target shall move the object into that scrap when the object type supports it.
- The list shall show a visual drop indicator for the current drag target.
- All map mutations shall support undo and redo.
- The map command surface shall expose zoom in, zoom out, fit geometry, fit background plus geometry, undo, redo, selection mode, draft completion, scrap insertion, point insertion, line insertion, freehand line drawing, and area insertion.
- In the main window, map command actions shall be hosted in the shared full-width document command toolbar above the tab strip; the graphical map canvas shall not host a floating in-canvas map toolbar overlay.
- In detached map windows, an equivalent top command toolbar shall be shown above the map canvas and inspector.
- Toolbar actions should present compact icon-first controls, with text equivalents available through tooltips, accessibility names, and automation-stable identifiers.
- When a map editor tab or detached map window is active, the status bar shall show the current map workflow instruction as the first left-aligned item, before zoom, map mode, compiler, and encoding indicators.
- When a map editor tab is active, the status bar shall show the current map zoom before the `M: Select`/`M: Insert` mode badge.
- The main-window status bar shall show a compact global compiler indicator prefixed with `C:` and containing the current compiler state and the last run result (`Idle`, `Running`, `OK`, or `Failed`).
- Activating the compiler status indicator shall toggle the Compiler sidebar: open it when hidden/collapsed or showing another pane, and collapse it when the Compiler pane is already visible.
- The status bar shall not show the active document file name or path; persistent document identity remains available in the tab title and tooltips.
- Bundled map toolbar icons shall be permissively licensed, shall include their license notice in the repository, and shall adapt to the active light/dark application palette.
- Placement tools that require a scrap context shall be disabled or unavailable until a valid scrap target exists.
- Zoom and pan shall be editable and shall persist per document session.
- Input-device behavior shall be deterministic and mode-aware:
  - holding Space and dragging with the primary mouse button shall temporarily pan the map canvas without leaving the current map tool
  - holding Ctrl and dragging with the primary mouse button shall pan the map canvas using the same behavior as right-button drag, while Ctrl plus primary-button click without drag shall remain available as a secondary-click/context-menu gesture on platforms that use it
  - precise scrolling devices (touchpad and Magic Mouse) shall pan by default
  - non-precise wheel scrolling may zoom by default
  - pinch/magnify gestures shall zoom
  - a modifier-based zoom gesture (for example Command+scroll) should be supported consistently across platforms
- Pen-plus-touch workflows shall not accidentally trigger zoom when the user performs a pan gesture.
- Map drawing and editing affordances, including point handles, line/area vertex handles, control handles, preview strokes, and line strokes, shall remain visually usable across zoom levels by using screen-adaptive sizing where appropriate.
- Area fill and fill-pattern rendering shall visually honor Therion's default `-clip on` behavior by clipping areas to the owning scrap wall boundary when that boundary can be resolved; areas with `-clip off` shall render without scrap-boundary clipping.
- Area rendering shall resolve `area ... endarea` border references against `line -id ...` geometry in the same owning scrap. Referenced border lines may be individually open; when their intersections form a closed face, the canvas shall render that face as the area fill without modifying the source line geometry.
- When referenced area border lines cannot be resolved into a closed polygon or closed intersection face, the map editor shall surface an explicit warning instead of silently rendering a misleading fill.
- Smart Area insertion shall be exposed as a separate map command surface tool from manual Area drawing. It shall let the user click inside a same-scrap closed face formed by existing closed or intersecting line geometry, preview the candidate area fill and referenced boundary lines without mutating source, and require explicit confirmation before writing the new `area ... endarea` block.
- Smart Area insertion shall not change existing line geometry. On confirmation it may add missing `-id` options to referenced boundary line commands when those identifiers are required to serialize the Therion area reference; the ID additions and new area block shall be one undoable source transaction.
- After Smart Area confirmation, the map editor shall return to selection mode.
- When multiple Smart Area candidates contain the click point, the editor shall choose the smallest containing candidate by default and provide a keyboard-accessible way, such as `[` / `]`, to cycle the other candidates before confirmation.
- When Smart Area cannot identify a closed candidate, or when candidate boundary lines cannot be referenced safely, the editor shall show an actionable warning and shall not write source text.
- The TH2 Visual map editor shall provide a fixed viewport magnifier overlay for precise tracing and placement. The magnifier shall show a zoomed crop around the current cursor position, an exact center crosshair, and the corresponding map/source coordinate readout.
- The map magnifier shall be rendered as a viewport overlay, not as scene geometry, so it remains pinned to the map viewport and does not pan or zoom with the document.
- The map magnifier shall be enabled by default and shall be user-toggleable through the `View -> Show Map Magnifier` / `Hide Map Magnifier` menu action. This preference shall be stored as UI/session state and shall not modify the TH2 source document.
- Background image position, X/Y scale, rotation, opacity, gamma, and visibility shall be editable per session.
- Background layers shall support insertion, selection, visibility toggles, opacity adjustment, gamma adjustment, persisted positioning, and persisted advanced transforms where the source metadata format can represent them.
- Session state may restore local display settings only for a background layer declared by the current TH2 source metadata; it shall not silently introduce a visible background reference that is absent from the TH2 document.
- The map viewport shall permit panning to every visible background layer edge, including a layer moved, scaled, or rotated beyond the base map canvas; expanding the scrollable extent shall not change the map/source coordinate projection.
- Adding a raster background image from the TH2 Visual map editor shall write XTherion-compatible `##XTHERION## xth_me_image_insert` metadata to the TH2 source, using document-relative paths where possible.
- Adding a PocketTopo Therion export (`.txt`) from the TH2 Visual map editor's background workflow shall prompt for XVI scale, resolution, grid spacing, and plan or extended-elevation projection; convert the selected PocketTopo projection data into a generated `.xvi` file beside the export using `_p` or `_e` naming; add the generated `.xvi` as the background layer; and write XTherion-compatible `##XTHERION## xth_me_image_insert` metadata to the TH2 source.
- Adding an SVG background from the TH2 Visual map editor shall write Mapiah-compatible `##MAPIAH## image_insert_v1` metadata with `format=svg`, document-relative paths where possible, transform fields, intrinsic size, and source viewBox fields derived from the SVG root metadata.
- Reading `xth_me_image_insert` metadata shall accept XTherion-compatible placement variants, including the common form where the y/base coordinate is an unbraced token after the first metadata group.
- Reading Mapiah `##MAPIAH## image_insert_v1` metadata shall support `format=xvi`, `format=raster`, and `format=svg` background references, including `xx`, `yy`, `xScale`, `yScale`, `rotationCenterDx`, `rotationCenterDy`, `rotationDeg`, `pivotSet`, SVG intrinsic size and source viewBox fields, and `xviRoot` where applicable, so rotated or scaled Mapiah-authored background references can be displayed in the TH2 Visual map editor.
- Mapiah `format=svg` references shall render as SVG background layers using the stored intrinsic size and source viewBox metadata as the placement contract. The source reference shall remain an SVG/Mapiah layer and shall not be rewritten as a raster image. SVG background layers shall not expose raster gamma adjustment.
- Editing a raster, `.xvi`, or SVG background layer's X/Y scale, rotation, or pivot in the TH2 Visual map editor shall write Mapiah `##MAPIAH## image_insert_v1` metadata for that layer so the advanced transform is restored when reopening in Therion Studio or Mapiah. The scale controls should default to locked proportions so common scaling changes update X and Y scale together, while still allowing explicit non-uniform scale edits. Pivot editing shall be available as a map-click workflow from the `Backgrounds` inspector, shall show a visible pivot marker while choosing the point, shall support clicks inside or outside visible raster/SVG layers as layer-local transform points, and shall support resetting to the layer's default pivot.
- Converting an existing XTherion `xth_me_image_insert` raster background to Mapiah metadata because of scale, rotation, or pivot editing shall preserve the existing metadata base position unless the user is explicitly moving the layer.
- Adding or removing raster background images from the TH2 Visual map editor shall maintain XTherion-compatible `##XTHERION## xth_me_area_adjust` and `##XTHERION## xth_me_area_zoom_to` header metadata.
- Editing a raster background layer's position, visibility, or gamma in the TH2 Visual map editor shall update the corresponding `xth_me_image_insert` metadata so reopening in XTherion or Therion Studio restores the same background reference.
- Editing a raster background layer's visibility or gamma shall preserve the layer's existing placement metadata, shall not rewrite X/Y placement tokens, shall not change its scene position, and shall not change the selected background layer.
- When a TH2 Visual map has no parseable point, line, or area geometry but has visible background layers, Fit shall center on the visible background bounds instead of the empty placeholder canvas.
- When a TH2 Visual map has no parseable geometry but has visible background layers, the map editor shall suppress the empty-document placeholder canvas and no-object messages so the background remains the primary drawing surface.
- Removing a raster background layer that is backed by `xth_me_image_insert` metadata shall remove that metadata line from the TH2 source.
- The `Backgrounds` inspector layer list shall expose per-row visibility and delete icon actions aligned with the object-list action pattern.
- The `Backgrounds` inspector shall not expose separate editor-generated map-grid controls; reference grid content shall come from background layers such as `.xvi` files.
- The map editor shall read Therion scrap `-scale` in numeric, unit, ratio, and 8/9-parameter calibration forms so manual scale calibration and geometry transforms remain compatible with Therion and XTherion-authored files.
- Manual edits to a selected scrap scale shall write XTherion-compatible 8-parameter `-scale [x1 y1 x2 y2 rx1 ry1 rx2 ry2 unit]` metadata and shall preserve other scrap options/comments where practical.
- Background sketch or image layers declared by TH2 source metadata shall be restorable when reopening the document.
- `.xvi` vector background references shall render as background layers when present.
- `.xvi` vector background rendering shall draw embedded `XVIgrid` lines as background content when present.
- The map editor shall not expose a dedicated touch-controls toolbar mode; mouse, touchpad, Magic Mouse, pinch, stylus, and platform touch gestures shall use automatic input-policy handling.
- After reparsing the document, the map editor shall restore the selected object when that object can still be resolved in the updated document.
- Geometry editing shall support point placement, line vertex editing, area editing, and selection of individual vertices or control points.
- When a visible bezier control point overlaps or is near another selectable line/area shape, hit testing shall prioritize the control point so it remains selectable and draggable.
- During interactive line insertion, click-only anchor placement shall create straight segments between consecutive anchors.
- During interactive line insertion, press-drag-release anchor placement shall follow XTherion-style Bezier entry: the press location shall place the new anchor, and the drag/release location shall seed the same anchor's outgoing Bezier control while the opposite incoming control is mirrored around that anchor.
- During interactive line insertion, dragging either visible draft control for a vertex that has both Bezier controls shall auto-mirror the opposite control so both handles remain collinear around that same vertex.
- During interactive line insertion, existing bezier control points shall remain visible in the draft preview and shall support direct drag adjustment before the draft is committed.
- During interactive line insertion, when a draft vertex has both incoming and outgoing bezier controls, dragging one control shall mirror-adapt the opposite control to preserve smooth tangent continuity.
- During interactive line insertion, hovering a draggable draft bezier control point shall present a hand cursor and active drag shall present a closed-hand cursor.
- During interactive line insertion, a left-button double-click on the map canvas shall insert a line vertex at the double-click position, commit the line draft when it is sufficiently complete, and return the map editor to Select mode.
- During freehand line insertion, the live preview shall show the drawn stroke as a solid line without per-sample point markers, and the sampled stroke shall be simplified before commit and serialized as bezier coordinate rows rather than as a dense point-by-point polyline.
- Freehand stroke simplification shall be geometry-sensitive rather than based on a fixed point count: simpler strokes shall collapse to fewer bezier anchors, while more complex strokes shall retain proportionally more anchors needed to preserve the drawn shape.
- Completing an interactive map insertion or map-edit command shall preserve the current map viewport center and zoom; the scene refresh shall not automatically refit or recenter on the inserted/rewritten object unless the user explicitly invokes a Fit command.
- During interactive area insertion, click-only anchor placement shall create straight segments between consecutive anchors.
- During interactive area insertion, press-drag-release anchor placement shall create curved segments with editable Bezier control points, using the same XTherion-style mirrored-handle seeding and handle-edit workflow as interactive line insertion.
- During interactive area insertion, when a draft vertex has both incoming and outgoing bezier controls, dragging one control shall mirror-adapt the opposite control to preserve smooth tangent continuity.
- During interactive area insertion, when a curved close can be resolved from first/last area-vertex tangents, the closing segment from last anchor to first anchor shall be serialized as an explicit cubic row in the generated border line so the closed outline preserves smooth curvature.
- During interactive area insertion, the committed geometry shall be serialized as a closed `line border -id ... -close on` block followed by an `area ...` block that references the generated border line identifier.
- When `##XTHERION## xth_me_area_adjust` metadata is present, map render projection, interactive scene-to-source coordinate mapping, and metadata-background reprojection shall use that model rectangle as stable source bounds to avoid edge-insertion remap drift.
- When a TH2 document lacks parseable `##XTHERION## xth_me_area_adjust` metadata, the map editor shall use one auto-adjust model rectangle for rendering and interactive scene-to-source coordinate mapping: derived from current map geometry/background bounds with padding, or `0 0 256 256` for an otherwise empty visual map. The first map insertion that writes point, line, freehand-line, or area geometry shall persist matching `xth_me_area_adjust` and `xth_me_area_zoom_to` metadata in the same undoable source transaction without changing existing geometry coordinates.
- During interactive area insertion, the generated border line identifier shall use `line-X` naming and shall be unique among explicitly assigned `-id` values inside the owning scrap.
- During map-driven scrap insertion, auto-generated scrap identifiers shall use `scrap-X` naming and shall be unique within the document.
- During map-driven scrap insertion, generated scrap commands shall include an XTherion-compatible default `-scale` using the current map source bounds: the first picture point at `(xmin, ymin)`, the second at `(xmax, ymin)`, and the real length as `(xmax - xmin) * 0.0254 m`.
- Object identifiers shall remain optional for most map objects, but when an identifier is explicitly set inside a scrap it should be unique within that scrap; generated border lines referenced by area blocks shall always include an explicit `-id`.
- Vertex overlays should be shown only for the currently selected map object, and line-control handles/connectors shall be shown only for the currently selected line vertex, to reduce visual clutter while preserving editability.
- Selecting a line object shall show an XTherion-compatible direction tick at the first line vertex; the tick shall be perpendicular to the first segment tangent, shall prefer the first outgoing control handle when present, and shall flip direction when the line has `-reverse on`.
- Point objects with explicit supported `-orientation` shall render their symbol geometry rotated around the point anchor; adjacent style-driven text labels shall remain screen-aligned unless their resolved point style opts into orientation-driven text rotation.
- Selecting an orientable point object shall show a compact orientation handle in the map canvas when that point has an explicit `-orientation` value, including `0`; dragging the handle shall update the point's `-orientation` source option and preserve the point selection after the scene refresh.
- Selecting a `line slope` anchor vertex shall show an XTherion-style left line-point handle in the map canvas when that line point has an explicit `orientation` value, including `0`; dragging the handle shall update the selected point's `orientation` and `l-size` source options using the same XTherion line-point row serialization as the inspector.
- Enabling orientation for a selected line anchor that does not yet have an explicit line-point orientation shall seed the new value perpendicular to the local line direction on the line's left side, rather than defaulting to `0`.
- Selecting a line or area vertex/control point in the map editor shall reveal the corresponding source coordinate token in the text editor when that source token can be resolved.
- Placing the text cursor on a line or area coordinate token shall select the corresponding map vertex/control point when that map geometry is resolvable.
- Placing the text cursor on vertex-related line option rows (for example `smooth off`) shall select the corresponding current line vertex when that map geometry is resolvable.
- If line-point orientation is not explicitly set, the effective default shall be perpendicular to the local line tangent on the left side of the line direction.
- Line-point orientation values shall be normalized to a canonical 0 to <360 degree range when parsed, edited, and serialized.
- Repeated directional line decorations (for example teeth or ticks) shall use line direction as their reference frame, and when a line is reversed the decoration orientation shall reverse accordingly.

#### 3.8.4 Map Object List Presentation

- Scrap rows shall show the scrap icon, scrap name, item count, and projection summary.
- Line rows shall show the line icon, line type, optional line ID, optional subtype, and closed-state indicator when applicable.
- Point rows shall show the point icon or station icon depending on the point type.
- Point rows shall show the point type as the main label and optional point ID and subtype as details.
- Station point rows shall additionally show the station name as the name field for station points only.
- Area rows shall show the area icon, area type, and optional area ID.
- The "name" concept shall be limited to station points in the object list and inspector; non-station points shall not show a station name field.

#### 3.8.5 Object Settings / Inspector Behavior

- The inspector shall always reflect the current object selection.
- Selecting a different object shall immediately update the visible settings panel.
- Station points shall expose `stationName` as an optional station-reference field.
- Station points shall hide label-specific rows that do not apply to stations.
- Non-station points shall treat `label` as optional and shall not expose station-name editing as a primary field.
- When a station point has a non-empty `stationName` / `-name` value, project validation shall validate that reference against survey stations when sufficient project context is available.
- If the containing scrap declares `-station-names <prefix> <suffix>`, project validation shall resolve the effective station name as `<prefix><stationName><suffix>` in the owning survey namespace; `[]` shall represent an empty prefix or suffix.
- `Edit Object Settings...` shall expose every parsed attribute from the selected object's command line in one option-style table: positional arguments, catalog options, and unknown-but-valid option rows. Positional argument rows such as `type`, `x`, `y`, and scrap `id` shall be protected from key editing but shall allow value editing. Commands whose first positional argument is not an ID shall not display that first positional argument as a separate ID field; object identifiers stored as `-id` shall remain editable as `-id` options. The dialog shall open at a usable width for typical option names, values, and contextual help without excessive horizontal footprint, and the option column shall not collapse to the shortest option key. For map objects, the contextual help pane shall show object/command-level help rather than switching to individual option help as rows are selected. This workflow shall use the shared catalog-backed command-options component used by structured command editing rather than a legacy block-only configure dialog.

#### 3.8.6 Structure Sidebar Behavior

- The structure sidebar shall present a navigable project hierarchy limited to surveys, maps, and scraps.
- The structure sidebar shall show a short explanatory description above the hierarchy tree.
- The structure sidebar shall not prepend synthetic top rows for project-root path or summary text inside the hierarchy tree.
- Structure rows shall include category icons for survey, map, and scrap items using the bundled compass, map, and puzzle Lucide icons respectively.
- Structure siblings under the same parent shall be displayed in category order survey, map, scrap, followed by diagnostics, and entries within the same category shall be sorted alphabetically by displayed name with source order used only as a stable tie-breaker.
- Structure parsing shall treat Therion spelling aliases `centreline` / `centerline` and `endcentreline` / `endcenterline` equivalently.
- Structure graph object-kind labels and summaries shall preserve canonical Therion source terms such as `survey`, `centerline`, `map`, `scrap`, `station`, `point`, `line`, and `area` instead of translating them.
- The project navigation sidebar shall provide a compact `Files` / `Survey` / `Map` segmented view selector. The `Files` view shall host project file navigation and file/folder actions. The `Survey` and `Map` views shall host Structure projections.
- The left activity rail `Structure` entry shall open the shared project navigation sidebar. The project navigation sidebar shall provide the `Files` tab internally rather than requiring a separate `Files` rail entry.
- The Structure sidebar shall provide separate `Survey` and `Map` views. The default `Survey` hierarchy shall be the survey namespace and definition hierarchy. Map and scrap items shall remain under the survey namespace where they are defined; resolved map composition references shall not reparent those definition nodes in the `Survey` view.
- The `Map` Structure view shall present map composition by showing top-level maps, child maps, and referenced scraps according to resolved map composition references.
- Map item parsing for Structure shall recognize both bare references and Therion offset syntax of the form `<map reference> [<offset X> <offset Y> <units>] <above/below/none>`.
- Map and scrap reference resolution shall follow Therion namespace rules: unqualified references resolve in the owning map namespace, and explicit `name@child.parent` qualifiers resolve relative to the owning map namespace.
- Unresolved and ambiguous map/scrap references detected inside a map block shall be shown as warning child rows under the owning map node and shall navigate to the source reference line when activated.
- A map block that mixes map references and scrap references shall be shown with a navigable warning child row because Therion map content expects one content kind per map block.
- `preview above` / `preview below` map relations shall not be treated as ownership hierarchy in the Structure sidebar.
- The Structure sidebar shall remain a lightweight navigation/indexing aid; it shall not be presented as a full substitute for Therion compiler validation or export selection semantics.
- Selecting a row in the structure sidebar shall only select that row. Activating a source row, such as by double-clicking it or pressing Enter, shall change the active document context and navigate to the source location.
- The structure sidebar shall remain synchronized with the open project and the currently selected content when possible.
- Structure refreshes shall preserve user-controlled expand/collapse state for stable survey, map, scrap, and diagnostic rows within the current project session.
- For files currently open in editor tabs, structure indexing shall use in-memory document text so unsaved edits are reflected immediately in the structure tree.
- Structure indexing shall treat the opened project folder as the root graph boundary and shall prefer an explicitly selected project target config when it resolves inside that project folder.
- Structure index snapshots shall expose the normalized project root, resolved root config path when one is used, and root traversal files so the UI and future compiler-confirmed checks can report the exact graph input.
- Renaming, deleting, creating, or externally changing project source/config files shall invalidate shared project source and index caches before structure, validation, and output projections are refreshed.
- When no explicit project target config is selected, a root-level preferred config shall define the default project structure graph using this priority order: `thconfig`, `thconfig.thconfig`, `main.thconfig`, `index.thconfig`, then `<project_name>.thconfig` where `<project_name>` is the opened project folder name.
- When no preferred root-level config file exists and exactly one root-level named config file (`*.thconfig` or `thconfig.*`) exists, that config may define the project structure graph.
- When multiple root-level named config files (`*.thconfig` or `thconfig.*`) exist without an explicit project target config or preferred root-level config, the application shall not silently merge those graphs and shall show an actionable message in the structure sidebar asking the user to choose a project target config; the message shall provide a direct action to focus the Compiler pane target-config control.
- The structure sidebar summary or diagnostic tooltip shall report the root config or inferred root files used to build the current structure graph.
- The structure sidebar shall provide explicit user controls to collapse and re-expand the panel.
- The sidebar shall use a fixed-width activity rail plus a resizable content pane so the rail remains available when content is collapsed.
- The activity rail shall provide `Structure`, `Search`, `Validation`, and `Compiler` pane entries plus a visually separated `Compile` action using the play icon. The rail `Compile` action shall run Therion with `Project Config` without requiring the Compiler pane to be focused.
- Resizing the sidebar content pane to its collapsed threshold shall automatically collapse the content pane, leave the activity rail visible, and shall not resize the rail itself.

#### 3.8.7 Therion Runner and Console Behavior

- Running Therion shall never freeze the UI thread.
- Therion output shall be streamed to a console view in the order it is produced.
- The compiler console output view shall contain Therion process output only; application status messages such as project-open events, runner setup notes, and UI workflow messages shall be shown outside the console output view.
- The console shall capture stdout, stderr, and the exit status of each run.
- The compiler status shall not report `OK` when Therion exits with code 0 but stderr contains a known output-write error such as `warning -- error writing`; that run shall be surfaced as a failed run with an actionable status detail.
- The console surface shall show the active config name and location, the working directory, and the active command-line options.
- The working-directory editor shall act as a `Project Config` override and shall be empty by default.
- When `Current Config` is active, the working-directory override shall be disabled and ignored; the effective working directory shall be the active config file directory.
- When `Project Config` is active and the override is empty, the effective working directory shall be the selected project config directory, falling back to the project root when no config is resolved.
- The resolved config path shall be displayed immediately below `Target Config`, and the effective working directory shall be displayed immediately below `Working Directory Override`, as less-prominent helper text without additional labels.
- The compiler sidebar surface shall remain usable at narrow widths by using stacked field labels, compact browse/reset controls, wrapped runner output, and action buttons that wrap into multiple rows instead of clipping horizontally.
- The compiler sidebar shall show a short explanatory description above the runner controls.
- The compiler sidebar shall provide a single run surface with an explicit run target selector for `Current Config` and `Project Config`.
- The status bar compiler indicator shall update when Therion starts, finishes, or reports a runner error, and shall keep the last success/failure result visible while the user continues editing.
- A `Compile Current Config` toolbar action using the play icon shall be shown for active Therion config documents after `Undo`/`Redo`, separated by a toolbar divider, and shall run Therion with `Current Config`.
- `Current Config` shall run the active document only when the active document is a Therion config file such as `thconfig`, `*.thconfig`, or `thconfig.*`.
- When the active document is a Therion config file, the run target selector shall automatically switch to `Current Config`.
- When the active document is not a Therion config file, the run target selector shall be locked to `Project Config`.
- `Project Config` shall run the configured project target config when one is set, otherwise it shall resolve a root-level preferred config from the project or working-directory context using this priority order: `thconfig`, `thconfig.thconfig`, `main.thconfig`, `index.thconfig`, then `<project_name>.thconfig` where `<project_name>` is the folder name used as the project or working-directory context; if no preferred config exists, it shall resolve the only named config file (`*.thconfig` or `thconfig.*`) when exactly one exists.
- Closing a project shall clear the project-scoped compiler target config and working-directory override while preserving global runner preferences such as executable path. Additional command-line options shall remain only in the current running window/session and shall not be restored from persistent settings on the next application launch.
- Arbitrary one-off configs shall be run by opening the config file as a document and using `Current Config` rather than a separate custom run-target mode.
- When no explicit config argument is supplied in the additional command-line options, the selected run-target config shall be passed to Therion as the source/config argument for the run.
- If no run-target config can be resolved, the application shall block the run and show an actionable message rather than invoking Therion without source files.
- The selected run-target mode and target config path shall persist across restarts.
- If the persisted or visible project target config path no longer resolves after a project file-system change, the application shall clear that stale project target config and return to automatic project config discovery.
- When no user-defined Therion executable is persisted, the runner shall use `therion` and platform auto-detection/fallback instead of requiring a visible Compiler-sidebar executable field.
- The Therion process shall run with a platform-aware `PATH` that includes the resolved Therion executable directory, common package-manager locations, and common TeX tool locations before the inherited environment path so helper tools such as `cavern` and `pdftex` can be found from GUI launches.
- The user shall be able to reset a non-empty project working-directory override and return to automatic working-directory resolution.
- The status-bar compiler indicator shall show a visible running, success, or failure state for the most recent run.
- The user shall be able to copy the full console output.
- The user shall be able to clear the visible console output from the Compiler sidebar without clearing runner settings or the status-bar compiler result.
- Failures shall be shown as actionable messages rather than raw process noise alone.
- After a successful run, the application may offer opening the working directory or output folder in the platform file manager.
- The user shall be able to rerun Therion from the current project context.
- The console surface shall provide explicit user controls to collapse and re-expand the panel.

#### 3.8.8 Session Restore Behavior

- The application shall reopen the last project when available.
- Opening a project shall move that project to the front of the recent-projects list, remove duplicate entries for the same normalized project path, and keep no more than five recent projects.
- When no project is open, the welcome surface shall allow reopening a recent project by selecting it from the recent-projects list.
- When no project is open, the `Files` sidebar shall show an empty project state with an `Open Project...` action and shall not expose the filesystem root or platform volume root as browsable project content.
- The `File` menu shall provide a `Recent Projects` submenu that allows reopening recent projects through the same project-open workflow as `Open Project...`.
- The project folder chooser opened by `Open Project...` shall start in the user's home folder when no more specific project-picking context is available, rather than the filesystem root or platform volume root.
- When a project is open and the welcome surface is visible, it shall show the active project name and path.
- When a project is open and the welcome surface is visible, it shall show up to ten recent files from that project and allow reopening them through the same document-open workflow as the project file browser.
- The `File` menu shall provide a `Recent Files` submenu for the active project when a project is open.
- Recent files shall be scoped to the normalized project root and shall not include files outside the active project.
- The application shall restore previously open tabs where possible.
- The application shall restore the active tab when the corresponding document still exists.
- The application shall restore viewport and selection anchors only when the underlying source locations still resolve.
- If a restored selection no longer exists, the document shall still open without failing the restore process.
- Automatic session restore shall attempt any previously saved project and document path that is still accessible, including paths inside standard user folders on macOS.

### 3.9 Command, Menu, and Shortcut Conventions

The Qt reimplementation shall preserve the intent of the current desktop shortcuts and menu commands.

Platform modifier mapping:

- Command means Command on macOS and Control on Windows/Linux
- Option means Option on macOS and Alt on Windows/Linux

| Action | Menu location | Shortcut | Required behavior |
|---|---|---|---|
| New Window | File | Command+N | Open a new empty main window without restoring the current project or open documents |
| New Therion Source (.th) | File -> New File; toolbar New Document menu | none | Open a new unsaved `.th` text document initialized with `encoding utf-8`; the first save shall prompt for a destination path |
| New Therion Map (.th2) | File -> New File; toolbar New Document menu | none | Open a new unsaved `.th2` map document initialized with `encoding utf-8`; the first save shall prompt for a destination path |
| New Therion Config (.thconfig) | File -> New File; toolbar New Document menu | none | Open a new unsaved `.thconfig` Therion config text document initialized with `encoding utf-8`; the first save shall prompt for a destination path |
| Import PocketTopo Text… | File -> Import | none | Import a PocketTopo Therion export `.txt` into the active `.th` text document; hidden when the active document is not `.th` |
| Settings / Preferences | File or native application menu | platform-standard Preferences placement where available | Open application settings for language override, Therion executable path, and default `.th` / Therion config editor mode |
| About Therion Studio | Help or native application menu | none | Show installed version/build metadata, Qt/platform details, repository, license, maintainer, and third-party notice location |
| Expand/Collapse Sidebar | View | none | Expand or collapse the left sidebar content without changing the active document |
| Expand/Collapse Help | View | none | Expand or collapse the Raw editor's contextual help column without modifying document source |
| Expand/Collapse Block Inspector | View | none | Expand or collapse the Block editor's details/inspector column without modifying document source |
| Expand/Collapse Map Inspector | View | none | Expand or collapse the TH2 Visual map editor's inspector column without modifying document source |
| Expand/Collapse Map Inspector and Help | View | none | When a TH2 Visual map pane is detached, expose both controls because the detached visual map and raw source help are visible at the same time |
| Show/Hide Map Magnifier | View | none | Enable or disable the visual map magnifier overlay as UI/session state only |
| Enter/Exit Full Screen | View | platform full-screen shortcut | Toggle the main window full-screen state |
| New Project | File | none | Expose `Project from Template...` and `Empty Project...` creation choices; disabled while a project is already open |
| Open Project… | File | Command+O | Open a Therion project; disabled while a project is already open |
| Recent Projects | File | none | Reopen one of the five most recently opened projects; disabled while a project is already open |
| Recent Files | File | none | Reopen one of the active project's ten most recently opened files; disabled when no project is open |
| Save | File | Command+S | Save the active document only; unsaved documents shall use Save As path selection on first save |
| Save All | File | Command+Option+S | Save all dirty open documents; unsaved documents shall use Save As path selection before saving |
| Close Tab | File | none in the current Swift app | Close the active editor tab after unsaved changes are resolved |
| Close All Tabs | File | Command+Shift+W | Close every open editor tab |
| Close Project | File | none in the current Swift app | Close the active project and its project-scoped documents after unsaved changes are resolved |
| Switch to Raw editor | document workspace | Command+top-row 1 | Switch the active `.th`, Therion config, or `.th2` document to Raw/source editing when supported; localized keyboard layouts shall not require Shift to type the digit character |
| Switch to secondary editor mode | document workspace | Command+top-row 2 | Switch the active `.th` or Therion config document to Blocks mode, or the active `.th2` document to Visual mode, when supported; localized keyboard layouts shall not require Shift to type the digit character |
| Undo | Edit | platform-standard Undo | Undo the last available change in the active document, including raw text edits and supported map-editing commands |
| Redo | Edit | platform-standard Redo | Redo the last undone change in the active document, including raw text edits and supported map-editing commands |
| Fold All | Editor | Command+Option+[ | Fold all foldable blocks in the active text editor |
| Unfold All | Editor | Command+Option+] | Unfold all folded blocks in the active text editor |
| Find… | Find | Command+F | Open the search bar in find mode |
| Replace… | Find | Command+Option+F | Open the search bar in replace mode |
| Find Next | Find | Command+G | Jump to the next match |
| Find Previous | Find | Command+Shift+G | Jump to the previous match |
| Toggle Debug Sidebar | Debug | none in the current Swift app | Toggle the debug sidebar state from the menu |
| User Manual | Help and Welcome | none | Open the localized full user manual document in-app, using `USER_MANUAL.<language>.md` with fallback to `USER_MANUAL.md`; the in-app manual reader shall render readable Markdown spacing, keep a generated table of contents visible beside the document, support local table-of-contents links, and provide in-manual text search with next/previous navigation. Opening the manual shall place keyboard focus in the manual search field. |

The Qt application may add additional platform-standard shortcuts only if they do not conflict with the behavior above and do not change the documented commands.

### 3.10 Window and Document Lifecycle

The Qt application shall define a consistent window and document model.

- The main application window shall present the project browser, tabbed text editor, structure sidebar, and console-related views used for project work.
- The main application window shall support a text workspace for all supported text files and, for TH2 files, an embedded mode-based `Raw`/`Visual` workspace presentation.
- The text editor shall provide a collapsible contextual help/documentation inspector below the editor or in an equivalent persistent surface.
- The Qt implementation shall use an equivalent persistent right-side help inspector surface for raw text editing to keep editor/help spatial structure consistent with the Blocks workspace, and both modes shall use the same command-help content renderer.
- A TH2 map shall open in a dedicated map editor window or equivalent dedicated top-level surface that remains associated with the same document session.
- A TH2 map may also be embedded in the main application window as part of a mode-focused workspace and detached into a dedicated map window.
- Opening the same TH2 document again shall prefer focusing the existing map editor window or session rather than creating a duplicate map editor, where the platform and implementation allow it.
- The application shall provide a dedicated Therion console window or equivalent dedicated console surface that can remain open independently of the active editor tab.
- A text editor tab shall own one document identity and one dirty state.
- Closing a dirty tab shall prompt the user to save, discard, or cancel.
- Closing a window that contains dirty documents shall prompt before any changes are lost.
- Closing the project shall close the project-scoped documents after unsaved changes are resolved.
- Quitting the application shall prompt for all unsaved changes across all open windows and tabs.
- The application shall not destroy unsaved document content unless the user explicitly chooses to discard it or a save operation succeeds.

### 3.11 Error Handling and Recovery

The Qt application shall handle recoverable errors in a user-visible and actionable way.

- Parse failures shall not destroy the current document text.
- Parse failures shall identify the document and, where possible, the line or region that failed.
- If Therion documentation/help metadata is unavailable, the text editor shall remain fully usable and the help panel shall fall back to empty or placeholder content.
- Save failures shall leave the document dirty and shall explain the reason to the user.
- If a file changes on disk while it is open, the application shall detect the change when practical; clean in-memory documents shall reload automatically, and dirty in-memory documents shall prompt the user to reload or keep the local version.
- If a file is missing when the user attempts to open it, the application shall show an actionable error and keep the project browser usable.
- If an external asset referenced by a project is missing, the project shall still open, and the missing reference shall be reported as a warning rather than as a fatal error.
- If TH2 background imagery or sketch references fail to load, the document shall still open and the failure shall be reported in the UI.
- If one background layer fails to decode or parse, remaining layers shall still load when possible.
- Therion runner failures shall preserve the console output and exit status for diagnosis.
- Invalid map edit operations shall be rejected without corrupting document state.
- Selection restoration failures shall clear the affected selection anchor instead of breaking the document or window.
- The application shall provide crash-recovery behavior for unsaved edits by using autosave snapshots or an equivalent recovery mechanism.
- On next launch after an abnormal termination, the application shall offer recovery of autosaved unsaved content where available.

### 3.11.1 Undo/Redo Transaction Semantics

Undo and redo behavior shall be predictable and user-centered.

Required behavior:

- a continuous drag gesture on map geometry shall commit as one undo step
- freehand line creation shall commit as one undo step per explicit completed draft
- batch inspector edits applied by one explicit commit action shall commit as one undo step
- any TH2 map-editor operation that mutates source text shall use one atomic source-write transaction that applies the text change and records its undo snapshot together via the shared map-source helper (`applySourceTextChangeWithSnapshot`) or an equivalent single abstraction
- text-driven and map-driven mutations shall both participate in the same document undo history for the active TH2 session
- TH2 map-editor undo history may be bounded to preserve memory use and input responsiveness during long drawing sessions
- failed or invalid operations shall not create empty undo entries

### 3.12 Preferences and File-Change Handling

The Qt application shall persist the user-facing settings that are necessary to restore a stable workflow.

Required persistent preferences include:

- recent projects list
- last opened project path, where accessible
- open tab restore state
- active tab restore state
- window size and position per top-level window type, where practical
- splitter and dock layout state, where practical
- visibility of the inspector and debug sidebar
- visibility and height of the help/documentation inspector
- project browser expansion state, where practical
- map editor viewport state, where practical
- map editor viewport state and automatic input-policy state
- background image adjustment state, where practical
- application language override, where the user has set one
- default `.th` / Therion config editor mode
- Therion executable path or runner configuration, where the user has set one
- editor preferences that affect visible behavior, such as folding state and search-bar mode if they are restored by the implementation

Preference behavior rules:

- changes to a visible preference shall take effect immediately when practical
- preferences shall survive application restart
- additional Therion runner command-line options shall remain session-only and shall not persist across application restarts
- if a preference cannot be restored exactly, the application shall fall back to a safe default rather than failing to start
- file-system refresh and project reload actions shall preserve user selections where possible

### 3.13 Pen, Touch, and Secondary Display Support

The Qt application shall support pen and stylus input for map editing on platforms and devices that expose such input through Qt.

Required behavior:

- the map editor shall accept stylus input for selecting and editing map content when the operating system and hardware provide it
- stylus input shall be treated as a first-class pointer source, not as a special-case mouse emulation path
- the map editor shall continue to work with a mouse or trackpad when stylus input is unavailable
- the map editor shall use automatic input handling for Sidecar and other pen-first secondary-display workflows without requiring a dedicated toolbar toggle
- on macOS, the application shall remain usable when the map editor is shown on a Sidecar secondary display
- when Apple Pencil input is available through Sidecar or equivalent macOS stylus routing, the map editor shall accept it as stylus input
- Sidecar support shall not be required for the core workflow; the application shall remain fully usable without it

Implementation note:

- pen pressure, tilt, and hover handling may be supported where the Qt backend exposes them, but they are not required for the MVP unless they are needed for a specific editing interaction

### 3.13.1 Accessibility Baseline

The Qt reimplementation shall provide a minimum accessibility baseline for desktop workflows.

Required behavior:

- all primary commands used in daily workflows shall be reachable from keyboard-only interaction
- focus order shall be deterministic and visible in project tree, editor, map toolbar, inspector, and console surfaces
- interactive controls shall expose accessible names and roles through the platform accessibility APIs
- UI contrast in both light and dark modes shall remain sufficient for primary text and control states

### 3.14 Localization and Multi-language Support

The Qt application shall support a multilingual user interface and locale-aware presentation without changing Therion file syntax or data semantics.

Required behavior:

- the initial release language set shall include English (`en`), Czech (`cs`), and Slovak (`sk`)
- additional language catalogs may be staged before release, but staged languages shall not be advertised, user-selectable, or loaded automatically until their application catalog and localized user manual are complete
- user-facing UI strings shall be localizable, including menus, toolbars, dialogs, alerts, empty states, inspector labels, search controls, console labels, and help-panel chrome
- platform/Qt-provided standard menu items and standard dialog chrome shall participate in application localization when the relevant Qt translation catalogs are available
- English shall be the required default source language and fallback language
- the application shall default to the operating system language when a supported translation is available
- the Settings dialog shall allow an explicit application-language override (`System Default`, `English`, `Czech`, `Slovak`); if immediate language switching is not practical, the change may take effect on next launch
- where the platform exposes a native per-app language mechanism, the Settings dialog shall write the explicit application-language override through that native mechanism and shall clear it when `System Default` is selected
- macOS application bundles shall advertise the shipped UI localizations to the operating system so the native per-app language selector can offer supported languages; macOS builds shall use the app-domain `AppleLanguages` override for explicit language choices
- Therion language elements such as commands, options, keywords, file-format tokens, and serialized document content shall remain in canonical Therion syntax and shall not be translated
- contextual help/documentation UI may be localized, but command names, accepted values, and syntax examples shall preserve the canonical Therion spelling used in files
- generated Therion command metadata shall store argument and option signatures separately from prose descriptions so UI rendering does not need runtime heuristics to identify canonical syntax
- file paths, project names, editor content, search terms, and user-entered labels shall support Unicode input, display, save, and restore behavior
- locale-sensitive formatting may be used for user-interface presentation, but Therion file serialization, numeric values, coordinates, and command invocation parameters shall remain locale-invariant
- the UI shall remain usable with longer translated strings and should avoid layout assumptions that only fit the English source text
- if a translation is incomplete or unavailable, the application shall fall back to the default source language without missing labels or broken placeholders

### 3.15 Map Symbol and Visual Style System

The Qt application shall use an explicit visual-style system for map rendering rather than relying only on hardcoded per-type drawing rules.

Required behavior:

- line, point, and area rendering shall be style-driven based on Therion object type and, where applicable, subtype
- the style system shall support separate definitions for line styles, point styles, area styles, and global interaction styles
- bundled map-style definitions shall prefer Therion's SKBB MetaPost symbol definitions when available and shall use UIS definitions as the next fallback reference before using project-specific approximations
- line styles shall support at least stroke visibility, optional edit-guide spine visibility, stroke color, stroke width, dash pattern, optional closed-line fill, and optional directional or repeated decorations
- closed-line fills shall support a background-clean mode so symbols such as `line rock-border -close on` can erase/fill their interior before the outline is drawn, matching Therion MetaPost `thclean` semantics
- line style decorations shall support built-in repeated or offset primitives including offset strokes, parallel strokes, ticks, rungs, teeth, repeated symbols, repeated wave marks, and `line slope` ticks driven by per-line-point `orientation` and `l-size`
- `teeth` line decorations shall render as Therion/MetaPost-like filled sawtooth segments based on the decorated line path, not as disconnected point-symbol triangles
- repeated line-symbol decorations shall define their repeated symbol geometry with `symbol_parts`, using built-in shape parts (`circle`, `oval`, `square`, `diamond`, `triangle`, `star`, `asterisk`, `plus`, `x`) and simple primitive parts (`line`, `polyline`, `polygon`, `ellipse`) so symbol-only line styles such as wall blocks, debris, pebbles, sand, and ladder-like features can be represented without custom SVG
- line decorations shall have decoration-local stroke and fill styling, spacing, optional MetaPost-style adjusted spacing for path-fitted repeated marks across repeated decoration kinds, side, offset, size, alternating short-mark scale for `line slope` ticks, and deterministic size, offset, angle, and along-line distance jitter parameters independent from the base line stroke
- line decoration `left` and `right` side semantics shall match Therion line orientation: following the line from its first point, `left` is the free-space side by default, `right` is the opposite side, and `-reverse` flips the rendered side
- the repository shall provide a developer style-gallery generator that renders all catalog-supported `point`, `line`, and `area` type/subtype combinations, including combinations with no specific style that therefore use type or global defaults, so bundled and override styles can be visually reviewed.
- point styles shall support at least fill color, stroke color, stroke width, `symbol_parts` geometry, symbol size, and label style
- point styles shall support an optional `label_field` style attribute that identifies a Therion option value to render next to the point symbol without hardcoding point-type-specific label extraction in the renderer
- point styles shall support an optional label font-size style attribute so bundled and user style metadata can tune label readability without renderer-specific type checks
- point styles shall support a label-orientation mode so text labels may either remain screen-aligned or rotate according to point `-orientation`
- line styles shall support an optional `label_field` style attribute that identifies a Therion option value to render along the line path without hardcoding line-type-specific label extraction in the renderer
- area styles shall support at least fill style and optional stroke style
- label styles shall support font size, weight, color, and positional offset relative to the rendered point symbol
- bundled station point styles shall render the point `-name` value when present, and bundled label point styles shall render the point `-text` value when present
- bundled label line styles shall render the line `-text` value when present by laying text along the line path; the line geometry shall determine text direction, spacing, and maximum fitted length
- point label rendering shall interpret common Therion label text markup used in `-text` values, including line breaks, multi-line alignment switches, thin spaces, basic font switches, right-to-left spans, and supported size switches; unsupported Therion label markup shall not be shown as literal control text when it is safe to ignore
- area fill styles shall support at least solid fill, hatch, and dot-pattern rendering
- area rendering shall honor Therion `-place bottom`, `-place default`, and `-place top` by assigning area fills and patterns to bottom, normal, or top render layers; default and bottom area rendering shall remain below normal line work, while explicit top area rendering may overlap normal line work
- area rendering shall honor default `-clip on` by clipping area fills and patterns to the owning scrap wall boundary when that boundary can be resolved, and shall preserve un-clipped rendering for `-clip off`
- dot-pattern area fill styles shall define their repeated symbol geometry with `symbol_parts`, using built-in shape parts (`circle`, `oval`, `square`, `diamond`, `triangle`, `star`, `asterisk`, `plus`, `x`) and simple primitive parts (`line`, `polyline`, `polygon`, `ellipse`), and each symbol part may override `fill_color`, `stroke_color`, and `stroke_width`
- dot-pattern area fill styles shall derive the repeated motif size from `symbol_parts` when no explicit motif `size` is configured
- dot-pattern area fill styles shall support deterministic `grid` placement and deterministic `scatter` placement for looser area-material fills
- dot-pattern area fill styles shall support optional deterministic per-symbol size jitter for non-uniform repeated symbol fills
- dot-pattern area fill styles shall support optional deterministic per-symbol angle jitter for non-uniform repeated symbol fills
- the map editor shall provide bundled default styles so supported Therion object types remain legible even when no external override exists
- when a style entry is missing for a specific type or subtype, the renderer shall fall back to a safe default style rather than failing to draw the object
- the style catalog shall be loaded from structured metadata such as JSON rather than being defined only in rendering code
- bundled map-style JSON resources shall be split into scoped files under `resources/map_object_styles/` using the same default, type, and subtype file naming convention as user overrides
- the application shall support user style override JSON files layered on top of the bundled defaults; if an override fails to decode, the application shall continue using the bundled styles and treat the failure as non-fatal
- user map-style override files shall be loaded from the application data location's `map_object_styles` directory (for example `~/Library/Application Support/Therion Studio/map_object_styles/` on macOS)
- user map-style override file names shall identify the target scope: `point.json`, `line.json`, and `area.json` for kind defaults; `<kind>.<raw_type>.json` for type styles; and `<kind>.<raw_type>.<raw_subtype>.json` for subtype styles
- user map-style override scopes shall be applied in default, type, then subtype order so more specific files take precedence regardless of lexical filename ordering
- selection, draft, edit-preview, handle, close-target, and direction-marker visuals shall also be style-driven or centrally configurable so interaction visuals remain consistent across object types
- style lookup shall not change Therion document semantics; changing a visual style shall affect rendering only unless the user explicitly edits the underlying TH2 content

### 3.16 Therion Syntax Highlighting Style System

The Qt application shall use an explicit text-editor style system for Therion syntax highlighting rather than relying only on hardcoded colors in the editor widget implementation.

Required behavior:

- syntax highlighting shall be driven by a structured highlight palette loaded from metadata such as JSON rather than being defined only inline in rendering code
- the highlight palette shall support distinct style roles for at least base text, block keywords, commands, parameters, numbers, booleans, strings, and comments
- Therion syntax highlighting shall be defined independently from the broader application theme and shall not be the place where window, panel, or editor-chrome theme colors are specified
- bundled default highlight styles shall include contrast-safe light and dark variants and shall be packaged with the application so the editor remains readable even when no override file exists
- active editors shall reload the effective syntax-highlight variant when the application appearance changes at runtime
- when a highlight-style entry is missing or invalid, the editor shall fall back to safe bundled or built-in defaults rather than disabling syntax highlighting
- the application may support a user override highlight-style file layered on top of the bundled defaults; decode failure of the override file shall be non-fatal and shall fall back to bundled styles
- changing the highlight style palette shall affect presentation only and shall not change tokenization rules, completion behavior, or document content

### 3.17 Application Theme and Appearance System

The Qt application shall define overall application appearance independently from Therion syntax highlighting.

Required behavior:

- the application theme shall define the appearance of non-document UI surfaces such as window backgrounds, toolbars, sidebars, inspectors, console surfaces, empty states, status bars, and other shared chrome
- editor chrome styling shall belong to the application theme or appearance system rather than to the Therion syntax-highlight palette
- the application theme shall centrally define at least line numbers, gutter dividers, expanded fold toggles, collapsed fold toggles, and fold-toggle hover state
- the application shall support both light and dark appearance modes
- the application theme shall provide coherent colors and contrast for both light and dark appearance modes across the main window, sidebars, inspectors, console surfaces, and editor chrome
- the TH2 visual map canvas shall use a stable light paper-style document surface and pasteboard in both light and dark application appearance modes so raster background images, `.xvi` sketch references, and Therion map symbols remain color-stable and readable
- the application may follow the operating-system appearance automatically or expose an explicit appearance preference, but dark mode support itself is mandatory
- when the effective operating-system appearance changes at runtime, the application shall update active light/dark theme rendering without requiring an application restart
- the application theme may use system colors, bundled theme tokens, or a combination of both, provided the resulting appearance remains consistent and accessible across supported platforms
- missing or invalid theme entries shall fall back to safe defaults rather than breaking editor or application usability
- changing the application theme shall affect appearance only and shall not change Therion tokenization, completion behavior, map style semantics, or document content

## 4. Data and File Handling Requirements

The reimplementation shall correctly handle Therion project files and related assets.

Required capabilities:

- parse and edit Therion source text without data loss
- preserve document formatting as much as practical when rewriting files
- preserve comments, unknown but syntactically valid directives, and unedited option lines without semantic loss when rewriting supported documents
- preserve the existing file encoding on save unless the user explicitly converts the file to UTF-8
- resolve non-UTF-8 Therion documents to concrete writable text encodings when opening and saving, including common legacy Central European encodings such as ISO-8859-2 where needed
- handle external file references used by the project
- preserve TH2 background-reference metadata such as `xth_me_image_insert` entries when editing or rewriting documents
- preserve scrap scale and related TH2 header metadata when rewriting scrap definitions
- support background images and sketch assets used by TH2 documents, including raster images and `.xvi` vector backgrounds
- maintain compatibility with existing Therion project structures
- parse and serialize Therion numeric values and coordinates using locale-invariant rules so localized decimal separators do not alter file content
- package the bundled map-style catalog with the application and load it deterministically at runtime
- load user map-style override files after the bundled catalog so user styles take precedence without modifying the application bundle
- package the bundled Therion syntax-highlight palette with the application and load it deterministically at runtime
- package the bundled application theme resources with the application and load them deterministically at runtime
- use UTF-8 by default and handle common text encodings robustly where needed

### 4.1 Round-Trip Compatibility Criteria

Round-trip compatibility is mandatory for trusted editing.

Required behavior:

- opening and saving a supported Therion document without user edits shall not change document semantics
- parser and serializer behavior shall be validated against a maintained compatibility corpus of representative project files
- compatibility-corpus verification shall be part of release-readiness checks

## 5. Cross-Platform Requirements

The Qt application shall behave consistently on macOS, Windows, and Linux.

Requirements:

- use a single shared codebase for the core application logic and UI
- respect platform conventions for keyboard shortcuts, menus, file dialogs, and drag and drop
- support high-DPI displays
- support both dark and light appearance modes
- handle platform-specific file path semantics correctly
- support Unicode file paths, input methods, and text rendering across all supported platforms
- avoid platform-exclusive assumptions in the core domain model
- keep the application responsive on all supported platforms
- follow each platform's native shortcut conventions for modifier keys where they differ
- use native file dialogs and native drag-and-drop behavior where available and practical
- ensure packaged releases include the bundled map-style resources required for correct map rendering
- ensure packaged releases include the bundled Therion syntax-highlight resources and bundled application-theme resources required for consistent editor and UI rendering
- the application shall feel native on each target platform as much as practical, including platform-appropriate interaction patterns, control behavior, keyboard conventions, menu structure, window management expectations, context-menu behavior, and file-dialog workflows
- where exact visual parity across platforms conflicts with native platform behavior, native platform behavior shall take priority unless it breaks documented functional requirements

### 5.2 Performance and Responsiveness Targets

The application shall define measurable responsiveness targets for core workflows.

Required behavior:

- opening and indexing a medium-size project shall complete without blocking the UI thread
- editing actions in text and map views shall remain interactive under normal project sizes
- map pan/zoom and selection feedback shall remain visually responsive while background work is in progress
- release readiness shall include a documented performance verification pass on each target platform

### 5.1 Release Packaging and Distribution

The release policy shall define how the application is packaged, signed, distributed, and updated for end users on each supported desktop platform.

Requirements:

- the packaging policy shall distinguish between internal preview builds and public production releases
- each production release shall publish a support matrix that identifies supported operating-system versions, supported CPU architectures, and provided package formats for macOS, Windows, and Linux
- internal preview builds may be unsigned or ad hoc signed and may use simplified packaging, provided the build is clearly marked as non-production and the installation steps are documented for testers
- the MVP shall provide end-user release artifacts for macOS, Windows, and Linux; users shall not be required to install a local Qt SDK or build the application manually in order to run the product
- macOS Homebrew releases shall build and install the application through a formula, declare Qt as formula dependencies, and shall not bundle Qt frameworks inside the app bundle
- macOS release support shall cover the last two major macOS versions available through the supported build and distribution channels where practical
- signed and notarized macOS `.dmg` or `.pkg` artifacts may be added as a later production distribution path when signing credentials are available
- Windows releases shall be delivered as an installer that bundles Therion Studio and the Qt runtime required by the application
- Windows release installers shall place the executable and deployed Qt runtime in a consistent runtime directory, preserving Qt plugin subdirectories such as `platforms/qwindows.dll` relative to the executable
- Windows release executables shall use the GUI subsystem so launching the installed application does not open a console window or tie application lifetime to a console host
- Windows release installers shall not bundle the external Therion command-line executable; users shall configure or install Therion separately through the existing runner settings
- Windows snapshot installer artifacts shall use a human-readable development artifact label such as `dev-<short_sha>` while keeping generated application and installer metadata on the current CalVer baseline
- Windows signed installers are preferred for production releases; unsigned installers may be used only for clearly documented internal preview builds
- Linux production releases shall provide at least one broadly portable distribution format, such as AppImage or an equivalent self-contained bundle; distro-specific packages may be added in addition to the primary artifact
- Linux production release workflows shall not execute mutable `continuous` third-party packaging binaries; externally sourced portable-package tooling shall come from maintained immutable releases, repository packages, or audited built-from-source inputs with recorded provenance
- Linux AppImage production workflows that use Qt's CMake deployment API shall build the portable artifact with a Qt version that supports Linux deployment through that API, shall ensure required Qt runtime libraries and plugins are staged inside the AppDir, shall launch-test the AppDir before packaging, and shall record the Qt package source, version, and selected package set in release provenance
- sandboxed Linux packages that include the integrated Therion run console shall either bundle a supported Therion command-line runtime or use an explicit audited host-execution bridge for user-configured host Therion executables
- preview sandboxed Linux packages may document sandbox limitations, but production sandboxed Linux releases shall not silently ship a compiler workflow that cannot run Therion under the package's sandbox/process model
- Linux snapshot artifacts shall use a human-readable development artifact label such as `dev-<short_sha>` while Debian package metadata shall use a valid Debian `Version` value that starts with the current CalVer baseline and includes git snapshot metadata
- Linux `.deb` release artifacts shall be built and named for the specific Ubuntu release baseline they target. The current `.deb` release baseline shall be Ubuntu 26.04, and Linux CI shall smoke-install that `.deb` on Ubuntu 26.04. Debian, other Linux distributions, and other Ubuntu releases shall be covered by the AppImage channel unless a matching distro-specific package is introduced.
- required Qt libraries and other non-system runtime dependencies shall be bundled with the application or installed by the package manager package so that first launch does not depend on a local Qt SDK or developer toolchain
- release artifacts shall include a branded dark cave-map ThS monogram application icon with yellow S accent for runtime windows and platform launch surfaces, with platform-specific icon metadata wired for macOS and Windows packages
- packaged releases shall store user settings, recent-project state, session data, and logs in standard user-writable per-platform locations rather than inside the installation directory
- updating from one released version to another shall preserve user settings and persisted session data where the stored data format remains compatible
- automatic update delivery is not required for the MVP, but the release process shall support a documented manual update path for each platform
- release artifacts shall include visible product version information and release notes or equivalent upgrade documentation
- release artifacts shall include the Therion Studio project license (`GPL-3.0-or-later`) and third-party notices for bundled resources
- the application shall not require administrator or root privileges for normal day-to-day use after installation, except where the target platform's installer flow requires elevation during installation itself

## 6. Architectural Requirements

The reimplementation should be organized as a modular desktop application.

Recommended module boundaries:

- core domain model for Therion documents and editing rules
- parser and serializer for Therion file formats
- project/file system services
- text editor subsystem
- map editor subsystem
- structure/indexing subsystem
- Therion process runner subsystem
- application shell and window management

Implementation guidance:

- keep document parsing and editing logic separate from UI widgets
- keep the map model independent from the rendering layer
- keep UI state separate from file content where possible
- make core logic testable without launching the full GUI
- avoid introducing unnecessary dependencies outside Qt and the standard library

### 6.1 Code Organization and Separation Requirements

The Qt reimplementation shall enforce clear code separation so the project remains maintainable as a long-lived desktop application.

Requirements:

- domain models, parsing, serialization, and editing rules shall live outside UI classes and shall not depend on widget-layer types
- UI views, windows, panels, delegates, and scene items shall not contain file I/O, parsing, encoding detection, project loading, or document-serialization logic
- application services shall own external interactions such as file access, process execution, metadata loading, and style/theme loading
- state used only for UI presentation shall be kept separate from persisted document content and separate from parser/domain state where practical
- map rendering code shall remain separate from map-editing domain rules so rendering changes do not require rewriting document-edit logic
- text-editor tokenization/highlighting logic, completion metadata logic, and editor widget plumbing shall remain separate responsibilities even when they cooperate at runtime
- modules and files shall be organized by responsibility rather than by generic helper buckets; generic catch-all names such as `Helpers`, `Utils`, `Misc`, or `Manager` should be avoided unless the contained code is truly cohesive
- large features should be split into focused files or subcomponents by responsibility instead of accumulating unrelated behaviors in a single class or translation unit
- public interfaces between subsystems shall be narrow and explicit so the text editor, map editor, Therion runner, and project/session services can evolve independently

### 6.2 Maintainability and Implementation Discipline

Requirements:

- the rewrite should prefer straightforward, testable code over speculative abstractions or framework-heavy indirection
- new dependencies outside Qt and the standard library should be avoided unless they provide clear, project-level value that outweighs packaging and maintenance cost
- fallback behavior for missing metadata, missing style/theme resources, and unsupported optional features shall be centralized rather than duplicated across views
- expensive reusable resources such as compiled regular expressions, parsed metadata catalogs, and style/theme catalogs should be cached or reused rather than recreated in hot rendering or editing paths
- logging, user-facing error presentation, and debug-only tooling shall be kept separate from core document mutation logic

### 6.3 Testing and Verification Requirements

Requirements:

- parser, serializer, project-loading, structure-indexing, Therion-runner coordination, and document-editing logic shall be testable without launching the full GUI
- shared logic that affects document integrity or cross-view synchronization should be covered by focused automated tests
- UI-specific tests may be added for critical end-to-end workflows, but they shall complement rather than replace tests for pure logic
- architectural refactors should preserve externally visible behavior unless the change is an intentional bug fix documented in the change history
- changes that affect model, parser, indexing, command routing, editor behavior, or session restore shall be verified by automated tests or an explicitly documented manual verification pass

## 7. Technology Guidance

The recommended baseline is Qt 6.

Suggested approach:

- use Qt Widgets for the main desktop shell unless there is a strong reason to choose QML for a specific surface
- use Qt model/view patterns for file trees, structure trees, and tabular data
- use a custom canvas or scene-based view for the map editor
- use Qt's text editing facilities, custom syntax highlighting, and completion APIs for the text editor
- use Qt's process APIs for Therion execution

If a component benefits from a more specialized implementation, it may be built with a custom widget or scene item, but the public application behavior should remain consistent.

## 8. Functional Acceptance Criteria

The reimplementation should be considered functionally complete when the following are true:

- a project can be opened on macOS, Windows, and Linux
- the project browser shows the expected Therion project tree
- Therion text files can be opened, edited, saved, and searched
- map documents can be opened, viewed, selected, and edited graphically
- undo and redo work across document mutations
- the structure sidebar reflects the current project contents
- Therion can be launched from inside the application and its output can be inspected
- selection and navigation behave consistently between text and map views
- the application remains stable and responsive on all supported platforms

### 8.1 Screen-Level Acceptance Criteria

The criteria below are intended for implementation verification and QA.

#### 8.1.1 Project Browser

- A project tree is visible after opening a project folder or project file.
- The tree shows folders and project files using distinct row icons.
- Clicking a file row opens the file in the editor area.
- Clicking a folder row toggles its expanded state.
- Clicking a folder row that is already selected collapses it and clears the selection.
- Empty-space clicks clear the browser selection.
- The browser context menu exposes creation, rename, delete, duplicate, and external-open actions where applicable.
- A `.th2` file exposes an explicit action to open it in the map editor.

#### 8.1.2 Text Editor

- Opening a Therion file shows it in a tabbed editor.
- Reopening an already open file activates the existing tab instead of creating a duplicate.
- Unsaved changes remain present when switching tabs.
- Save affects only the active document; Save All affects every dirty document.
- The editor supports syntax highlighting, completion, and code folding for Therion syntax.
- Find and Replace show an inline search bar with next, previous, replace, replace all, whole-word, and match-case controls.
- Global project search is available from the left Search activity pane and `Command/Ctrl+Shift+F`, lists file-grouped matches with source locations, and opens selected matches in raw mode.
- The editor shows a contextual help/documentation panel for Therion commands and options when metadata is available.
- In raw text-editing mode, the contextual help panel appears in a resizable right-side inspector column (not a bottom strip), and editor/help spacing remains visually consistent with Blocks mode.
- For `.th` and Therion config documents, `Raw`/`Blocks` mode controls appear in the full-width document command toolbar row above the tab strip.
- The application shows the active file path and encoding in a status area for the active document.
- For map-editor documents, the status area shows a color mode badge (`Select` green, `Insert` red) and updates when the mode changes.
- Non-UTF-8 files can be explicitly converted to UTF-8.
- Search and replace works in the current file and is usable for broader project search where implemented.
- When a TH2 file is active, text selection and map selection remain synchronized where the source location is known.

#### 8.1.3 TH2 Map Editor

- The map editor renders the currently open TH2 file as a 2D editable workspace.
- Closed TH2 line geometry shall render the closing segment between the final and first line vertices whenever at least two line vertices exist, including two-anchor closed Bezier curves.
- Existing TH2 `area` objects whose referenced same-scrap border lines form a closed face through line intersections shall render as filled areas without requiring those border lines to be individually closed.
- A TH2 document exposes an embedded mode selector with `Raw` and `Visual` modes.
- In the main window, the TH2 `Raw`/`Visual` mode selector is shown in the right-aligned controls of the full-width document command toolbar row above the tab strip.
- In the main window, TH2 map-pane detach/reattach (`Separate Map` / `Return Map`) is available in the same document command toolbar control area, after `Raw`, using screen-share/monitor-x icons for detach/return state.
- In the main window, when a TH2 tab is active, the document command toolbar includes left-side zoom and map-tool groups (`Zoom In`, `Zoom Out`, `Fit`, `Fit With Background`, `Select`, `Complete Draft`, separator, `Insert Scrap`, `Point`, `Line`, `Freehand`, `Area`, `Smart Area`) after `Undo`/`Redo`.
- In the main window, these TH2 zoom and map-tool groups are enabled only while the embedded TH2 tab is in `Visual` mode; in `Raw` mode they remain visible but inactive.
- In detached dedicated map-editor windows (without shared tab strip), an equivalent in-window top command toolbar remains available.
- In embedded `Visual` mode, the tab shows the graphical map editor plus a right-side map inspector (`Selection`, `Objects`, `Backgrounds` views).
- In embedded `Raw` mode, the tab shows the source text editor plus contextual help inspector.
- The graphical map pane does not show a separate persistent map-help panel.
- The same TH2 session can be shown in an embedded workspace and a detached map pane window without diverging document state.
- While the map pane is detached, the main tab keeps the raw text editor visible and the detached map window keeps map/inspector editing visible.
- Scrap nodes start expanded on first display.
- Selecting an object in the map editor updates the selected object state in the rest of the application.
- Clicking a map object row reveals the corresponding source line when it exists.
- Clicking an already selected object row again clears the current object selection.
- Visibility toggles affect only the selected object visibility state and do not change selection.
- Object-tree delete icon actions remove the corresponding source-backed object span after confirmation and can be reverted through undo/redo.
- Dragging and dropping map objects reorders or moves them according to the drop target.
- Undo and redo work for map mutations.
- The map command surface exposes drawing, fitting, zoom, undo/redo, and draft-completion actions in the top toolbar (main and detached map windows).
- Freehand line drawing serializes simplified bezier line geometry.
- The separate `Smart Area` map tool previews a clicked same-scrap closed face made from existing closed or intersecting lines, confirms with one source transaction, and may add only missing line IDs needed for the area reference without changing existing line geometry.
- Background layers support persisted position, visibility, gamma, opacity, `.xvi` rendering, and SVG rendering where applicable.
- Raster background image add/move/show-hide/gamma/remove operations write and maintain XTherion-compatible `xth_me_area_adjust`, `xth_me_area_zoom_to`, and `xth_me_image_insert` metadata in the TH2 source.
- Adding an SVG background from the TH2 Visual map editor shall write Mapiah-compatible `##MAPIAH## image_insert_v1` metadata with `format=svg`, document-relative paths where possible, transform fields, intrinsic size, and source viewBox fields derived from the SVG root metadata.
- PocketTopo Therion export (`.txt`) background import generates a plan or extended-elevation `.xvi` file, adds it as a background layer, and persists the reference through XTherion-compatible `xth_me_image_insert` metadata.
- The Backgrounds inspector exposes layer controls without editor-generated grid toggles or spacing fields; `.xvi` grid lines render only as part of `.xvi` background content.
- Map-driven scrap insertion writes XTherion-compatible default `-scale` metadata, while existing Therion/XTherion `-scale` forms are preserved unless explicitly edited.
- Selecting a scrap in `Selection` exposes manual scale calibration controls in a separate `Scrap Scale` section and writes XTherion-compatible 8-parameter `-scale [...]` metadata to the scrap command.
- Automatic input-policy handling supports pen-first workflows without a dedicated touch-controls toolbar button.
- Zoom, pan, and background-image adjustments persist for the session.

#### 8.1.4 Object Settings / Inspector

- The inspector always matches the current selected object.
- During point/line/freehand/area insertion, the Selection inspector is active before placement, shows pending type/subtype fields, supported point-value fields, and the resolved style preview, and the completed object is serialized with the edited pending values. Pressing `Esc` from the map canvas, command toolbar, or those pending Selection inspector fields shall exit insert mode; sufficiently complete line/area drafts shall be committed according to the existing draft-completion rule, while incomplete drafts shall be canceled.
- During point/line/freehand/area insertion, the Selection inspector metadata shall show the target scrap identifier and, when the document contains existing scraps, shall expose an `Insert into` selector for choosing the target scrap. When the document has no existing scrap, the Selection inspector shall not show the selector and shall instead explain with a visually distinct notice that the new object will create the generated draft scrap. The initial target shall be the selected object's enclosing scrap when a map object or scrap selection exists at insertion activation time; completed geometry shall be serialized before the selected target scrap's `endscrap` line.
- During scrap insertion, Insert Scrap creates a new scrap immediately, selects the created scrap in both source navigation and the Objects tree, and shows the created scrap in the Selection inspector for immediate ID/projection editing.
- Changing selection updates the visible object settings immediately.
- The `Selection` inspector shall group controls in this order: selected-object section, `Geometry`, contextual point/line-point details, `Line Point Actions`, and `Object Actions`, so object identity edits, object-level geometry state controls, point/line-point details, point/line-point actions, and object actions remain visually distinct.
- The selected-object section title shall use the selected object kind (`Scrap`, `Point`, `Line`, or `Area`) instead of a generic `Object` heading, and shall show source location plus the selected object's enclosing scrap identifier as compact metadata.
- The selected-object section shall expose quick-edit fields for common map-object identity attributes in a stable command-focused order. Scrap objects shall expose ID and `-projection`; scraps shall not expose type/subtype because scraps do not have type/subtype. Point, line, and area objects shall expose `ID (-id)`, type, and subtype where supported. Station points shall additionally expose `Name (-name)` after subtype rather than using station name as the ID field. Point types that support Therion `-value` shall expose a `Value (-value)` field in the selected-object section; edits shall insert, replace, or remove the `-value` option through the safe source-edit path, preserving bracketed Therion value groups and unrelated options/comments where practical. Type, subtype, and scrap projection shall be editable pick lists that allow catalog/default choices and manual custom values. Type and subtype catalog choices shall be shown in alphabetical order. Subtype pick lists shall include an empty choice that clears/removes the selected object's `-subtype`. Field changes shall rewrite the selected command line through the safe source-edit path without a separate `Apply` button and preserve unrelated options/comments where practical.
- For selected or pending point/line objects with type `label`, the selected-object section shall expose a `Text (-text)` quick-edit field. Editing it shall rewrite the object's `-text` option through the safe source-edit path and shall preserve unrelated options/comments where practical.
- The `Object Actions` section shall provide catalog-driven object settings and selected-object deletion through the standard confirmed source-delete workflow.
- The contextual point/line-point details section shall be titled `Geometry` when a standalone point is selected and `Line Point` when a line anchor or its control handle is selected.
- The contextual point/line-point details section shall not expose coordinate text fields; exact point, vertex, and control-point coordinates shall remain editable in Raw mode.
- For selected editable line vertices, the `Line Point Actions` section shall expose `Insert Before`, `Insert After`, `Delete Point`, and `Split Here` actions. At the first line vertex, `Insert Before` shall become `Extend Before`; at the last line vertex, `Insert After` shall become `Extend After`. Insert actions shall split the adjacent segment in the requested direction. Endpoint extension shall enter interactive line continuation mode from the selected endpoint; completing the continuation shall prepend or append the newly drawn vertices to the existing line block rather than creating a separate line object. `Split Here` shall be available only for interior vertices of non-closed lines that are not referenced as area borders; if the original line has an `-id`, the first split line shall keep it and the second split line shall receive a unique `<original-id>-split-N` identifier.
- For selected line-point anchors, the `Line Point` section shall read and write XTherion-compatible per-point line options. The line-point control group shall expose `<<`, `Smooth (-smooth)`, and `>>` checkboxes for incoming control handle, smooth tangent behavior, and outgoing control handle respectively. Disabling either control-handle checkbox shall remove that handle and force smoothness off; enabling `Smooth (-smooth)` shall ensure both handles exist and align them as a smooth tangent. Disabled smoothness shall serialize as `smooth off`; `orientation` shall be available only when the selected line type supports orientation; `l-size` shall be available for `line slope`, and the editable orientation/size handle shall scale with the same source length used by rendered slope ticks rather than clamping at a fixed maximum display length. The section shall expose `Subtype` only for line types with catalog-backed segment subtype values, and `Altitude (auto)` only for `line wall` points because Therion line-point `altitude <value>` is valid only with wall lines; `Altitude (auto)` shall write `altitude .`. New line-point options shall be written as separate indented rows (`smooth off`, `orientation ...`, `l-size ...`, `subtype ...`, `altitude .`) before the next coordinate row or `endline`.
- For selected line-point anchors, the `Line Point` section shall additionally expose an `Additional line-point options` editor that reads and rewrites non-coordinate standalone per-vertex option rows (for example `altitude`, `subtype`, `direction`, `adjust`) through the same atomic source-change-with-snapshot path used by other map-editor source mutations. Structured rows managed by visible dedicated controls, such as `subtype ...` and `altitude .`, shall be hidden from the additional editor while unsupported or unmanaged rows remain visible and round-trip preserved. These options shall be applied automatically when the editor loses focus; the workflow shall not require dedicated Apply/Clear buttons.
- Line-vertex handles shall visually indicate when the selected vertex carries additional standalone line-point options, and the vertex tooltip shall include a compact preview of those options.
- Deleting a line vertex that carries additional standalone line-point options shall require explicit user confirmation before the source rewrite is applied.
- Line-point control-handle and smoothness rewrites shall preserve supported standalone line-point option rows for the affected anchors, including `orientation` and `l-size`, so converting straight line segments into Bezier segments does not drop slope or orientation metadata.
- Line-point control-handle toggles, smoothness toggles, and control-point drags shall preserve the selected logical line vertex after the scene is rebuilt, even when the rewrite changes concrete source vertex indices.
- Coordinate rewrites from map geometry drags shall preserve existing coordinate precision where practical and shall write newly fractional coordinates with enough decimal precision to avoid visibly degrading Bezier tangents or repeated point edits.
- For `scrap`, `point`, `line`, and `area`, inspector configuration uses the same catalog-driven option workflow as structured block selection and shall expose parsed positional arguments as protected rows in the options table alongside parsed option rows. Point object settings shall expose `x`, `y`, and `type` rows; line and area object settings shall expose a `type` row; scrap object settings shall expose the positional `id` row. Object identifiers stored as `-id` shall remain option rows rather than being confused with positional arguments such as point X or line type.
- For `scrap`, the inspector provides dedicated manual scale calibration fields for picture points in pixels, real points, and unit, and applies them by rewriting the scrap `-scale` option.
- Catalog-driven option editors shall treat bracketed multi-token values such as scrap `-scale [...]` as one logical option value and shall not interpret negative numbers inside that value as option keys.
- Required fields are validated before commit.
- Station points use station name as their required name field.
- Non-station points do not expose station-name editing as a generic field.

#### 8.1.5 Structure Sidebar

- The structure sidebar shows only surveys, maps, and scraps.
- The structure tree does not include synthetic project-root or summary rows.
- Sidebar selection changes the active document context.
- The sidebar remains synchronized with the open project when the underlying structure changes.
- The activity rail exposes `Structure`, `Compiler`, and quick `Compile`; the `Files` view lives inside the shared project navigation sidebar, and no dedicated `Files` or `Map` rail pane is required.
- The sidebar can be collapsed and re-expanded through explicit UI controls.

#### 8.1.6 Therion Runner / Console

- Therion can be started from inside the application.
- Therion output appears in the console in the order it is produced.
- The UI remains responsive while Therion is running.
- The console shows config identity, effective working directory, command-line options, and the current run state.
- The main status bar shows the current compiler state or last compiler result, and clicking that indicator toggles the Compiler sidebar.
- The working-directory override can be cleared and the output can be copied.
- Failures are shown with actionable messages and exit status.
- Therion runs can be cancelled by the user.
- If a run is already active, the application shall define a deterministic policy: reject parallel runs, queue them, or replace the active run.
- Command-line argument and path quoting shall be platform-correct on macOS, Windows, and Linux.
- The console panel can be collapsed and re-expanded through explicit UI controls.

#### 8.1.7 Session Restore

- The last project reopens after restart when it is still accessible.
- The five most recently opened projects are available from the welcome surface and `File -> Recent Projects` when no project is open.
- Selecting a recent project reuses the normal open-project validation and updates recent-project ordering without duplicate paths.
- The open-project welcome surface identifies the active project and offers up to ten recent files from that project.
- The active project's ten most recent files are available from `File -> Recent Files`.
- Selecting a recent file reopens it in the appropriate editor and updates recent-file ordering without duplicate paths.
- Previously opened tabs are restored where possible.
- The active tab is restored when the underlying document still exists.
- Restoring state does not fail the application if a previously selected source location no longer exists.
- A project or document inside a standard macOS user folder shall be restored when accessible; an inaccessible path shall not prevent the remaining session state from being restored.

#### 8.1.8 Window and Document Lifecycle

- Closing a dirty tab prompts the user before discarding changes.
- Closing a dirty window prompts the user before discarding changes.
- Quitting the application prompts for all unsaved changes across all open windows.
- Opening the same TH2 document again focuses the existing map editor session when possible.
- The application never discards unsaved content without an explicit user choice or a successful save.

#### 8.1.9 Error Handling and Recovery

- Parse failures are shown to the user without erasing the document content.
- Save failures leave the document dirty and explain the cause.
- Missing files and missing external assets are reported as actionable warnings or errors.
- A file modified on disk while open produces a reload-or-keep-local decision when practical.
- Invalid map operations do not corrupt the document state.

#### 8.1.10 Preferences

- Recent projects are restored after restart.
- Window state and visible sidebars are restored when practical.
- Help/documentation inspector state is restored when practical.
- Therion runner configuration persists across restarts.
- The application starts with a safe default if a saved preference cannot be restored.

#### 8.1.11 Pen and Secondary Display Support

- The map editor accepts stylus input when the platform and hardware provide it.
- Mouse and trackpad input remain fully supported when stylus input is unavailable.
- The map editor provides automatic input-policy handling for pen-first workflows without requiring a dedicated touch-controls mode.
- On macOS, the app remains usable when the map editor is shown on a Sidecar secondary display.
- Apple Pencil input routed through Sidecar or equivalent macOS stylus support is accepted by the map editor.
- Sidecar and Apple Pencil support are not required for the core workflow to function.

#### 8.1.12 Localization and Multi-language Support

- The initial release ships with English, Czech, and Slovak UI coverage for the core application chrome and workflows.
- The application displays translated UI strings when launched in a supported non-default language.
- The Settings dialog can override the operating-system language with English, Czech, or Slovak, with the change applying no later than next launch.
- On macOS, the packaged app appears in the system per-app language selector as supporting English, Czech, and Slovak, and explicit Settings language choices are reflected through the app-domain language override used by that selector.
- If a user-selected or system language is unsupported, the application falls back to English without missing labels or broken placeholders.
- Menus, dialogs, toolbar labels, inspector labels, search controls, and console labels participate in localization.
- Platform/Qt-provided standard actions such as the macOS application menu `Preferences...` item display in the selected application language when matching Qt translation catalogs are installed.
- Bundled Czech and Slovak translation catalogs build with no unfinished application-string entries for the shipped UI coverage.
- Staged language catalogs can exist in the repository without being exposed to users; promoting a staged language to the supported set requires complete application translations, a localized user manual, placeholder validation, and runtime/packaging wiring.
- Therion commands, options, keywords, and serialized file content remain in canonical Therion syntax and are not translated.
- Unicode file paths, project names, and editor content can be opened, displayed, searched, and saved correctly.
- Locale-sensitive UI does not change the numeric formatting written into Therion files.

#### 8.1.13 Map Symbol and Visual Style System

- Supported line, point, and area types render using a style catalog rather than a single generic fallback appearance.
- Type-specific and subtype-specific styles are applied when matching catalog entries exist.
- Missing or invalid style entries fall back to bundled defaults without preventing map rendering.
- Point symbols support configurable `symbol_parts` geometry, size, fill, stroke, and label presentation.
- Point styles support an optional `label_field` that reads one Therion option value, for example `name` for `-name` or `text` for `-text`, and renders it next to the point symbol when present.
- Station point defaults render `-name`; label point defaults render `-text` with common Therion label formatting such as `<br>` line breaks, multi-line alignment tags, and `<thsp>` thin spaces, and rotate that text when explicit `-orientation` is present.
- Line styles support an optional `label_field`; bundled label line styles render `-text` along the line path so the label line geometry determines text direction, spacing, and maximum fitted length.
- Line symbols support configurable base stroke visibility, stroke width, dash pattern, optional closed-line fill, and optional decorations such as offset strokes, parallel strokes, ticks, rungs, teeth, repeated symbols, or slope ticks driven by line-point `orientation` and `l-size`.
- Line decoration side semantics match Therion orientation; the selected-line side marker points to the same side used by `side: "left"` decorations before `-reverse`.
- Closed `rock-border` lines clean-fill their interior with the map background before drawing the outline, so they cover default area fills and patterns like Therion's UIS MetaPost symbol.
- Repeated line decorations, including `line slope` ticks, can be fitted to the path with MetaPost-style adjusted spacing; alternating long/short slope ticks preserve the configured ratio.
- Area symbols support configurable solid, hatch, or dot-pattern fills and optional strokes.
- Area symbols honor `-place bottom`, `-place default`, and `-place top` render layering; default/bottom areas remain below normal line work, and explicit top areas may overlap normal line work.
- Area symbols honor default `-clip on` scrap-boundary clipping when the owning scrap wall boundary can be resolved, and preserve un-clipped rendering for `-clip off`.
- Dot-pattern area fills support configurable repeated `symbol_parts` geometry with per-part fill, stroke, and stroke-width overrides.
- Dot-pattern area fills support deterministic grid and scatter placement modes.
- Dot-pattern area fills support optional deterministic per-symbol size jitter.
- Dot-pattern area fills support optional deterministic per-symbol angle jitter.
- Selection, hover, draft, and edit-preview visuals are applied consistently through the shared style system; selection uses red overlays, hover uses azure/cyan overlays, and normal rendered object colors remain style-driven.
- Line/area vertex visuals and selection-highlight overlays are visible for selected objects and are suppressed for non-selected objects in normal editing view.
- Line control handles/connectors are visible only for the selected line vertex (or its selected control handle) and remain hidden for other vertices.
- Packaged builds include the style resources required to render the map consistently on all supported platforms.

#### 8.1.14 Therion Syntax Highlighting Style System

- Syntax highlighting uses a structured Therion-specific palette rather than a single hardcoded color scheme embedded only in the editor widget code.
- Distinct highlight roles exist for base text, blocks, commands, parameters, numbers, booleans, strings, and comments.
- Therion syntax-highlight rules are defined independently from broader application theme colors.
- Bundled syntax-highlight styles include light and dark variants, and active editors switch variants when runtime appearance changes.
- Missing or invalid highlight-style entries fall back to bundled or built-in defaults without disabling editor usability.
- Packaged builds include the style resources required to render syntax highlighting consistently on all supported platforms.

#### 8.1.15 Application Theme and Appearance System

- Non-document UI surfaces use a defined application theme rather than ad hoc per-widget colors.
- Editor chrome uses application-theme styles for line numbers, gutter dividers, and fold-toggle states.
- The application supports both light and dark appearance modes.
- The application theme remains coherent and legible in both light and dark modes across the main window, sidebars, inspectors, console, and editor chrome.
- Runtime operating-system light/dark appearance changes are applied in the running application without requiring relaunch.
- Missing or invalid theme entries fall back to safe defaults without breaking application usability.
- Packaged builds include the theme resources required to render the application chrome consistently on all supported platforms.

#### 8.1.16 Release Packaging and Distribution

- A documented support matrix exists for each release, covering operating-system versions, CPU architectures, and package formats.
- Internal preview builds are explicitly identified as non-production when they are not fully signed or notarized.
- macOS Homebrew releases install normally through the formula and run against Qt dependencies installed by Homebrew.
- Windows installer artifacts install Therion Studio and the required Qt runtime, including the Qt `windows` platform plugin, without requiring a local Qt SDK or preinstalled Qt runtime.
- Windows installer artifacts do not install the external Therion command-line executable; the runner remains configurable by path.
- Linux release artifacts launch on the supported target environments using the documented package format.
- Linux portable release artifact generation records the packaging tool source and does not depend on mutable `continuous` binary downloads.
- Sandboxed Linux release artifacts verify or explicitly document the integrated Therion runner execution strategy.
- Updating the application preserves user settings and recent-project/session data when the release notes do not declare a breaking migration.
- End users can install and run the released product without building from source.
- Release artifacts expose the `GPL-3.0-or-later` project license and bundled-resource third-party notices.

## 9. Suggested Reimplementation Order

A staged migration is recommended.

### 9.0 Scope Split

The first Qt release should be planned around two buckets:

- MVP: the smallest complete product that can open projects, edit files, edit TH2 maps, inspect object metadata, run Therion, and restore a usable session
- Post-MVP: improvements that refine editor ergonomics, expand automation, and add secondary tools without blocking the first usable release

### 9.1 MVP Requirements

The MVP shall include the following capabilities at a minimum:

- open a Therion project on macOS, Windows, or Linux
- browse the project tree and open files in tabs
- edit and save Therion source files
- provide Therion syntax highlighting and basic completion assistance
- provide the inline find/replace controls, contextual help panel, and encoding visibility needed for daily text-editing workflows
- provide localization-ready UI infrastructure and support the initial release language set of English, Czech, and Slovak without altering Therion syntax or serialized file content
- provide the bundled Therion syntax-highlighting palette and bundled application-theme resources needed for consistent editor and UI presentation
- support both light and dark appearance modes in the shipped application theme
- open TH2 files in a graphical editor
- provide the bundled map-style catalog and style-driven rendering needed for supported line, point, and area symbol types
- support synchronized text/map TH2 editing through split or equivalent embedded map workflows in addition to the dedicated map surface
- select, move, and inspect scraps, lines, points, and areas
- edit point, line, and area properties with undo and redo support
- keep selection synchronized between the text editor, map editor, and object inspector when the source location exists
- expose the structure sidebar
- run Therion, configure the run context, and show the console output
- restore the last project and previously open tabs when possible
- expose the documented menu commands and shortcuts
- prompt before discarding unsaved changes and report recoverable errors clearly
- produce packaged builds for macOS, Windows, and Linux according to the documented support matrix and packaging policy; public production releases shall satisfy the signing/notarization requirements, while internal preview builds may use the documented non-production path
- persist the core workflow preferences required to restore the application state

### 9.2 Post-MVP Requirements

The following items may be delivered after the MVP, provided they do not regress the MVP behaviors:

- project-wide replace and advanced project-search options beyond literal text, whole-word, and case-sensitive search
- richer map-object list presentation polish beyond the required fields
- a user-facing style editor for map symbol customization beyond bundled and override-file workflows
- a user-facing style editor for Therion syntax-highlighting customization beyond bundled and override-file workflows
- a user-facing application-theme editor or theme-switching system beyond bundled defaults and override-file workflows
- advanced background image and sketch-layer ergonomics
- future smart-trace-assisted line creation with explicit preview, acceptance, bezier simplification, and undo semantics
- expanded debug and diagnostic sidebar tools
- stronger session restoration of viewport, selection, and window layout state
- additional quality-of-life shortcuts or menu actions that do not conflict with the documented command set
- broader translation coverage for extended documentation/help content beyond the core UI string catalogs
- deeper pen/stylus ergonomics, including hover, pressure, and tilt handling where available
- Sidecar-specific refinements for macOS secondary-display workflows

### 9.3 Implementation Milestones

The following milestones are intended to function like a worklog: each milestone has a concrete output and a clear exit condition.

| Milestone | Goal | Exit criteria | Primary deliverables |
|---|---|---|---|
| M0 | Project audit and architecture plan | Core modules, data flow, and file format responsibilities are documented and agreed | Qt module breakdown, migration notes, parsing strategy |
| M1 | Core data foundation | Therion project files can be parsed, represented, and serialized without data loss for supported cases | Domain model, parser, serializer, project loader, compatibility corpus |
| M2 | Text editor baseline | Therion files can be opened, edited, searched, folded, and saved in tabs | Tabbed editor, syntax highlighting, completion, search/replace, save logic |
| M3 | Project navigation and structure | Users can browse the project tree, open files, and navigate structure state | Project browser, structure sidebar, document/tab coordination |
| M4 | Map editor baseline | TH2 files can be opened, inspected, and edited graphically with undo/redo | Map canvas, selection, inspector, object list, geometry editing |
| M5 | Therion runner and session persistence | Therion can be run from the app and the workflow can be restored after restart | Runner integration, console output, recent projects, restore logic |
| M6 | Hardening and release readiness | Known error cases are handled, packaged release artifacts are produced, and the MVP checklist is satisfied on all target platforms | Error handling, file-change prompts, preferences, production signing/notarization where applicable, preview-build packaging path, support matrix, QA pass |

Milestone rules:

- each milestone shall end with a reviewable build or documented prototype
- milestone scope shall be additive; later milestones shall not require rewriting completed core behavior unless a defect is being fixed
- if a milestone reveals an architectural dependency, the team may split it into smaller sub-milestones, but the exit criteria shall remain explicit
- milestone acceptance shall be based on the screen-level criteria in section 8 and the MVP requirements in section 9.1

### 9.4 Worklog Usage and Progress Tracking

`WORKLOG.md` shall be used as the operational progress tracker for the rewrite, while this specification remains the source of truth for requirements and milestone order.

Worklog rules:

- before starting a new implementation session, identify the active milestone from section 9.3 and the next unfinished phase item from sections 9.1 to 9.3
- use `WORKLOG.md` to record the current milestone, in-progress substeps, notable implementation decisions, blockers, and the next recommended action
- each meaningful implementation step should leave a short worklog note describing what changed, how it was verified, and whether the change advanced or completed a milestone substep
- when a requirement change alters behavior, scope, compatibility, or architecture, record that decision in `WORKLOG.md` and cross-reference the relevant specification section
- if a task reveals unexpected dependencies or forces a milestone split, update `WORKLOG.md` to show the revised substep order rather than silently changing the plan
- unresolved questions, known regressions, or temporary shortcuts shall be recorded explicitly in `WORKLOG.md`; they shall not remain implicit in code alone

Recommended implementation order:

1. complete M0 by documenting Qt module boundaries, data flow, and compatibility constraints
2. complete M1 by delivering parser, serializer, project loading, and document-domain foundations
3. complete M2 by delivering the text-editor baseline, including highlighting, completion, search, and save behavior
4. complete M3 by delivering project navigation, structure browsing, and document coordination
5. complete M4 by delivering the TH2 map editor, selection model, inspector, and geometry editing workflows
6. complete M5 by delivering Therion runner integration, console workflows, and session restore
7. complete M6 by hardening error handling, packaging, preferences, release support matrix, and final MVP verification

Progress-following rules:

- progress shall be measured against milestone exit criteria in section 9.3 and acceptance criteria in section 8, not only against code volume or number of changed files
- a milestone should be treated as complete only when its exit criteria are met and a reviewable build or documented prototype exists
- after each milestone, the team or agent should record which acceptance criteria were verified, which remain open, and what the next milestone entry point is
- if implementation work begins to drift outside the active milestone, the worklog should call that out explicitly and either defer the work or record why the dependency is necessary now

## 10. Out of Scope

The following are not required for the initial reimplementation unless explicitly added later:

- a redesign of the Therion file format
- new authoring features unrelated to the current app
- cloud synchronization or collaboration
- mobile or tablet support
- plugin architecture
- compatibility with unrelated cave-survey formats

## 11. Delivery Expectations

The deliverable should include:

- a working Qt desktop application for macOS, Windows, and Linux
- the core Therion project workflows listed in this document
- documentation for build, run, and packaging steps
- release packaging artifacts and installation/update instructions for macOS, Windows, and Linux
- a documented release support matrix covering operating-system versions, architectures, and package formats
- automated tests for core parsing and editing logic where practical
- a clear architecture that separates domain logic from UI code

## 12. Notes for Implementation Teams

The current Swift application is a useful behavioral reference. The reimplementation should preserve user-facing behavior first, then improve implementation quality where Qt makes that possible.

When there is a tradeoff between exact visual fidelity and maintainability, prioritize consistent behavior, document integrity, and cross-platform reliability.

## 13. Guidance for AI Implementation Agents

This section is workflow guidance for coding agents and other automated implementation tools working on the Qt rewrite.

Agent workflow rules:

- read this specification, `WORKLOG.md`, and the repository architecture guidance before starting substantive implementation work
- treat the behavior in sections 3, 4, 5, and 8 as the source of truth for user-visible behavior unless a later approved decision explicitly replaces it
- work milestone-by-milestone using section 9; do not jump ahead to polish or speculative features before the current milestone exit criteria are satisfied
- preserve file-format compatibility and document integrity over visual fidelity or internal implementation convenience
- prefer structured metadata sources and bundled JSON resources over runtime heuristics when implementing completion metadata, syntax highlighting palettes, map styles, or other rule-driven editor behavior
- keep model, service, and UI responsibilities separate according to section 6; if a change crosses those boundaries, split it into smaller steps
- avoid broad rewrites when behavior-preserving incremental changes are possible
- do not introduce new user-facing features outside the documented MVP scope unless the change is explicitly requested and documented
- treat missing optional metadata, theme resources, or style resources as recoverable conditions with safe fallbacks rather than fatal errors
- use the current Swift implementation as a behavioral reference, but do not copy architecture mistakes that conflict with the separation requirements in section 6

Agent delivery rules:

- after each meaningful step, verify the affected behavior against the relevant acceptance criteria in section 8
- update `WORKLOG.md` when a milestone, requirement area, or architectural decision is materially advanced or changed
- treat `WORKLOG.md` as the session-to-session handoff document for current milestone status, blockers, verification notes, and the next implementation step
- keep preview-build and production-release requirements distinct when working on packaging or release automation
- when making schema or resource-format decisions, document whether the decision preserves compatibility with the existing JSON/resource contract or intentionally introduces a new contract
- if a requirement is ambiguous, prefer the more conservative behavior-preserving interpretation and record the open question rather than silently inventing product behavior


## 14. Current Application Functionality Inventory (Swift Reference)

This section documents the currently implemented Therion Studio functionality that the Qt rewrite shall treat as the behavioral reference baseline.

### 14.1 Project and Workspace

Required parity scope:

- open, close, and reopen project folders
- project tree navigation with expansion state and file/folder context actions
- tabbed multi-document workflow with dirty tracking
- active-tab persistence and project/session restore
- project-scoped close semantics with unsaved-change prompts

### 14.2 Text Editing

Required parity scope:

- Therion-focused text editing for `.th`, `.th2`, and config files
- syntax highlighting driven by Therion token categories
- inline completion for commands/options/values with metadata-backed suggestions
- folding of structured Therion blocks
- inline find/replace bar with next/previous and replace modes
- save, save-all, and encoding visibility/conversion workflows
- contextual Therion help pane tied to caret context

### 14.3 TH2 Session Model and Synchronization

Required parity scope:

- TH2 parsed document model with scraps, lines, points, and areas
- stable object identity across reparses where source anchors still resolve
- synchronized selection between text editor, map canvas, object list, and inspector
- source-line reveal from graphical selection and object-list actions
- robust behavior when anchors cannot be resolved after edits/reparse

### 14.4 Map Editor Core Interaction

Required parity scope:

- embedded TH2 workspace with `Raw`/`Visual` modes plus detachable map window for the same TH2 session
- selection, hover, visibility toggles, and object-focused editing
- viewport controls: pan, zoom, fit geometry, fit geometry+background
- input-mode-aware navigation behavior for touchpad, mouse, and stylus workflows
- automatic input-policy behavior suitable for pen-first workflows

### 14.5 Map Geometry Editing

Required parity scope:

- point insertion/editing/deletion
- line insertion/editing/deletion including:
  - vertex insertion/removal
  - control-handle toggles and smoothness changes
  - line closing/opening and reverse state editing
  - freehand line creation
- area creation and editing, including source-line-linked borders
- map mutation undo/redo coverage

### 14.6 Map Semantics and Styling

Required parity scope:

- style-catalog-driven rendering for lines, points, and areas
- subtype-aware style lookup with safe fallback behavior
- point symbol orientation rendering and editing for command-catalog-supported `-orientation` objects, with style-controlled label text rotation
- directional line decorations (ticks, teeth, repeated symbols, offset strokes, parallel strokes, and rungs) with reverse-aware orientation behavior
- line-point orientation editing with normalized degree range (`0..<360`)
- default orientation behavior tied to local line tangent and side semantics
- background layers including raster imagery and `.xvi` references
- persisted background layer adjustments (visibility, gamma, opacity, placement)

### 14.7 Sidebar and Inspector Ecosystem

Required parity scope:

- project files sidebar
- project structure sidebar
- map inspector `Objects` view with selection-coupled behavior
- object settings inspector for scrap/line/point/area
- line-point options editing (orientation and l-size)
- debug sidebar and inspector diagnostic surfaces
- shared document inspector infrastructure for common views such as `File`, with editor-specific views supplying only their own content panels

### 14.8 JSON/Metadata-Driven Runtime Configuration

Required parity scope:

- map object settings panel generated from structured option schema
- enum values sourced from Therion metadata rather than duplicated literals
- map visual styles loaded from structured style catalog with override layering
- syntax highlight styles loaded from structured palette metadata
- graceful fallback when overrides or optional metadata fail to decode

### 14.9 Therion Runner and Console

Required parity scope:

- configure/discover Therion executable and run context
- execute Therion asynchronously without UI blocking
- stream stdout/stderr to console surface with run status and exit result
- keep console output copyable and diagnostically useful after failures

### 14.10 Windowing and Lifecycle

Required parity scope:

- main editor window lifecycle
- detachable map window lifecycle synchronized to active TH2 session
- console window/surface lifecycle
- unsaved-change protection on tab close, window close, project close, and app quit

## 15. Implementation Path (Workstreams + Gates)

The rewrite shall execute as parallel workstreams with explicit integration gates, not as isolated feature spikes.

### 15.1 Workstreams

1. Domain and File Fidelity
- parser/serializer round-trip guarantees
- TH2 identity/anchor stability rules
- compatibility corpus and regression suite

2. Text Editor Parity
- editing, highlighting, completion, folding, find/replace, help panel
- encoding workflows and save semantics

3. Map Editor Parity
- canvas rendering, interaction, geometry editing, background layers
- style-catalog application and directional semantics

4. Sidebar/Inspector Parity
- object list interactions and settings-panel behavior
- schema-driven settings generation and metadata-backed enums

5. Runner and Session Lifecycle
- Therion process execution and console state model
- session restore, window lifecycle, and unsaved-data safety

### 15.2 Integration Gates

- Gate A: File Fidelity Gate
  - corpus-based parse/serialize verification passes
  - no semantic loss on open/save without edits

- Gate B: Editing Parity Gate
  - text workflows and map workflows pass section 8 acceptance checks
  - selection synchronization works across text/map/sidebar/inspector surfaces

- Gate C: Interaction Semantics Gate
  - directional decoration, orientation defaults, reverse handling, and input-mode behavior are validated

- Gate D: Cross-Platform Gate
  - packaging and runtime smoke checks pass on macOS/Windows/Linux
  - core workflows remain responsive and stable on all targets

### 15.3 Delivery Rule

No feature area is considered "ported" until:

- behavior is validated against section 8 acceptance criteria
- regressions are covered by automated or explicitly documented manual verification
- outstanding deviations from this specification are recorded in `WORKLOG.md` with owner and follow-up step
