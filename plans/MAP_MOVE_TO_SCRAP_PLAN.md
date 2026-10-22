# Map Move To Scrap Plan

Date: 2026-10-21

Status: implemented on `feature/map-scrap-fade-and-move-to`, as the commit after the scrap fade work; not yet tried
in the running app.

It reuses the fade work's scrap membership logic (`MapEditorScrapVisibility`), its context menus and the
`MapEditorUiQTests` runner, so both are on one branch.

Scope: a `Move to Scrap` submenu on a map object (canvas) and on an object row (Objects view) that moves the object
into another scrap of the same drawing.

## Why

Objects drawn in the wrong scrap can only be moved today by dragging the row onto a scrap in the Objects view, one
block at a time, with no protection for area borders or duplicate IDs.

## Behaviour

- Offered for a point, line or area inside a scrap; not on scrap rows, and not when the drawing has one scrap.
- Lists the other scraps by ID in source order; choosing one moves the object to the end of that scrap.
- One undoable source transaction (`Move to Scrap`); the object stays selected at its new place; coordinates do not
  change, since all scraps in a `.th2` share the drawing space.
- An area moves together with its border lines from the same scrap, in their original order.
- A line that is the only border of an area moves together with that area.
- A target is disabled, with the reason as its tooltip, when:
  - the object is a line that is one of several borders of an area in its scrap (move the area instead);
  - a border line of the area is also used by another area in the scrap.
- No `-id` check is needed. Therion requires IDs to be unique in the whole survey, so moving an object between scraps
  cannot create a duplicate.
- Several selected objects are not supported; the map editor has no general multiple selection yet.

## Implementation Outline

1. `MapEditorObjectMovePlanner::planMoveIntoScrap`: moves several blocks to before the target `endscrap` in one text
   rewrite, reusing the planner's line splitting and block end rules.
2. `MapEditorMoveToScrapPlanner` (pure): picks the blocks to move and the refusal rules, using
   `mapEditorScrapObjectLines` and the area reference resolver limited to the source scrap; lists the targets with
   their blocked reasons from one projection.
3. `MapEditorInspectorObjectController`: the drag/drop move and `Move to Scrap` share one transaction path
   (`applyObjectMovePlan`).
4. `MapEditorMoveToScrapMenu`: the submenu builder, used by the canvas menu and the Objects view menu.

## Tests

- Logic (`MapEditorMoveToScrapPlannerTest`): point to a later and an earlier scrap; area with its border line; sole
  border line with its area; one of several border lines refused; shared border refused; target list with reasons; CRLF kept.
- Map UI (`MapEditorScrapVisibilityUiTest::movesObjectToScrapWithUndo`): Objects view right-click, `Move to Scrap`,
  source text after the move, undo restores it.

## Documentation

- `SPECIFICATION.md`, user manuals (en, cs, fr, sk), cs/fr/sk catalogs (insert-only), `WORKLOG.md`.
