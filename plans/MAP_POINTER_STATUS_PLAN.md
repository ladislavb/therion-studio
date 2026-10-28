# Map Pointer Status Plan

Date: 2026-10-28

Branch: `feature/map-pointer-status` from `main` 78110e0.

Scope: show the pointer position and the hovered XVI station in the map editor status bar, as XTherion does.

## Current State

- The main-window status bar shows the map instruction, zoom, `M:` mode, `C:` compiler indicator and encoding. The
  detached map window shows its own zoom and mode labels. Neither shows where the pointer is.
- The map magnifier (on by default, `View -> Show Map Magnifier`) already shows the pointer position in TH2 units as
  `x 1234.5  y -67.8`, but only while it is shown, and it covers part of the map.
- Point and vertex coordinates are otherwise only visible in Raw mode.
- `MapEditorXviBackgroundItem` keeps each XVI station name (`MapEditorXviStationData`) but only draws the circles.
  The name is used only when inserting a station point (`MapEditorTab::xviStationSnapAtScenePosition`).

## XTherion Reference

- `xtherion/me.tcl` `xth_me_area_motion` shows the pointer as `x : y` in TH2 units with one decimal place.
- `xtherion/me_imgs.tcl` shows `station - NAME` while the pointer is over an XVI station.
- XTherion also shows the XVI grid spacing on hover. This is not part of this plan.

## Decisions

- Coordinates are TH2 drawing units only, using the magnifier's text (`x 1234.5  y -67.8`) rather than XTherion's
  `x : y`, so both readouts agree.
- The XVI station name goes in the same label as the coordinates.
- No grid-spacing hover.
- One commit per slice. Ask before each commit.

## Slice 1: Pointer Coordinates (done, d4e53e0)

- The magnifier's throttled pointer tracking becomes the shared pointer readout: one update computes the coordinate
  text and feeds both the magnifier (when enabled) and the new `MapEditorTab::pointerStatusText()` /
  `pointerStatusChanged()`. The readout runs whether or not the magnifier is shown.
- The text comes from the existing magnifier formatter, made static as `MapEditorTab::pointerCoordinateText`, over
  `sourcePointFromScenePosition`, so the figures match what insertion would write.
- Cleared when the pointer leaves the viewport or the viewport is hidden (tab switch, detach). A pending throttled
  update after leaving is dropped.
- Main window: `statusMapPointerLabel_` before zoom, shown while an attached map tab is current, refreshed on
  `pointerStatusChanged` without a full status refresh.
- Detached map window: matching label before zoom. The main window hides its label while detached (as for zoom and
  mode).
- Both labels take a fixed minimum width from `MapEditorTab::pointerStatusWidthSampleText()`.

Tests (`MapEditorPointerStatusTest` in `MapEditorBackgroundUiQTests`): formatter (one decimal, `.` under a Czech
locale); status follows a mouse move and clears on leave, with the magnifier shown and hidden.

## Slice 2: XVI Station On Hover (done)

- While the pointer is within the snap radius of a visible XVI station, append the station to the same label, e.g.
  `x 1234.5  y -67.8   Station: 12.3@cave`.
- Reuse `xviStationSnapAtScenePosition` (same 12 px radius, same rule for overlapping stations). No new search code.
- Hidden XVI layers are ignored, as for snapping.
- The magnifier keeps showing coordinates only.
- The label may widen while a station name is shown; its fixed minimum width covers coordinates only.
- The status updates on pointer movement. Hiding a layer while the pointer stays still takes effect on the next move.

Tests: label text with and without a station; over a station the name is shown, away from it and with the layer
hidden it is not.

## Settings (done)

Requested after slice 2, before its commit: both parts are options, off by default so the status bar is unchanged
unless the user opts in.

- `File -> Settings...` has two independent tick boxes under `Map status bar`: `Show pointer coordinates in the map
  status bar` and `Show XVI station names in the map status bar`. Stored by `ISessionStore`
  (`therionMapPointerCoordinatesInStatusBarEnabled`, `therionMapXviStationNamesInStatusBarEnabled`), copied to new
  windows, applied to open attached and detached map tabs on OK.
- Each part shows alone or both together. With both off the label is hidden. The XVI lookup only runs when station
  names are on. The magnifier is not affected.
- `MainWindow.cpp` was at its line cap, so `showSettingsDialog` moved to `MainWindowSettings.cpp`, as PR 38 also
  does. Merging into `mostPRs20261022` needs that file's conflict resolved by hand.

Tests: off by default and switching on applies at the current pointer position; label text for each combination;
XVI hover with both parts, station names only, and coordinates only.

## Each Slice Also

- `SPECIFICATION.md` status-bar section.
- `docs/USER_MANUAL.md` and the cs/sk/fr manuals.
- New strings inserted only into the cs/sk/fr `.ts` catalogs, all pending strings in one run of
  `insert_translations.py`.
- `WORKLOG.md` entry.

## After Merge Of Slices

- Push the branch and open a PR to upstream.
- Merge the branch into `mostPRs20261022`.
