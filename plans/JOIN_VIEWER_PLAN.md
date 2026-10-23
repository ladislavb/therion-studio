# Join Viewer Plan

Date: 2026-10-13

Branch: `JoinViewer`

Scope: add a `Joins` page to the project navigation sidebar that lists `equate` links between different surveys, and a
Join view that opens the two `.th2` drawings containing the equated stations side by side, aligned on the join station
through the centreline, each fully editable and saved independently, with a faded read-only outline of the partner
drawing in each pane.

Status: phase 1 implemented (slices 1–6 below). Phase 2 (options A-C and fixes D1-D5) implemented by 2026-10-22,
pending review and the PR; items left for later are noted in their sections. Specification, architecture, user manual, worklog and translations are updated in the same
changes that implement each slice. Not archived until phase 2 is done.
Phase 3 (connections and compile data, E1-E4, and fixes E5a-E5c) implemented 2026-10-23, pending review and the PR.

## Current State

- The project navigation sidebar (`src/app/MainWindowSidebar.cpp`) hosts a segmented control `Files | Survey | Map`
  over a `QStackedWidget`; Survey and Map share one `QTreeView` that is re-parented between pages.
- `ProjectStructureIndex` already parses `equate` commands and resolves station references to survey namespaces
  (including `-namespace off` surveys) for validation diagnostics, but keeps no equate link data in
  `ProjectIndexSnapshot`.
- Map editors can center on a source line (`MapEditorTab::selectMapLine(line, centerOnSelection)`).
- A `.th2` file is open in at most one editor (attached tab or detached window, keyed by canonical path).
- `TherionXviParser` already reads XVI grid origin/vectors and station positions; `TherionBackgroundMetadata` already
  reads `##XTHERION## xth_me_image_insert` placement.

## Observations From Test Data (`CheddarCatchment/LongwoodValleySink`)

- Each survey has one `.th2` per projection (`*.plan.th2`, `*.extended.th2`, `*E315.th2`, ...). A join pairs drawings of
  the same projection.
- The `##XTHERION##` header lines carry editor state only (`xth_me_area_adjust` canvas extent, `xth_me_area_zoom_to`
  view zoom) and the background placement (`xth_me_image_insert`). The metric grid lives in the referenced XVI file
  (`set XVIgrids {1 m}`, `set XVIgrid {originX originY dx 0 0 dy nx ny}`), together with the centreline station
  positions (`set XVIstations`). 108 of 111 drawings reference an XVI.
- Scrap `-scale` is set on 203 of 212 scraps but is not reliable: Therion derives scrap scale/rotation from two or more
  station points matched to the centreline, so `-scale` is often absent or nominal. It is not used as the primary
  alignment source.
- Equates may link more than two stations (`equate 13 14@TennisBallRift 2@TickleCheck 2@TickleWest`) and may reference
  stations of a `-namespace off` survey from the parent (`equate 9 9@Triassic`). Equates inside one survey and
  commented-out equates are not joins.

## Alignment Method

Mirror Therion: place each drawing by matching its station points to centreline positions, then link the two drawings
at the equated station.

1. For each drawing, collect matched pairs: `point x y station -name N` (drawing coordinates) and the centreline
   position of `N` in metres.
2. Centreline positions, in order of preference:
   - the XVI referenced by `xth_me_image_insert` (the one matching most station names): XVI station positions
     converted to metres with the XVI grid (`XVIgrid` vector length per `XVIgrids` unit). The XVI placement offset
     is not needed because the fit maps drawing points straight onto XVI coordinates;
   - fallback when no usable XVI exists: scrap `-scale` (if set), translation-only at the join station;
   - otherwise the pair is listed but marked "no scale"; the panes open centred on the station without linking.
3. With two or more matched stations, fit a 2D similarity transform (scale, rotation, translation; least squares)
   from drawing coordinates to survey metres. With one station, use the grid scale and no rotation.
4. Each survey's XVI has its own origin, so the cross-survey offset is taken from the equated station: translate so
   the two join stations coincide.
5. Pane transform: `partnerToThis = inverse(fitThis) ∘ translateJoin ∘ fitPartner`. Pan/zoom in one pane is mapped
   through this transform to the other.

Notes:

- Exact for plan and fixed-angle elevation projections. Extended elevations are drawn independently per survey; the
  join station coincides but passage direction may differ.
- Later upgrade: phase 2 stage C4 places drawings from Therion-computed station positions (SQL database export).
- No external library: the fit is a closed-form 2D similarity. Core stays QtCore-only, so it uses a small
  `Th2SimilarityTransform` value type; the app layer converts it to `QTransform` for `QGraphicsView`.

## Sidebar: Joins Page

- A fourth segment `Joins` is added to the existing `Files | Survey | Map` control (same segmented style, last-segment
  property moves to `Joins`), backed by a new page in `structureViewStack_` and a new `StructurePanelJoinsPage` value.
- Page content, top to bottom:
  - projection selector (combo): projections found in the project, e.g. `plan`, `extended`, `elevation 315`;
    default `plan`; remembered per session;
  - filter field (survey or station text);
  - tree:
    - top level: one row per cross-survey `equate`, e.g. `0@BoldStep = 36@TridentLeft`; tooltip shows the
      source file and line of the `equate`;
    - two-station equates open directly on activation;
    - multi-station equates expand to one child row per survey pair;
    - rows are dimmed with a tooltip reason when either survey has no drawing for the selected projection or the
      station point is not drawn.
- Activation (double-click / Enter) opens the Join view. Context menu: `Open Join View`, `Go to Equate in Source`,
  `Open <survey> drawing` for each side.
- The Joins tree uses its own model and view (it is a flat/grouped list, not a projection of the Survey tree), filled
  from the same background project-index snapshot that drives Survey/Map, so it refreshes on save/edit without UI
  blocking.
- Structure remains navigation-only: unresolved equates stay reported in Validation, not in Joins.

## Join View (as built)

- Hosting: no join tab. The two drawings open as normal document tabs, and the user can show either map in the
  existing separate map window (`MapEditorTab::toggleMapPaneWindow`). Both editors therefore stay visible to save,
  close, and session handling, which a tab holding two editors would not be without a `MainWindow` refactor (46 code
  paths enumerate attached tabs or detached windows separately).
- `MapEditorTab` exposes a narrow view-linking API: view centre and scale in drawing units, `mapViewChanged`,
  `centrelineFitForSourceLine`, `stationPointSourcePosition`, outline path and source-to-scene transform, and adding a
  display-only overlay item.
- `MapEditorJoinViewLink` keeps both views on the same real point at the same real-world scale; views agree to within
  about 3 screen pixels because each view scrolls in whole pixels. A linked view widens its own scroll area so a join
  at a drawing edge can be centred.
- `MapEditorJoinOutline` shows the partner as a faded, display-only outline (lines and station points) placed through
  the join transform. It rebuilds after any partner source change (debounced 150 ms, because some map edits skip the
  scene refresh) and re-adds its item after host scene refreshes.
- `MapEditorJoinPairs` opens, lists, and closes a join (fit, outlines both ways, one link) and zooms both views to about
  30 m across the first view. A `Joined: <drawing>` status-bar indicator offers `Link Views`, `Show Outline`, and
  `Remove Join` per joined drawing. Changed 2026-10-15 after review: `Link Views` removed (joined drawings always move
  together), and the View menu entry renamed `Linked Drawings`.

## Changes From The Original Plan

- Join tab with a splitter replaced by normal tabs plus the existing separate map window (data-safety reason above).
- Faded outline implemented before wiring the link, at the user's suggestion; it made a two-editor host unnecessary.
- Alignment uses XVI station fits; scrap `-scale` is only a fallback (scale often unset; Therion fits from stations).
- Side fixes kept out of this branch: `fix/french-3d-inspector-strings`, `chore/refresh-translation-locations`, and
  `XVIrootStation` (`xth_me_image_insert` root station `0` was ignored, misplacing the XVI background).

## Implementation Slices

1. Core equate links: `ProjectEquateLink` data in `ProjectIndexSnapshot` (stations, resolved survey namespaces, source
   file/line), cross-survey filter, plus station-point lookup per survey/projection (`.th2` file, scrap, line).
   QTest cases: pairs, multi-station, `-namespace off`, same-survey exclusion, commented-out equates.
2. Core alignment: `Th2CentrelineAlignment` (pure functions): XVI grid scale, station matching, similarity fit,
   join transform. Extend `TherionXviParser` to read `XVIgrids` units if needed. QTest cases with fixtures taken from
   LongwoodValleySink.
3. Joins sidebar page (`ProjectJoinsPanel` over the pure `ProjectJoinList`), opening the two drawings centred on the
   station. Done.
4. View-linking API and `MapEditorJoinViewLink`. Done.
5. Faded partner outline (`MapEditorJoinOutline`). Done.
6. `Open Join` links the views (`MapEditorJoinPairs`) with the `Joined` indicator. Done.

Slices 1 and 2 are also done.

Each slice: update `SPECIFICATION.md`, `ARCHITECTURE.md` (UI surface ownership: Joins page), `docs/USER_MANUAL.md`,
`WORKLOG.md`, translation catalogs; run `scripts/check_structure_constraints.py`; stop for user review against
LongwoodValleySink.

## Phase 2: Options (requested 2026-10-14)

### A. Open one view in a separate window

- Default: activating a join (double-click / Enter) opens both drawings as tabs, as now.
- Joins row context menu gains `Open Join with Separate Window`: as `Open Join`, then shows the second drawing's map in
  the existing separate map window. Always the second (right-hand) drawing of the row; choosing a side is not worth
  the extra complexity.
- A Settings option reverses the two: double-click opens with a separate window, and the context menu offers
  `Open Join in Tabs`. Stored through `ISessionStore` like the other Settings options.

### B. Show that drawings are linked

- While a join's views are linked, both document tabs show a link icon (Lucide `link`, added to the bundled icon set
  under the existing Lucide licence notice). The separate map window shows the same icon in its title area. The icon
  goes when the link is turned off or the join removed.

### C. Therion `join` and `nosurvey` awareness

Background (Therion book, `join`):

- `join` works in two modes. **Scrap join**: two scrap IDs; Therion joins the closest scrap ends (passage walls only).
  `-count N` asks Therion to join the scraps in N places/passages. **Point/line join**: IDs of points or lines,
  optionally with a line point mark (`id:mark`, `:0`, `:end`, or an index); several items in one command join them all
  (`join a b c`). A passage usually needs two line joins; in extreme cases single points are joined, which can leave
  points on one side and a line on the other.
- `data nosurvey <from> <to>`: the network is computed from the other data and the listed stations are then connected
  by a line, so the two stations have no measured relation of their own.

One list, coloured (no separate modes): the Joins page lists every connection between drawings of different surveys:

- equate rows (as now);
- `nosurvey` rows: a `data nosurvey` leg whose two stations are in different survey namespaces;
- join-only rows: a `join` between drawings of different surveys with no equate or `nosurvey` leg between them.

Colours (three greens, light and dark theme tokens, with a legend tooltip):

1. **Bold green**: automatic scrap join (no `-count`) and an equate or `nosurvey` leg anywhere between the two
   surveys (decided 2026-10-14; to be reviewed once it is in use).
2. Green (second shade): scrap join with `-count N`.
3. Green (third shade): point/line join.
4. Plain: equate or `nosurvey` leg with no `join`. Legitimate; never shown as a problem.

A `nosurvey` leg counts exactly as an equate: it records a definite link, so it qualifies for bold green.

Join rules between one pair of drawings (decided 2026-10-14): the three join kinds are mutually exclusive. A scrap
join (automatic or `-count N`) is a single `join` command. Point/line joins need at least two `join` commands (a
passage needs both walls joined). Breaking these rules is an input error:

- more than one join kind between the same pair of drawings;
- more than one scrap-join command between the same pair;
- only one point/line join between a pair.

These are reported as Validation warnings naming the `join` lines (problem reporting stays in the Validation panel),
and the affected Joins row carries a warning marker that points to them.

Join-only rows use the colour of their join type and say "no equate" in the tooltip. Connections with neither a `join`
nor an equate are legitimate and simply not listed.

Staged implementation:

- **C1 Index.** Record `join` commands (mode, `-count`, each item resolved to its scrap or line and the line's scrap,
  marks) by extending the existing join-reference validation scan rather than scanning again, and record
  cross-survey `nosurvey` legs from the centreline scan. Drawing points gain their scrap's identity so equates and
  joins can be matched. Core tests only; no UI change.
- **C2 Classification and colours.** `ProjectJoinList` classifies each row (equate / `nosurvey` / join-only, and join
  type) for the selected projection; `ProjectJoinsPanel` shows the colours, legend, and join file/line in tooltips.
  Core and panel tests.
- **C3 Join rule checks.** Add the three join-rule warnings to project validation (same index data as C2), with
  focused false-positive tests, and the warning marker on Joins rows.
- **C4 Computed centreline placement.** Therion computes the station network; the app asks Therion for it rather than
  using the `.lox` (which also carries walls and surfaces). A background Therion run, on its own
  `TherionRunnerService` instance so the Compiler panel is not blocked, uses a temporary config that sources the
  project root and only exports the database (`export database -fmt sql`): surveys, stations with computed X/Y/Z,
  shots, and flags. The app already imports this format for its SQL report tab, so loading and station lookup reuse
  that code. The export is regenerated when missing or older than the sources. While it runs, equate rows open at
  once with the existing XVI anchor; when it is ready, every row (equate, `nosurvey`, join-only) is placed by fitting
  each drawing's station points to the computed positions projected into its projection (plan, or elevation at its
  angle), so both drawings share one frame and no anchor is needed.
  C4 details (decided 2026-10-15, to confirm in review):
  - Temporary config in an app cache folder (never in the project): `encoding utf-8`, one `source` line per `source`
    of the target config (absolute paths), and `export database -fmt sql`. Therion runs with that folder as its working
    directory, so `therion.log` also stays out of the project. About 0.5 s on the test project.
  - Station positions keyed like the index (`name@namespace`, namespace innermost first, matching the SQL
    `SURVEY.FULL_NAME`); every station point of every scrap is kept per scrap (line number and station key).
  - Projection into drawing metres: plan (X, Y); `elevation θ` (degrees) (X cos θ - Y sin θ, Z). Checked on 114
    scraps of a real project: 0.00-0.54 m RMS; the mirrored sign gives metres. Extended and other elevations: none.
  - Each scrap with two or more matched station points gets a similarity fit; partner-to-this is partner fit then
    the inverse of this fit, with no anchor. Otherwise the XVI anchor (equate rows) is used as now.
  - Run policy: one run at a time on its own runner; a request while running is queued once. Starts when the Joins
    page gets a project index and when a join opens while the export is missing or older than the project sources
    (saved files; Therion reads the disk). The status bar shows the run; a failed run leaves the XVI behaviour and says why.
  - Join-only rows open both drawings aligned when both fits exist, centred between the two scraps.
  - Slices: C4a core (positions, projection, fit, index station points), C4b service (export run, cache, load),
    C4c wiring (join open, join-only rows, status).
- **C5 Fallback without computed positions** — dropped (2026-10-15). Much of Therion Studio already needs Therion
  installed, so a second, approximate placement for when it cannot run is not worth its maintenance. Without computed
  positions, equate rows keep the XVI/`-scale` fit at the join station and join-only rows open unaligned with a
  status-bar reason. (The idea was: anchor point/line joins on the closest joined points and scrap joins on the
  closest wall-line ends.)
- **Extended elevations** have no common frame: each survey's extended layout is independent. Equate rows keep the
  equate anchor; rows with no equate open both drawings unlinked and say why, until a way to line them up is found.
  Note (2026-10-15): nothing to do at the moment; revisit only if a way to give extended elevations a shared frame
  appears.

### D. Fixes found in use (2026-10-21)

Reported while testing on a real project. Status: all done (2026-10-22). D1 was fixed and then replaced by D5, D3 was
reduced to its stale-check part, then D4 and D2 followed. D1 came first because it might have explained D3, and its
result changed D2-D4.

- **D1 Alignment is wrong even with a shared point.** Equate rows, where both drawings draw the shared station, do not
  line up correctly. Investigate before changing anything else. Measure where the shared station point lands in each
  pane in the common frame and how far apart it is. Then find which placement was used (computed fit or the XVI
  anchor, see D3) and check each step. That means station keys matched to the SQL `FULL_NAME`, plan and elevation
  projection and its sign, the per-scrap similarity fit against a drawing with several scraps, and the
  partner-to-this transform (partner fit, then the inverse of this fit). Record the cause here with its numbers before
  the fix is agreed.

  Result (2026-10-21, measured on the test project with a scratch tool that calls the branch's own core functions;
  every available equate row in plan, extended, elevation 0, 90 and 315):
  - Placement with computed positions is correct. Both copies of the shared station land within 0.23 m of each other
    (most under 0.1 m); fit residuals are 0.00-0.76 m RMS. The one row 2 m apart has its two stations 2 m apart in
    Therion's own positions. Station keys, the plan and elevation projection and its sign, the per-scrap fit and the
    partner-to-this transform all check out. The outline and the view link apply the same transform.
  - The fallback, used when computed positions are missing, still being computed or failed, and always in extended,
    is correct where both scraps match their XVI centreline stations. At 10 m from the join it is out by 0.6 m at
    most (rotation within 1.9 degrees, scale within 4 percent).
  - The fault is the `-scale` fallback. A scrap whose XVI is missing, or does not contain its stations, is placed by
    its `-scale` option. 9 rows in plan and elevations use it. In 5 of them (1 plan, 1 elevation 0, 3 elevation 90)
    the scale is 2 to 5 times wrong, so the partner is 5 to 42 m out at 10 m from the join, while the shared station
    still coincides. In the other 4 the `-scale` happens to be right. 5 extended rows also use `-scale`; they cannot
    be checked because extended has no computed frame.
  - Therion does the same steps differently. It applies `-scale` first, but a scrap with two or more station points
    is then refitted to the stations (`thdb2d::pp_adjust_points`), so `-scale` only decides scale and rotation for
    a scrap with fewer than two stations. Studio uses `-scale` even for scraps with many stations.

  Fix (agreed and done): with computed positions, fit each scrap to them on its own, and link the two
  drawings at the join station when only one of them can be fitted that way (computed metres and XVI metres should
  both be north-up metres in plan and the same elevation; to be checked). Use `-scale` only for a scrap with fewer
  than two station points, as Therion does. Otherwise refuse the placement and say why, instead of opening the
  drawings misaligned. D3 then matters less, because rows only fall back while positions are being computed.

  After the fix, measured the same way: no row is placed by a wrong `-scale`. Without computed positions the XVI
  placement is out by 0.54 m at most at 10 m from the join. The computed and XVI frames of the same scrap differ by
  2.2 degrees and 4 percent at most, so linking one of each at the join station is sound. Rows that can no longer be
  placed without computed positions: 3 plan, 1 elevation 0 and 4 elevation 90 (placed once positions are ready), and
  5 extended (extended has no computed frame, so these now open without outlines and say why, instead of opening
  misaligned).
- **D2 Station not drawn in the view.** Some rows of the form `<station>@<survey> = <station>@<other survey>` are
  dimmed with "not drawn in plan view", although the survey has a drawing in that view. Only the join station's point
  is missing. This happens in both plan and elevation views.
  Revised after D5 (2026-10-22). Such a row has no shared station point, so it is placed like a join-only row. Of the
  survey's scraps in that view, open the one whose station points lie nearest the station's computed position. Each
  drawing is placed by its own fit (D5) and offset by the closest pair of computed station points, and the views are
  centred as in D4. In extended there is no offset, so the row is not openable and says why, as for join-only rows.
  Without computed positions keep the dimmed row but word the reason exactly, e.g. "<station>@<survey> has no
  station point in <survey>'s plan drawing". Rows where the survey reads no drawing at all stay as they are.
  Done (2026-10-22). Changes from the plan: the row is not dimmed without computed positions, because the panel
  does not know about them; instead opening it says so in the status bar and opens nothing. A station that no
  drawing draws at all is treated the same when its survey has scraps in the view. The nearest scrap is the one
  with a station point nearest the station in 3D. On the test project this opens rows in plan and elevation views, and
  the scrap chosen matches the scraps named by hand in the project's `join` commands where both exist.
- **D3 Joins opened during a Therion run use the XVI.** While a run is going the placement ignores the previous
  legs and positions, so joins opened then fall back to the XVI, and join-only rows open unaligned. Since D5 this
  affects equated rows too, because the legs come from the same run. Every drawing save starts a run, because the
  results count as out of date when any source is newer, `.th2` drawings included.
  Revised 2026-10-22. Only `.th` files and the config make the results out of date, since the SQL export does not
  depend on the drawings. The rest is dropped and kept as a known problem: a join opened during a run after a `.th`
  or config change uses the XVI, or opens unaligned, until it is opened again. If that proves a nuisance, keep the
  previous results while a run is going and re-place joins that opened without them.
  Done (2026-10-22): drawings are left out of the stale check.
- **D4 Centre on where the drawings meet.** Join-only rows are centred between the middles of the two scraps, which can
  be well away from where they meet. Instead centre both views halfway between the closest pair of station points,
  one in each scrap, using their computed positions (the user's suggestion). D5 already finds this pair for the
  offset, so D4 reuses it. Rows whose two stations are linked by a `nosurvey` leg are centred halfway between those
  two stations. Placement itself stays on each drawing's fit (D5).
  Done (2026-10-22): every row is centred halfway between the two stations that place it, which for an equate is
  the join station.

- **D5 Place drawings as drawn, from measured legs (replaces the D1 fix).** Agreed in review on 2026-10-22, done
  (results at the end of this item). The D1 fix still places by Therion's computed station positions, which are loop closed for the whole
  project, while the drawings were made on unclosed or differently closed data. In the test project the two frames
  agree within 2.2 degrees and 4 percent, but a cave with large misclosures could be much worse. `-scale` is also
  still used for scraps with fewer than two stations, and it is unreliable because it can differ in every scrap.
  The join view should show the drawings as drawn.

  Scale and rotation of each drawing:
  - Fit the measured leg vectors to the drawing. Therion's SQL export, already read by the app, has a `SHOT` table
    with measured `LENGTH`, `BEARING` and `GRADIENT` beside the loop closed `ADJ_*` values. Checked with Therion
    6.4.0 that `BEARING` includes declination and that loop closure only changes `ADJ_*`. Units, calibration and
    data formats are then Therion's, not reimplemented. Splays and duplicates are left out using `SHOT_FLAG`.
  - Use every leg with both ends drawn as station points in the scrap. Project the measured vector into the view
    (plan, or elevation at its angle) and fit scale and rotation from measured to drawn vectors, weighting each leg by
    its projected length. Drop legs whose projected length is very short (threshold to be set from test data, for
    example under 1 m or a small fraction of the longest), since a drawing error of 0.1 m turns a 1 m leg by about 6
    degrees. This also drops legs that run nearly along the view direction in an elevation, and vertical legs in plan.
  - Extended has no rotation. Fit the scale only, from each leg's horizontal length along x and its vertical change
    along y. The left or right direction is taken from the drawing, so Therion's extend rules need not be copied.
  - Legs between drawn stations that pass through stations not drawn in the scrap are not chained for now. Add
    chaining only if real drawings need it.
  - If a scrap has too few usable legs, use its XVI grid (`XVIgrids` metres per `XVIgrid` vector) for scale and
    orientation. Where both are available, compare them and say when they disagree, which means the drawing does not
    sit on its background.
  - `-scale` is not used at all.

  Declination. Drawings may not be corrected for declination. SexyTopo has no declination setting, PocketTopo had one
  but it may have been approximate and may not be stored, and TopoDroid is unknown. Assume drawings are uncorrected
  unless found otherwise. The leg fit is not affected, because the fitted rotation absorbs it in plan, and extended
  has no rotation. Two cases are affected.
  - The XVI grid fallback in plan takes the drawing's north as true, so it may be turned by the declination. The
    message or tooltip says when a drawing was placed by its XVI grid.
  - An elevation drawn on uncorrected data is projected at a slightly different angle, a small change of shape that
    no rotation can remove. To be measured on test data; no correction planned.

  Placement of the two drawings:
  - Equated rows. Put the shared station at the same point in both drawings.
  - Rows without a shared station (join-only rows, and `nosurvey` legs) in plan and elevation. Take one station point
    from each drawing and offset by the projected difference of their computed positions. Use the closest such pair,
    so that loop closure has least effect, or for a `nosurvey` leg its two stations. Computed positions are used only
    for this one vector, never for scale or rotation. D4 still decides where the views are centred.
  - Rows without a shared station in extended. Computed positions are 3D and give no extended offset, so these rows
    are not openable for now, with a message saying why. To be revisited later.

  Order for each drawing: leg fit, then XVI grid, then no outlines with a message. The shared frame of computed
  positions from the D1 fix is removed. D2 and D3 change with this, since computed positions are then needed only
  for rows without a shared station, so revisit both after D5 (done, see D2 and D3).

  Considered and not used. Running Therion on the single survey would close loops inside it and could unroll
  extended differently from the drawing. Therion cannot export without a config file either (`therion -s file.th`
  processes the data but writes nothing), though a temporary config would do.

  Verification. Measure with the scratch tool on the test project before and after, in plan, elevations and
  extended: the gap at the shared station, the error 10 m from the join compared with the XVI placement, legs used
  and dropped per scrap, and leg fit against XVI grid per scrap. QTests for the leg fit (plan, elevation, extended,
  short and end-on legs dropped, splays left out), the XVI grid fallback, and the offset for rows without a shared
  station.

  Done (2026-10-22). Changes from the plan found while measuring:
  - A station drawn twice in one scrap took the fit 15 percent off (1.1 m RMS), so the legs of a station drawn
    more than once are left out. Legs more than 0.5 m, and more than three times the median, off the median of the
    legs' own scale and rotation are also left out as misdrawn; a least-squares first fit was pulled by the bad leg
    and did not catch it.
  - The XVI fallback keeps the fit to matching XVI stations when there are two or more, and uses the grid alone
    otherwise.
  - The XVI check compares scale only. Rotation differs by the declination when a drawing was made without it.
  - Legs keep the weighting of a plain least-squares fit of the vectors, so longer legs count more.
  - Therion also lists `nosurvey` legs in `SHOT`, with the vector computed from the network, and nothing marks
    them. They are not left out; they rarely join two stations of one scrap, and their vector is still close.
  - D3 is unchanged, so legs, like positions, are not used while a run is going; joins opened then use the XVI.

  Results on the test project with the scratch tool (every available row, 23 plan, 20 elevation 315, 9 elevation 0,
  5 elevation 90, 19 extended):
  - Every row is placed by measured legs on both sides; none needs the XVI and none fails. The 5 extended rows that
    D1 could not place are placed.
  - Leg fits agree with fits to computed positions within 1.5 degrees and 3.8 percent, and with the XVI within 1.9
    degrees and, except one drawing, 1 percent. That drawing's XVI is at a quarter of the legs' scale and of the
    computed positions' scale, and the status bar now says so.
  - Compared with the D1 placement from computed positions, the partner moves at most 0.47 m at 10 m from the join;
    the shared station, up to 0.23 m apart before, now coincides.
  - Angle error of single legs against the computed fit: up to 22 degrees under 1 m, up to 3.2 degrees from 1 to
    2 m, under 1.5 degrees for most longer legs (one 4.6). This supports leaving out legs under 1 m.
  - Join-only rows find a closest station pair 3.5 to 15.5 m apart.

Each slice is a separate commit on `JoinViewer` with tests, doc and translation updates, measured on the test project
before and after, and checked in the app.

### Open questions for phase 2

Answered (2026-10-14):

- An equate or `nosurvey` leg anywhere between the two surveys is enough for bold green (to be reviewed in use).
- Join kinds are exclusive per pair of drawings; point/line joins need at least two commands, scrap joins exactly one;
  violations are Validation warnings.
- Computed station positions are produced by the app through Therion when needed, and used for all rows once
  available.
- A `nosurvey` leg counts as an equate.
- Join-rule errors: Validation warnings plus a warning marker on the Joins row (confirmed).
- Extended elevations without an equate are left unlinked for now.
- Computed positions come from Therion's SQL database export, not the `.lox`.
- The background Therion run starts automatically when a join needs computed positions and they are missing or older
  than the sources; the status bar shows that it is running.

Still open:

1. Check during C4 whether `export database` can leave out the shot tables. No longer wanted once D5 is done, since
   it reads the `SHOT` table.

## Phase 3: Connections and Compile Data (planned 2026-10-23)

Background Therion runs are expensive, so connection windows take Therion data from the user's own compile, as the
planned structure info panel does (`plans/STRUCTURE_INFO_PANEL_PLAN.md` on `feature/structure-info-panel`). The
shared code is on `feature/compile-results` (from `main`, `TherionCompileResultRecorder`), merged into this branch and
into the info panel branch so neither depends on the other.

What the SQL data is used for today:
1. Scale and rotation of each drawing from measured legs between its station points. Used even when the station is
   drawn in both drawings; without it the XVI is used, which may be turned by the declination (existing note).
2. Lining up two drawings that share no station point (equates and Therion `join` rows alike).
3. Choosing a drawing for an equate whose station has no station point in that projection (nearest scrap of its
   survey).

### E1. Terminology

Status: done 2026-10-23. Kept as `join`: `Go to Join in Source`, the key entries, the row
tooltips about `join` kinds and the Validation messages, since they name the Therion command.

- "Connection" is the general word: the sidebar page becomes `Connections`, the Join view becomes the connection
  window, and the join-related UI text follows. `join` and `equate` are kept for the Therion commands and their rows
  (legend entries such as `Automatic Join` stay).
- Specification, manual, worklog and translations updated in the same change. Internal type names are not renamed in
  this slice.

### E2. Compile data instead of the automatic run

Status: done 2026-10-23. No new UI strings.

- Merge `feature/compile-results`. Station positions and legs are read from the database export of the latest
  compile (`TherionCompileResult::databasePaths`), with the existing queries (`TherionStationPositionExport`).
- The automatic background run is removed: showing the Connections page or a project index refresh no longer starts
  Therion.
- A one-off run is kept only for opening a connection that cannot be lined up otherwise (uses 2 and 3) when there is
  no compile data. Use 1 without compile data falls back to the XVI as now.
- Compile data is out of date when a `.th` source was saved after the compile started. `.th2` saves do not make it
  out of date, since positions and legs do not depend on drawings (as now).

### E3. Status line above the list

Status: done 2026-10-23 (`ProjectConnectionDataStatus`).

One line for the whole list, since one compile serves every row. For example "Positions from the compile at 10:42",
"Sources changed since the compile", or "No database export. Add `export database` to the thconfig". Nothing is shown
per row for this.

### E4. Row symbols

Status: done 2026-10-23. Approximate uses Lucide `circle-dashed`. Measured on the test project:
the fit of every scrap takes about 11 ms (555 legs, 180 scraps), so it runs on the UI thread when the positions or
the index change. Without positions no row is marked approximate (the status line already says there are none).

At most one symbol per row, for what that connection needs now. Tooltips and the legend explain each.

| Symbol | Meaning |
|---|---|
| none | opens and lines up fully now |
| approximate | opens, lined up from the XVI only (may be turned by the declination); compile data would improve it |
| needs compile data | cannot be lined up (no shared station point, or an equated station not drawn); works after a compile with a database export |

- `Cannot be opened` keeps its existing grey row; the rule warning icon is unchanged.
- When compile data is out of date, rows that use it show their symbol faded; E3 says why.
- Icons are Lucide, like the rest of the app. Proposed: `database` for needs compile data; the approximate icon is
  chosen in the slice (not `compass`, which is the survey icon).
- Working out "approximate" needs the legs fit per scrap; measure its cost on the test project before building it into
  the list, and compute it off the UI thread if needed.

Each slice is a separate commit with tests, doc and translation updates, checked in the app.

### E5. Fixes found in use (2026-10-23)

- **E5a Status line explains missing setup.** Without a target config most rows cannot be opened (6 of 43 plan rows
  on the test project, 30 with its config), and nothing said why. The line now says so first, with a
  `Select Target Config...` button (the Compiler pane's file dialog), and row tooltips name it. After a compile with
  no database export it says that, and that opening a connection that needs positions runs Therion once.
  Status: done 2026-10-23.
- **E5b Symbols push row text right.** Only some rows have an icon, so their text starts further right. Reserve the
  icon space on every row. Status: done with an item delegate (rows without a symbol keep no icon in the model).
- **E5c Choose and show the one-off run.** The run takes about 1.3 s on the test project and many seconds on large
  ones, it reran by itself when its positions were out of date, and the only sign of it was a line of text. Agreed
  2026-10-23:
  - A setting "When a connection needs station positions" with `Ask` (default), `Run Therion` and `Never`, stored
    with the other settings.
  - `Ask`: opening such a connection without positions asks "This connection needs station positions. Run Therion
    now? On a large project this can take a while." with `Run Therion` and `Open Without Alignment`, and a
    "Don't ask again" box that sets the setting to the choice made.
  - Out-of-date positions from a run are used faded, like compile data, and are not rerun by themselves. Under `Ask`
    the prompt offers updating them or using them as they are; `Run Therion` updates them; `Never` uses them.
  - `Never`: opens without alignment (or with the out-of-date positions); compiling with `export database` provides
    positions.
  - While a run is going, the status line shows a busy bar, the seconds elapsed and `Cancel`.
  - The requested connection opens by itself when the run finishes; after `Cancel` or a failed run it opens
    without alignment.
  - Status: done 2026-10-23 (`ConnectionPositionRunPolicy`, service `run`/`cancel`/
    `attemptFailed` replacing the automatic `request`). A failed run now keeps the positions there were.

## Risks

- Same file in two buffers: avoided by keeping normal tabs (no editor reuse needed).
- Drawings whose station points are not snapped to the XVI stations give a less exact fit; the fit residual is shown in
  the pane tooltip.

## Verification

- Core QTests for slices 1–2.
- Manual: LongwoodValleySink, `0@BoldStep = 36@TridentLeft` in plan and elevation 315; `13` four-way equate; edit and
  save each pane separately and confirm only that file changes (`git diff`) and undo stays per pane.
