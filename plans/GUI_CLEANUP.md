# GUI Cleanup Plan

This plan describes how to separate visual styling and UI construction from application logic while keeping the current Qt Widgets application shippable. The immediate goal is cleaner Qt Widgets code with centralized styling and lower-risk UI changes. Qt Quick/QML is only a possible future view technology, so the cleanup should avoid locking application behavior into widget internals.

## Goals

- Centralize visual decisions such as colors, spacing, borders, radii, icon sizes, row heights, and control states.
- Keep UI construction, styling, behavior wiring, and domain logic in separate layers.
- Avoid broad rewrites that risk map/source synchronization, undo/redo, validation, or project indexing.
- Keep future view technologies, including possible QML views, able to consume presenter/view-model contracts rather than widget internals.
- Preserve native platform behavior where it works well, especially complex controls such as combo boxes, scroll bars, spin buttons, menus, and dialogs.

## Non-Goals

- Do not migrate any surface to QML as part of this plan.
- Do not add Qt Quick dependencies unless a separate future decision explicitly starts that work.
- Do not replace the map canvas/editor scene as part of the first cleanup phase.
- Do not move parser, source mutation, validation, file IO, settings, or process execution into UI style or view layers.
- Do not introduce a generic `Utils`, `Helpers`, or catch-all `widgets` folder.

## Target Ownership

### App-Wide UI Layer

Use `src/app/ui/` for application-wide UI primitives:

- style tokens
- palette-derived colors
- standard spacing, radius, and control metrics
- small stylesheet builders
- icon sizing helpers
- native-control policy helpers

This layer must not know about map objects, Therion commands, documents, validation findings, or project state.

Current baseline:

- `src/app/ui/` already contains application style tokens, style sheets, segmented-control styling, and control metrics.
- Shared text-editor inspector foundations already exist under `src/app/text_editor/`, including inspector panel styling and context-help/document inspector components.
- The map editor has several focused UI/context/controller files, including inspector background UI, inspector panel UI, object contexts, selection contexts, scene contexts, viewport input contexts, and detached-pane delegates.
- Main window and runner code has already been split into multiple service/controller/builder/wiring files, but `MainWindow.cpp` and several top-level shell files remain orchestration-heavy.

### Feature UI Layers

Keep feature-specific UI near the owning feature:

- `src/app/text_editor/` for shared Raw/Blocks/Map editor shell and inspector foundation
- `src/app/text_editor/raw_editor/` for raw-editor widgets and views
- `src/app/text_editor/block_editor/` for block-editor widgets and views
- `src/app/text_editor/map_editor/` for map-editor widgets and views

Feature folders may contain feature-specific style files when the style is not reusable application-wide.

### Presentation Contracts

Introduce presenter/view-model contracts where UI state is currently assembled directly in widgets. These contracts should be plain value types or narrow QObject-free interfaces where practical.

Qt Widgets should consume these contracts first. If QML is ever introduced later, it should bind to the same contracts instead of requiring domain or workflow code to be moved out of widgets at that time.

## Proposed Folder Shape

```text
src/app/
  ui/
    ApplicationStyleTokens.h
    ApplicationStyleTokens.cpp
    ApplicationStyleSheets.h
    ApplicationStyleSheets.cpp
    ApplicationControlMetrics.h
    ApplicationControlMetrics.cpp

  text_editor/
    InspectorPanel.h
    InspectorPanel.cpp
    InspectorPanelStyle.h
    InspectorPanelStyle.cpp
    InspectorPanelTokens.h

    raw_editor/
      RawEditorViewModel.h
      RawEditorViewModel.cpp
      RawEditorStyle.h
      RawEditorStyle.cpp

    block_editor/
      BlockEditorViewModel.h
      BlockEditorViewModel.cpp
      BlockEditorStyle.h
      BlockEditorStyle.cpp

    map_editor/
      MapEditorViewModel.h
      MapEditorViewModel.cpp
      MapEditorInspectorSelectionUi.cpp
      MapEditorInspectorBackgroundUi.cpp
      MapEditorInspectorObjectsUi.cpp
      MapEditorInspectorStyle.h
      MapEditorInspectorStyle.cpp
```

The exact files should be introduced incrementally only when there is code to move into them.

## Phase 1 - Maintain Style Tokens

Keep app-wide style token helpers as the place for reusable visual constants and palette-derived values. Extend them when new UI cleanup work would otherwise add repeated literals to widgets.

Move constants such as:

- control border radius
- inspector field radius
- toolbar button size
- sidebar rail button size
- section spacing
- standard icon extents
- row heights
- focus border policy

Rules:

- Token helpers may use `QPalette`.
- Token helpers must not return feature-specific text or inspect application state.
- Prefer named functions over raw global constants when values depend on palette or platform.

Verification:

- Build `TherionStudio`.
- Run `python3 scripts/check_structure_constraints.py`.
- Compare light/dark screenshots of the map inspector and sidebars.

Next slices:

- Replace one repeated spacing/radius/icon-size literal cluster with `ApplicationControlMetrics` or `ApplicationStyleTokens`.
- Keep feature-specific colors or map-rendering colors out of app-wide tokens unless another feature actually reuses them.

## Phase 2 - Continue Stylesheet Builder Extraction

Move remaining long inline stylesheet strings out of widget constructors and keep existing style builders focused.

Use focused style builders such as:

```cpp
QString inspectorPanelStyleSheet(const QPalette &palette);
QString inspectorSegmentedControlStyleSheet(const QPalette &palette);
QString sidebarActivityRailStyleSheet(const QPalette &palette);
QString workspaceCommandBarStyleSheet(const WorkspaceCommandBarStyleInput &input);
```

Rules:

- Style builders return styles only.
- No widget allocation.
- No signal wiring.
- No document, map, project, or validation logic.
- Keep selectors semantic and scoped by object names/properties.
- Avoid styling native subcontrols unless there is a documented reason.

Verification:

- Build `TherionStudio`.
- Run focused UI smoke tests where available.
- Manually check light/dark mode for inspector fields, combo boxes, sidebars, status badges, and toolbar buttons.

Next slices:

- Extract one remaining long inline stylesheet from a widget constructor into a focused style builder.
- Prefer existing style files before adding new `*Style` modules.
- Do not combine stylesheet extraction with layout, label, or behavior changes.

## Phase 3 - Add Semantic UI Object Names

Replace broad stylesheet selectors with semantic object names and dynamic properties.

Examples:

- `inspectorPanel`
- `inspectorField`
- `inspectorCombo`
- `inspectorLayerTree`
- `inspectorSegmentButton`
- `statusModeBadge`
- `sidebarRailButton`

Rules:

- Do not style all `QComboBox` or all `QTreeView` globally unless the style is intentionally application-wide.
- Prefer scoped selectors under owning containers.
- Use dynamic properties for state that is visual only, such as severity or mode.

Verification:

- Confirm style changes do not affect raw editor text widgets, map canvas, or native dialogs unintentionally.
- Check dark/light screenshots of Raw, Blocks, Map, Search, Validation, Compiler, Settings, and Help.

Next slices:

- Add semantic object names only when replacing broad selectors or stabilizing a style builder.
- Avoid object-name churn in widgets whose styling is not being touched.

## Phase 4 - Split UI Construction From Behavior Wiring

Separate widget creation from controller/presenter wiring.

Pattern:

- `*Ui` files create widgets and layout.
- `*Controller` files wire behavior and update state.
- `*Style` files provide visual policy.
- domain/application services own source edits, validation, IO, and process work.

Good examples to continue:

- `MapEditorTabInspectorBackgroundUi.cpp`
- `MapEditorTabInspectorControllerDelegates.cpp`
- `MainWindowTherionConsoleBuilder.cpp`
- `MainWindowTherionConsoleWiring.cpp`

Candidate splits:

- `MapEditorTabInspectorPanelUi.cpp` into selection/background/objects/file UI builders
- `MainWindowSidebar.cpp` into structure/search/validation/compiler page builders
- status bar badge construction vs status update logic

Current preferred split order:

1. Status bar badge construction vs status update logic, because it has limited source-model risk.
2. `MainWindowSidebar.cpp` page-builder extraction, one sidebar page at a time.
3. Map inspector file/background/selection/object UI split only when a concrete inspector change needs it.

Verification:

- Existing tests must pass.
- Add focused tests for extracted pure style or view-model state where useful.
- No user-visible behavior should change in this phase.

## Phase 5 - Introduce View Models For View-Technology Readiness

Create view models that represent UI state without exposing widgets.

Start with inspectors because they are currently widget-heavy and are a good boundary for separating state from widget implementation:

- `MapInspectorSelectionViewModel`
- `MapInspectorBackgroundViewModel`
- `ValidationPanelViewModel`
- `CompilerPanelViewModel`
- `ProjectStructureViewModel`

View models should expose:

- labels and display text
- enabled/visible state
- selected item identity
- list rows
- validation severity
- editable field values
- commands/actions as semantic intents

They should not expose:

- `QLineEdit`
- `QTreeView`
- `QGraphicsItem`
- `QTabWidget`
- raw widget pointers

Qt Widgets controllers should render these view models. The benefit is better testability and cleaner widget code now, while preserving the option for another view layer later.

Verification:

- Add unit tests for view-model state transitions.
- Check that core state and actions are not reachable only through widget internals.

Next slices:

- Start with a read-only panel or status surface before editable map inspector rows.
- Define one narrow view model for an existing repeated row/list state rather than a whole inspector at once.
- Avoid view-model work that would duplicate source parser, validation, project-index, or map selection state.

## Phase 6 - Define UI Intent Interfaces

Replace UI callbacks that directly mutate source or application state with semantic intents.

Examples:

- `setObjectType(QString type)`
- `setLinePointSubtype(QString subtype)`
- `moveBackgroundLayer(int from, int to)`
- `setBackgroundLayerPosition(QPointF position)`
- `applyValidationFix(FindingId id)`
- `runTherion(RunRequest request)`

Rules:

- Intents should be narrow and testable.
- Source mutations still go through existing transaction services.
- File IO and process execution remain outside widgets.

This phase keeps user actions independent from the current widget implementation. A future UI layer should call semantic operations, not private C++ widget slots.

Verification:

- Undo/redo tests for source-changing intents.
- Regression tests for map selection, drawing, dragging, validation fixes, and compiler launch.

## Phase 7 - Document Future View-Layer Criteria

This is not a QML implementation phase. It defines the conditions that should be true before any future view-layer experiment is considered.

Criteria for any future QML or alternate UI pilot:

- The surface already has a tested view model.
- The surface already uses semantic intent interfaces.
- Styling tokens are available without depending on widget internals.
- Domain, parser, validation, source transaction, settings, file IO, and process logic are outside the widget implementation.
- The existing Qt Widgets version can remain the production implementation while the experiment is evaluated.

Recommended future-only pilot candidates, if the project later decides to try QML:

1. Settings dialog
2. About/help metadata panel
3. Compiler runner panel
4. Validation panel

Verification for a future pilot:

- Build dependencies are explicitly documented before adding Qt Quick.
- The same view-model tests pass for the existing Qt Widgets surface and the experimental surface.
- Manual light/dark screenshots are checked for the pilot surface.

## Phase 8 - Preserve Map Editor Optionality

The map editor has custom rendering, selection, snapping, dragging, background layers, source transactions, and QGraphicsScene behavior. Treat it as a later architectural decision.

Do not plan a map canvas rewrite now. Keep these options open:

- Keep QGraphicsView for the map canvas permanently and clean up surrounding Qt Widgets panels.
- Keep QGraphicsView for the map canvas and, only if needed later, render surrounding panels with another UI technology.
- Wrap the existing map canvas inside a hybrid UI.
- Reimplement map rendering with another scene technology only after source/model/view contracts are stable.

Do not migrate the map canvas until:

- map geometry projection is fully separated from widgets
- source transaction ownership is centralized
- selection state is represented outside QGraphicsItem internals
- background rendering and hit testing have focused regression tests

## Control Styling Policy

- Prefer palette-driven native controls.
- Style simple frames, backgrounds, text colors, spacing, and radii centrally.
- Avoid styling native subcontrols such as:
  - `QComboBox::drop-down`
  - `QComboBox::down-arrow`
  - scrollbar arrows
  - native menu internals
  - spinbox arrows
- If a subcontrol must be styled, make it explicit:
  - use a dedicated style builder
  - add a comment explaining why native rendering was insufficient
  - verify in light and dark mode
  - verify on macOS, Windows, and Linux where possible

## Migration Rules

- Each phase should be independently shippable.
- Do not combine visual cleanup with source-model, parser, validator, or file IO changes.
- Keep commits narrow:
  - one style extraction
  - one UI builder split
  - one view-model introduction
  - one presenter or intent extraction
- Update `WORKLOG.md` when a phase starts or changes scope.
- Update `docs/USER_MANUAL.md` only when user-visible behavior changes, not for internal style refactors.

## Recommended Next Slice Queue

1. Extract one remaining long inline stylesheet into an existing style builder and verify light/dark behavior.
2. Replace one repeated metric cluster with `ApplicationControlMetrics` / `ApplicationStyleTokens`.
3. Split status badge construction from status update logic if the next status-bar change touches that area.
4. Extract one `MainWindowSidebar` page builder only when modifying that sidebar page.
5. Introduce one small read-only view model for a panel/list state with focused unit coverage.

## Verification Gates

- `python3 scripts/check_structure_constraints.py` for every GUI cleanup change.
- Build the affected target; run focused UI tests when the touched surface has coverage.
- Manual light/dark check for style or object-name changes.
- Manual narrow-width check for inspector/sidebar changes.
- No source-model, file IO, process execution, or source mutation behavior should change in a GUI cleanup commit unless that commit is intentionally re-scoped outside this plan.
