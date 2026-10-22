# Map Scrap Fade Plan

Date: 2026-10-21

Status: implemented on `feature/map-scrap-fade-and-move-to` (from `main` 78110e0); not yet tried in the running app.

Scope: let map objects and whole scraps be faded as well as hidden, from the Objects view eye icon and from a
right-click menu on the map canvas and the Objects view.

## Why

Scraps in one drawing often lie on top of each other (levels, passages crossing). The Objects view can already hide
a scrap, but hiding removes it from view entirely. Fading keeps the other scraps visible for orientation while the
working scrap stands out, and keeps clicks on the working scrap where scraps overlap.

## Behaviour

- Each object and scrap is shown, faded or hidden. This is view state only, like hiding today: it never changes the
  `.th2` file and is not stored between sessions.
- Faded objects are drawn at about 25% opacity and cannot be picked or hovered on the canvas; hidden objects are
  not drawn, as today.
- Objects view eye icon: a click cycles shown, faded, hidden, shown. On a scrap row it applies to the whole scrap,
  as hiding does today. The icon is the eye for shown, a dimmed eye for faded and the crossed eye for hidden.
- Right-click menu on a map object (canvas) or on an object or scrap row (Objects view), when it is in a scrap:
  - `This Scrap` with `Show`, `Fade`, `Hide`;
  - `Other Scraps` with `Show`, `Fade`, `Hide`, when the drawing has other scraps.
  The current choice is ticked when all objects of those scraps share it.
- Right-click on empty canvas: `Show All Scraps`, which shows every faded or hidden object again. This is the way
  back when everything has been faded or hidden, since faded and hidden objects cannot be clicked.
- Unchanged: selection, handles of a selected object, backgrounds, area clipping, `Fit`.

## Implementation Outline

1. `MapEditorScrapVisibility` (pure, no widgets): the shown/faded/hidden value and its cycle, a view over the hidden
   and faded line sets, and the object lines of each scrap from `Th2GeometryProjection` (scrap block ranges and the
   points, lines and areas inside them).
2. Map tab state: keep `hiddenInspectorObjectLines_` and add `fadedInspectorObjectLines_`; every place that removes,
   clears or prunes hidden lines does the same for faded lines through the view.
3. `MapEditorInspectorObjectController::applyInspectorObjectVisibility`: also set opacity and turn off mouse picking
   for faded items, keeping each item's original accepted buttons in a scene data role so it can be restored.
4. Objects view: eye click cycles; icon and tooltip per state; dimmed eye drawn with the disabled text colour.
5. `MapEditorScrapVisibilityMenu`: one builder for the `This Scrap` / `Other Scraps` submenus and `Show All Scraps`,
   used by the canvas menu (`showMapSelectionContextMenu`) and a new Objects view context menu.

## Tests

- Logic (`MapEditorLogicQTests`): visibility cycle; scrap object lines from a two-scrap drawing; set and read
  `This Scrap` / `Other Scraps`; removing and pruning lines clears both sets.
- Map UI (`MapEditorUiQTests`): two scraps; fading one scrap lowers its items' opacity and stops picking while the
  other scrap is unchanged; hiding hides them; `Show All Scraps` restores opacity, visibility and picking; the eye
  click cycles shown, faded, hidden.

## Documentation

- `SPECIFICATION.md`: Objects view visibility rows, the cycle, the context menus, faded objects not pickable.
- User manuals (en, cs, fr, sk): Objects view section and the map context menu.
- Translations (cs, fr, sk): new strings inserted with the insert-only script.
- `WORKLOG.md`: entry while in progress.

## Implementation Notes

- Empty canvas right-clicks still open nothing in the normal case; the `Show All Scraps` menu appears only while
  something is faded or hidden.
- The Objects view context menu is handled in the existing tree viewport event filter, so
  `MapEditorTabInspectorPanelUi.cpp` stays within its structure line limit.
- Map UI QTests now have a general runner, `MapEditorUiQTests`, for this and later map UI suites.
- Known limit, shared with hiding on `main`: the single-object partial refresh after an edit rebuilds that object's
  items without reapplying visibility, so editing a faded object through the inspector draws it at full strength
  until the next full refresh.

## Follow-up

- `Move to Scrap` in the same menu, as the next commit on the same branch (`plans/MAP_MOVE_TO_SCRAP_PLAN.md`).
