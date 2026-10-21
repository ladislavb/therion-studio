# Line Point Neighbour Handles Plan

Date: 2026-10-21

Status: implemented on `feature/line-point-neighbour-handles`; tried in the running app.

Scope: when one line point is selected on the map, also show the two neighbouring handles that shape its segments, and
let them change length but not direction, as xTherion does.

## Why

Today selecting a line point shows only its own `<<` and `>>` handles. The two segments either side of it are also
shaped by the previous point's outgoing handle and the next point's incoming handle. In xTherion those two handles
are shown with the selected point, in a lighter colour, and can only be made longer or shorter along their current
direction. That lets the curve next to the point be adjusted without changing the direction at the neighbour, which
would alter the neighbouring segment as well.

## Behaviour

- Selecting one line point shows, besides its own handles:
  - the previous point's outgoing handle, if it has one;
  - the next point's incoming handle, if it has one;
  - for a closed line, the neighbours across the join.
- These neighbour handles and their connector lines are drawn in a lighter colour than the selected point's handles,
  in light and dark themes, so the restriction is visible before dragging.
- Dragging a neighbour handle moves it only along the line from its own anchor through the handle: its length
  changes, its direction does not. It cannot pass through its anchor (a small minimum length is kept), and snapping
  to other geometry is off for it.
- Because the direction does not change, a smooth neighbour stays smooth and its opposite handle is not moved; a
  corner stays a corner.
- The change is written as one source transaction with an undo label, like other handle moves; only the dragged
  handle's coordinates change.
- Selecting the neighbour point itself (clicking its anchor) gives it full handle editing as today.
- With several points selected, or the whole line selected, behaviour stays as today.

## Implementation Outline

1. Selection reveal (`MapEditorSelectionController`): when exactly one line point is selected, also reveal the
   neighbour handles and connectors, marked with a new role (for example `kMapSceneNeighbourControlRole`) set when
   the scene builds handle items (`MapEditorSceneRenderer`).
2. Styling (`MapEditorSceneThemePolicy`): add neighbour handle stroke and fill colours, lighter than
   `controlHandleStroke` and `controlHandleFill`, with a connector colour to match; apply them while the handle is in
   the neighbour role.
3. Drag constraint (`MapEditableGeometryVertexItem` in `MapEditorSceneInternals.h`): while in the neighbour role,
   project the drag candidate onto the ray from the owning anchor through the handle's position at drag start, clamp
   to the minimum length, and skip snapping. Put the projection in a small pure function so it can be tested.
4. Commit: reuse the existing handle move commit path, but skip the smooth mirroring of the opposite handle for a
   neighbour drag: a length-only change needs no other handle moved. This also keeps the work independent of the
   smoothness fix on `fix/line-point-smooth` (GitHub #36, #48), so the branch starts from `main`.

## Tests

- Pure tests for the projection: along the direction, beyond the anchor (clamped), perpendicular movement ignored.
- Selection test: one selected point reveals its own handles plus the two neighbour handles in the neighbour role;
  two selected points reveal no neighbour handles.
- Rewrite test: a constrained drag changes only that handle's coordinates, keeps its direction, and leaves a smooth
  neighbour's `smooth` row and opposite handle unchanged.

## Documentation

- User manuals (all four languages): the lighter neighbour handles and that they change length only.
- `SPECIFICATION.md`: the reveal rule, the length-only constraint, and the lighter styling.

## Implementation Notes

- The neighbour handles are the existing handle items; the scene records the anchor at the other end of each
  handle's segment, and the selection presentation marks them as neighbour handles.
- Picking a neighbour handle keeps the selected point as the selection owner, so the inspector and the shown handles
  do not switch to the neighbour point.
- The four repeated handle-building blocks in the scene renderer were folded into one lambda.
- Mirroring is skipped for any handle drag that keeps the handle's direction, which covers neighbour drags without
  extra plumbing.

## Before The PR

- Confirmed with the user that this matches xTherion (only the two facing handles, length only, lighter).
- Test-merge against the open PRs and `fix/line-point-smooth`; `MapEditorSceneRenderer.cpp` is also changed by #43,
  #51, and that branch, in other parts of the file.
