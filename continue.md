# Continue - navigation phase 1 (2026-10-23)

Development stopped at the user's request for transfer to another PC.
This is a WIP checkpoint, not a verified release. Read AGENTS.md first.

## Last action

Added a two-entry decoration projection cache in
`src/app/text_editor/map_editor/MapEditorLineDecorationItem.{h,cpp}`.
Affine translation reuses flattened polygons and cumulative distances;
paint and hit testing compensate for the cached translation. Two entries
are intended to keep the main viewport and magnifier warm independently.
Geometry changes invalidate both entries; projective transforms use exact keys.

Added work counters and `projectionCacheReusesPanAndMagnifier` in
`tests/MapEditorLinePreviewTest.cpp`. Before the fix the test failed as intended:
5 projection builds, expected 2. The old renderer's test image SHA256 was
`72e45c13b08c4f8973c143aae9911f578b0a3bf853da58765565fda361286a26`.
The subsequent build/test call was interrupted; GREEN is NOT established.
Pixel fidelity, clipping, hit testing and cache invalidation need more coverage.

## Next action

Run:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target MapEditorBackgroundUiQTests -j2
QT_QPA_PLATFORM=offscreen build/MapEditorBackgroundUiQTests --suite MapEditorLinePreviewTest projectionCacheReusesPanAndMagnifier
```

Compare the image hash with the baseline, then run the full LinePreview suite.
Navigation synchronization optimization has NOT started: zoom currently calls
screen-space synchronization twice; wheel/touch pan still call it unnecessarily.
Add a regression before changing these paths. Preserve screen-space bounds.

## Open threads

- User approved ONLY navigation priorities 1/2 plus magnifier; no drag/drawing,
  document, TH2-format or undo changes in this phase. Stop after build/install
  and let the user manually evaluate navigation before proceeding.
- Gamma stale-result/backpressure and projection-cache pruning were previously
  built/tested/installed (163/163 CTest at that time), not freshly verified here.
- Earlier console retention, scene membership and path-sort changes are also
  included in this checkpoint. Console disk writes/flush remain synchronous;
  stderr accumulation and disk retention are unresolved. Last alias test and
  console CMake registration changes still need verification.
- Source includes substantial prior 2D/3D/GitHub backup work. Do not revert it.
  Local builds, logs, AppImage and private survey data are not transferred.
- Once approved work passes, build the application and run
  `cmake --install build --prefix install`; no navigation build was installed.

## Do not

- Do not claim 60 FPS or pixel fidelity without new evidence.
- Do not resume station/XVI diagnosis; the user canceled it.
- Do not force-push or merge this WIP checkpoint into upstream main.
