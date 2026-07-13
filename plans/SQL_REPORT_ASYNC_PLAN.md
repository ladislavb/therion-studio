# SQL Report Responsiveness Plan

Date: 2026-07-13

Review findings: P1-2 and the report-specific part of P2-1.

Status: active after monotonic scanner results.

Scope: move Therion SQL import and read-only report queries behind a worker-owned SQLite connection with request
supersession, real interruption, bounded execution, and safe tab teardown. Preset persistence and CSV file output move
out of the widget only after the worker lifecycle is stable.

## Required Ownership

- `TherionSqlReportTab` owns widgets, busy/error presentation, latest request identity, and immutable table DTOs.
- `TherionSqlReportWorker` owns `TherionSqlReportDatabase`, its `QSqlDatabase` connection, import/query execution, and
  connection teardown.
- The SQLite connection is created, used, and destroyed on one dedicated worker thread.
- Queued signals cross the thread boundary only with value requests/results; never with `QSqlDatabase`, `QSqlQuery`,
  model, or widget pointers.
- The existing read-only statement policy, expected-schema validation, row cap, presets, and CSV output remain stable.

## Non-Goals

- Do not move the widget or table model to a worker thread.
- Do not share a SQL connection between UI and worker threads.
- Do not add charts, filters, write queries, or report features.
- Do not treat a UI timer or an atomic flag alone as cancellation of a currently executing SQLite statement.
- Do not refactor all settings/dialog/file workflows in the application.

## S1 — Prove The Worker-Owned Database Contract

Allowed scope:

- new `src/app/reports/TherionSqlReportWorker.*`
- minimal value request/result types
- `TherionSqlReportDatabase.*` changes required for worker construction/teardown
- `CMakeLists.txt`
- focused app/service QTests

Steps:

1. Create the database inside the worker thread, not in the tab constructor and not before `moveToThread()`.
2. Add import and query requests carrying request ID, source identity, query text, row limit, and execution policy.
3. Emit value results with request ID, source identity, table/schema DTO, error code/message, and cancelled state.
4. Close and remove the named Qt SQL connection in the same worker thread after queries are destroyed.
5. Test thread identity and deterministic teardown without a widget.

Acceptance:

- every Qt SQL operation and connection lifecycle event occurs on the worker thread;
- no SQL connection warning appears during teardown;
- malformed import rolls back and returns an actionable value error;
- the worker has no QWidget or dialog dependency.

Stop condition: any design requiring a connection created on the GUI thread or returned to the GUI thread is rejected.

## S2 — Define Async Tab Load And Query Semantics

Read callers of `TherionSqlReportTab::loadFile()` before editing. Preserve the generic document-open contract by making
the synchronous return mean "request accepted/path valid" and present later import/schema failure through the tab's
normal error state, or introduce an explicit async open result at the application workflow boundary. Do not silently
reinterpret failure without updating callers/tests.

Steps:

1. Replace the tab-owned database with the worker/controller boundary.
2. Assign a monotonically increasing request ID when import/query is accepted.
3. Disable conflicting controls and show translatable busy/cancel state.
4. Apply a result only when request ID and current source identity still match.
5. Query B supersedes query A; reload/new file supersedes all queries for the previous database.
6. Closing the tab requests cancellation, disconnects publication, and tears the worker down without UI callbacks.

Tests:

- event-loop heartbeat during a generated large import;
- query A then B publishes only B;
- load file A then B cannot publish A's schema/table;
- import error leaves a coherent empty/previous state according to the declared policy;
- close tab during import/query is bounded and safe.

## S3 — Add Real SQLite Interruption And Deadline Policy

First perform a focused feasibility slice for the deployed QSQLITE driver. A valid implementation must reach SQLite's
progress/interruption mechanism through a supported, narrowly wrapped native handle and must be compatible with all
packaged platforms.

Requirements:

- cooperative cancel is observable inside a long-running statement;
- an execution deadline/progress budget protects against recursive or computationally expensive read-only queries;
- cancellation and timeout have distinct result states/messages;
- existing row cap remains in force and is not presented as an execution deadline;
- interruption is invoked in a thread-safe way documented by the SQLite/Qt contract;
- handler state cannot outlive the worker/database.

Stop conditions:

- the deployed Qt driver does not expose a safe interruption/progress seam;
- implementation requires sharing `QSqlDatabase` across threads;
- a long query cannot be deterministically terminated in teardown tests.

If blocked, record the driver/platform evidence and keep the finding open. Do not claim cancellation based only on
discarding a late result.

## S4 — Progress And Recovery

- Import progress may report statements/bytes processed at a bounded frequency.
- Query execution reports indeterminate busy state unless the SQLite policy provides a meaningful bounded metric.
- A cancelled/failed request shall leave the worker able to import/query again.
- Status and error strings are translated at the presentation boundary or through explicit translation contexts.
- No request logs SQL text or imported source content.

## S5 — Extract Report Persistence And CSV IO

Only after S1-S4:

1. Inject the preset store/settings adapter instead of constructing/owning real `QSettings` in the widget.
2. Extract deterministic CSV serialization from path/dialog handling.
3. Route file writing through a focused exporter/file workflow; the tab selects export intent and presents errors.
4. Preserve CSV quoting, headers, row order, and existing user workflow.

Keep this as a separate commit chain from worker changes.

## Verification Matrix

- `TherionSqlReportDatabaseTest`: statement splitting, read-only policy, schema validation, row cap, malformed rollback.
- New worker QTest: thread affinity, large import, supersession, cancel, timeout, recovery, teardown.
- Tab/controller QTest: busy state, stale suppression, current-table behavior, translated errors.
- CSV parity test after S5.
- Cross-platform Debug CI, structure constraints, localization checker, `git diff --check`.

## Exit Gate

- Import and queries do not block the GUI event loop.
- A stale result cannot replace a newer file/query result.
- Cancellation interrupts active SQLite execution and tab teardown is bounded.
- Connection creation/use/destruction is worker-thread-affine.
- Preset and CSV behavior remains compatible and no real settings/file adapter is hidden in the widget.
