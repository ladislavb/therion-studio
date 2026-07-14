# Map Runtime Ownership And Undo Memory Plan

Date: 2026-07-13

Review findings: P1-6, P2-2, and focused parts of P2-1/P2-3.

Status: active after worker responsiveness work. Resource ownership and undo-memory work are independent tracks and
shall never be combined in one implementation slice.

Scope: replace hidden Map style/background runtime state with explicitly composed bounded dependencies, then separately
measure and reduce full-document undo memory where reversible source edits are provably safe.

## Ownership Target

- One composition-owned style catalog provider loads bundled/user/environment inputs outside render hot paths.
- Render/preview contexts receive immutable catalog data or a stable catalog handle.
- One composition-owned background asset cache owns raster, XVI, and SVG source assets with explicit limits and
  invalidation.
- QGraphicsItems remain presentation projections; they do not own resource IO, settings, source metadata, or cache
  policy.
- `TextEditorSourceTransactionController` remains the single transaction/undo contract. Memory optimization may change
  command representation but not transaction semantics.

## Non-Goals

- Do not redesign Map styles, SVG metadata, source transactions, and scene refresh in one change.
- Do not add a generic service locator or static provider accessor.
- Do not move source metadata or inspector state into background graphics items.
- Do not remove full-snapshot undo fallback for complex/ambiguous rewrites.
- Do not claim memory or performance improvement without measurements.

## R1 — Inject Immutable Style Catalog Data

Read first:

- `MapEditorObjectStyleCatalog.*`
- `MapEditorSceneRenderer.*`
- `MapEditorStylePreviewWidget.*`
- Map scene/preview context types and production composition path

Steps:

1. Separate deterministic catalog parsing/merging from resource/settings/path discovery.
2. Introduce a narrow provider interface only for external catalog loading/invalidation; keep resolved catalog values
   concrete.
3. Compose the production provider at startup/Map tab construction and pass immutable catalog data into renderer and
   preview contexts.
4. Give tests a deterministic in-memory catalog without resource overrides or process-global reset hooks.
5. Remove production render-path calls to the function-static catalog accessor only after all consumers are injected.

Acceptance:

- no resource, environment, application-data, or settings IO occurs while rendering a scene item;
- renderer and style preview use the same injected catalog revision;
- missing/invalid user catalog still falls back to bundled defaults;
- `MapEditorObjectStyleCatalogTest` and representative render/preview tests pass.

Stop condition: do not introduce an interface for deterministic style resolution; the external loading seam is enough.

## R2 — Define A Bounded Background Asset Cache

Add a focused cache service under `src/app/text_editor/map_editor/` with:

- canonical source identity;
- file size and high-resolution mtime, plus content revision/hash fallback when metadata cannot distinguish a change;
- asset format and decode/projection options in the key;
- byte-cost-aware LRU limit and explicit eviction;
- per-entry load error and revision identity;
- no QGraphicsItem, inspector, or source-transaction ownership.

The first slice defines/tests policy with fake loader data. Do not wire all formats immediately.

Tests: hit, metadata invalidation, same-timestamp content change fallback, byte-limit eviction, oversized-entry policy,
explicit clear, and deterministic destruction.

## R3 — Migrate XVI, Raster, Then SVG Loading

Order:

1. XVI, because its current unbounded static cache still reads/hashes the file before a hit;
2. raster, preserving existing display cap and Gamma behavior;
3. SVG source/renderer data only after invalid-load and intrinsic-size behavior are covered.

For each format:

- replace only that static cache;
- preserve current item/placement type and metadata behavior;
- keep loading/parsing off the GUI thread where it is expensive;
- suppress stale async results by layer identity and request generation;
- verify reopen, source file mutation, visibility, opacity, z-order, transforms, Fit With Background, and teardown.

Stop condition: if one common cache value type forces format-specific presentation state into the service, retain
separate typed payloads behind one policy owner.

## R4 — Explicit Invalidation And Diagnostics

- Invalidate canonical asset identities after watched file changes, removal, project close, or explicit reload.
- Log bounded cache stats only under diagnostic logging: hits, misses, evictions, bytes, load time, stale result.
- Never log document content or unbounded file lists.
- Add fakes proving tests do not share mutable cache state.

Implementation status (2026-07-14): R3 format migration is complete. XVI documents, raster source/display payloads,
and immutable SVG source bytes plus intrinsic metadata use the tab-owned cache. SVG graphics items receive cached source
data and no longer open source files themselves. R4 is the next cache slice.

## R5 — Background File Responsibility Split

Only after R2-R4 provide a real seam, extract asset loading/cache coordination from
`MapEditorBackgroundLayers.cpp`. Do not mechanically split by line count. Dialog intent, source transaction wiring,
placement state, and graphics-item application remain separate named responsibilities.

## U1 — Measure Undo Memory Without Behavior Change

Instrument transaction diagnostics or a focused benchmark to record:

- document character/byte size;
- command representation estimate;
- map undo depth and aggregate retained estimate;
- source edit count/replaced byte count;
- snapshot fallback reason.

Use generated small/medium/large TH2 documents and narrow point/line/background edits. Diagnostic output must be opt-in
and must not expose source text.

Exit decision: continue to U2 only when measurements show material retained-memory cost and identify a safe high-volume
edit class.

## U2 — Reversible Range Command For Simple Source Edits

Eligibility requires:

- valid, non-overlapping `TherionSourceTextEdit` ranges against the exact before revision;
- captured removed and replacement text for each edit;
- deterministic forward and reverse application order;
- no encoding/newline conversion;
- the same projection invalidation and selection/cursor restoration contract as snapshots.

Structural block rewrites, ambiguous offsets, whole-document formatting, or requests without sufficient edit metadata
shall use `TextEditorSourceSnapshotCommand` unchanged.

Tests:

- point and line drag undo/redo;
- multiple non-overlapping edits;
- insert/delete and CRLF preservation;
- stale revision rejection;
- dirty-state and undo labels;
- mixed map/text arbitration;
- exact source equality after undo and redo;
- fallback selection for complex requests.

Stop condition: any mismatch in exact text, revision timeline, dirty state, or projection restoration requires fallback,
not a heuristic repair.

## U3 — Bound And Verify Mixed History

- Compare retained-memory estimates at the existing 200-command Map limit.
- Verify clearing/reloading releases range and snapshot commands.
- Keep full snapshots for commands where safety dominates memory.
- Do not change the user-visible history limit in the same slice.

## Relationship To Other Plans

- R1 is a prerequisite for the revised render context in `MAP_PARTIAL_REFRESH_PLAN.md`.
- R2-R5 own general cache/loading policy; `SVG_BACKGROUND_PLAN.md` owns only SVG compatibility and UX.
- U-series is not a partial-refresh optimization and uses separate commits/tests.

## Exit Gate

- Map rendering and previews receive explicit style catalog data with no hidden production IO.
- Background caches are bounded, explicit, invalidatable, and isolated in tests.
- Expensive asset loading is stale-result safe and does not block the GUI thread.
- Any range-based undo command is exact and conservative, with full snapshots retained as the safe fallback.
