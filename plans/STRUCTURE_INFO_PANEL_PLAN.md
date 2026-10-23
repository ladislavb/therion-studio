# Structure Info Panel Plan

Date: 2026-10-23

Branch: `feature/structure-info-panel` (from `main` 78110e0)

Scope: add an info box at the bottom of the `Survey` and `Map` Structure views, similar to the xtherion compiler
`Survey info` panel. It shows details of the selected survey, map or scrap. Survey statistics come from Therion's own
xtherion output, then map and scrap details and further survey details come from Therion's SQL database export,
both written by the user's compile.

Status: slices 1 to 3 done and committed (5770d13, 37f66dc, eb2a00d, local, not pushed), after merging
`feature/compile-results` (460d843). Slice 4 not started; it waits for the worst-adjustment measure to be agreed.
The optional fix at the end is open. Specification, architecture, user manual, worklog and translations are updated
in the same changes that implement each slice.

## Current State

- The project navigation sidebar (`src/app/MainWindowSidebar.cpp`) hosts `Files | Survey | Map` pages over a
  `QStackedWidget`. Survey and Map share one `QTreeView` that is re-parented between the two page layouts.
- Structure rows carry source file, line, category, name and object ID (`MainWindowStructureRoles.h`), built from
  `ProjectStructureEntry`, which also holds the namespace path used to match Therion's survey full names.
- The Validation panel already uses a "select above, detail below" layout (`MainWindowValidationSidebar.cpp`).
- `TherionRunnerService` runs Therion as a process. `TherionSqlReportDatabase` imports a Therion SQL export into an
  in-memory database for the SQL report tab.
- The `JoinViewer` branch (PR 43) adds a background Therion run, `ProjectStationPositionService`, for join
  placement. This plan does not use it (see Data Source).

## Findings (Therion 6.4.0, checked on Therion's `samples/basics/rabbit`)

- `therion -x` always writes `.xtherion.dat` (hidden, fixed name) in the working folder. The survey tree is
  written only when Therion's exit state is above 0.
- Each survey line is `xth_cp_data_tree_insert <id> <parent id> <level> <name> <full name> "<title>" "<plain title>"
  "<statistics>"` (`thselector.cxx`). The statistics text is length (surface, duplicated), vertical, north-south and
  east-west range with end stations, number of shots and stations. It is exactly what xtherion shows.
- The map lines in `.xtherion.dat` give only projection, name, title and whether the map is basic. Scraps are not
  exported. This adds nothing to the Map tree, so maps and scraps need the SQL export.
- SQL `MAPS` has `LENGTH`, `DEPTH`, `PROJID` and title. `MAPITEMS` has the map contents with item type.
  `SCRAPS` has `PROJID`, `AVG_DISTORTION` and `MAX_DISTORTION`. Distortion is filled with only `export database`
  in the config (no map export needed).
- `MAPS.DEPTH` is bottom minus top (`thmapstat::get_depth`), so it is negative, while the PDF legend prints top minus
  bottom. The box shows it positive like the legend.
- The SQL export has no projection names, only a numeric `PROJID`.
- A `-namespace off` survey still has its own full name (`flat.cave`) in both files, and its maps and scraps are filed
  under it (`SURVEY_ID`), not under the namespace they belong to.
- When reading fails, Therion prints `writing xtherion file` straight after `reading ...` on the same line.
- Map team, dates and copyright (`thmapstat`) are written only to the PDF legend and are not available.
- `export survey-list` has explored and approximate lengths but rounds to whole metres and identifies surveys only by
  title and tree level, so it is not used.
- Therion's per-loop errors are only in `therion.log` and are not used. Per-shot adjustment errors are in SQL `SHOT`
  (`ERR_LENGTH`, `ERR_BEARING`, `ERR_GRADIENT`).
- One Therion run with `-x` and `export database` produces both files.

## Data Source: The User's Compile

Decided 2026-10-23: like xtherion, the information comes from the user's own compile. There is no background Therion
run.

- Every compile adds `-x` (not twice if the user already gives it in the compile arguments). Therion writes
  `.xtherion.dat` in the compile's working folder, where it is left, as xtherion does. Projects are expected to ignore
  it in git.
- Map, scrap and survey extras (slices 3 and 4) come from the database export of the same compile, found with the
  existing database-output rules (`isTherionDatabaseOutputFileName`). The user adds `export database -fmt sql` to the
  thconfig for these. Without it the box says so and offers a one-off compile that adds the export without changing
  the config (see slice 3).
- Figures follow the user's config, including its `select` lines.
- Before the first compile the box shows the slice 1 source details and a line saying statistics appear after a
  compile. After a source change the last figures stay, marked out of date. If the compile fails, the box says so and
  keeps the last good figures, marked out of date.

## Layout

- A box under the Structure tree on both the `Survey` and `Map` pages, in a vertical `QSplitter` so it can be resized
  or collapsed. The splitter position is kept for the session.
- Since the tree is shared between the pages, the box is shared too and moves with it. It starts 240 pixels high.
- Without saved expansion state the `Survey` view opens one level deep (top-level surveys open, their children
  collapsed); the `Map` view opens fully expanded.
- The box shows only what fits the selected row.
  - Survey row (Survey page): survey details.
  - Map row (either page): map details.
  - Scrap row (either page): scrap details.
  - Nothing selected, or a diagnostic row: a short prompt.
- Text is selectable and can be copied. Definition locations open the source at that line.

## Slices

### 1. Info box with source details (done, 5770d13)

- Add the splitter and box to both Structure pages. Selection in the tree updates it.
- Show what the project index already knows without Therion: name, full name (namespace path) and where it is defined.
  Reuse the structure row data and existing namespace rules. No new parsing.
- Keep the widget presentation-only. Formatting of the box text lives in a non-widget presenter so it can be tested
  without the GUI.
- Tests: presenter output for survey, map, scrap and empty selection.

### 2. Survey statistics from `.xtherion.dat` (done, 37f66dc)

- Add `-x` to compiles and read `.xtherion.dat` from the working folder when the compile finishes, off the UI thread.
  Only a file written by that compile is used.
- A core parser for the `xth_cp_data_tree_insert` lines (Tcl braces and quotes, `\n` in the statistics). Pure
  function, no IO.
- Match surveys by Therion full name to the structure row's namespace path, including `-namespace off` surveys. Keep
  Therion's innermost-to-outermost order (`child.parent`) as `docs/THERION_COMPATIBILITY.md` requires.
- Show title and the statistics text as Therion writes it (metres, as xtherion).
- Tests: parser on hand-written lines (quoting, empty title, survey without stations), name matching, staleness.

### 3. Map and scrap details from the SQL export (done, eb2a00d)

- Read the compile's database export with the existing SQL import code (`TherionSqlReportDatabase` or a shared part
  of it, not a second importer). Only an export written by that compile is used.
- Map: title, length, depth and contents (maps and scraps, preview above/below).
- Scrap: average and maximum distortion.
- Found while implementing: the export has only a numeric `PROJID`, so the projection is not shown. `DEPTH` is
  bottom minus top (`thmapstat::get_depth`), while the PDF legend prints top minus bottom, so the box shows it
  positive like the legend. Therion files a map or scrap under the survey block it is in (`SURVEY_ID`), also inside a
  `-namespace off` survey, so rows are matched by name and the enclosing survey's full name.
- Shown on both pages for map and scrap rows.
- The manual explains that `export database -fmt sql` in the thconfig is needed for these details.
- Added at the user's request: when the compile wrote no database export, the box offers a one-off compile that adds
  the export without changing the config. It runs a temporary copy of the config, next to it with the export line
  added, through the normal compile, so all figures come from one compile. The next compile is the normal one again.
- Tests: queries against a small hand-written SQL fixture, matching map and scrap names with their survey.

### 4. Survey extras from the SQL export (not started)

- Flagged stations: number and names of entrances, continuations and fixed stations (other flags if useful), with
  coordinates. Activating one opens its source where it can be found.
- Team and dates: surveyors, explorers, survey and exploration dates, gathered over the survey and its sub-surveys.
- Shot types: number of splay, approximate and duplicate shots.
- Worst adjustment: the shot with the largest adjustment error after loop closure, with its stations and errors. The
  exact measure (length, bearing or gradient error, or a combined figure) is agreed before this slice starts.
- These are counts and lookups over Therion's output. The headline survey statistics stay those from `.xtherion.dat`;
  Therion's own statistics rules are not copied.
- Tests: queries on the SQL fixture, including sub-survey aggregation.

## Documents To Update

- `SPECIFICATION.md` 3.8.6 Structure Sidebar Behavior: the info box, what each row type shows, out-of-date behaviour.
- `ARCHITECTURE.md` if reading compile results adds a new service boundary.
- `docs/USER_MANUAL.md` and the translated manuals.
- `WORKLOG.md`.
- Translations: insert only the new strings into the `.ts` catalogs.

## Optional Fix At The End

- Survey statistics did not show when the Compiler `Target Config` was set explicitly, even to the same `thconfig`
  that is found by default. The box said the survey was not in the last successful compile. Clearing the target
  config and compiling again showed the statistics. Outside the app the index gives the same survey full names with
  and without the explicit target config, and the compile output, `.xtherion.dat` and name matching are the same, so
  the difference is in how the running app connects the target config to the compile result or the index snapshot.
  Reproduce in the app first (project folder `LongwoodValleySink`, target config set to its `thconfig`).

## Open Points

- Measure used for the worst adjustment (slice 4). Suggested: the shot whose end moved furthest, as the distance in
  metres between the measured and adjusted leg, shown with its `ERR_LENGTH`, `ERR_BEARING` and `ERR_GRADIENT`.
  Alternatives are the error relative to shot length, or one chosen column. Not yet agreed.
