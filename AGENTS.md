# Therion Studio Agent Instructions

## Scope

These instructions apply to the whole repository.

## Project State

- This repository is currently specification-first. The primary source of truth is [SPECIFICATION.md](SPECIFICATION.md).
- Treat the specification as implementation-grade requirements for a Qt reimplementation of Therion Studio.
- Treat [ARCHITECTURE.md](ARCHITECTURE.md) as the current architecture direction and boundary contract. It does not override product requirements in the specification, but implementation choices should align with it unless the divergence is explicit and documented.
- Treat [docs/THERION_COMPATIBILITY.md](docs/THERION_COMPATIBILITY.md) as the current Therion language compatibility guardrail for parser, validator, project-index, source-range, namespace, reference-resolution, completion, highlighting, and map/source synchronization work.
- Do not invent product behavior that conflicts with the specification. If a requested change requires behavior not covered by the specification, update the specification as part of the same change or clearly flag the gap.
- If implementation, tests, architecture, and the specification diverge, prefer bringing them back into alignment explicitly rather than silently choosing one source of truth.
- Treat [plans/REVIEW_CODEX.md](plans/REVIEW_CODEX.md) as the current architecture review record. It is not a product specification, but actionable findings from it should be reflected in `AGENTS.md`, `WORKLOG.md`, focused tests, or the specification before being treated as resolved.

## Specification Editing Rules

- Preserve the document's SRS style and section structure unless the change explicitly requires restructuring.
- Use normative language consistently:
  - "shall" for mandatory behavior
  - "should" for strong recommendations
  - "may" for optional behavior
- Keep requirements testable. New functional requirements should be specific enough to verify.
- Keep implementation notes and guidance separate from normative requirements.
- Preserve explicit MVP and Post-MVP distinctions where relevant.
- Prefer additive edits over broad rewrites. Do not weaken or broaden requirements casually.
- When adding a new behavior, update the most relevant acceptance criteria section if verification expectations change.

## Implementation Expectations

- Default technology assumptions should match the specification: Qt 6, cross-platform desktop, shared core logic across macOS, Windows, and Linux.
- Follow established best practices and relevant platform standards for modern Qt and C++ desktop application development unless the specification explicitly requires a different approach.
- Treat [WORKLOG.md](WORKLOG.md) as the active short-form implementation roadmap and backlog. Keep stable architectural principles in [ARCHITECTURE.md](ARCHITECTURE.md), keep detailed working plans in [plans/](plans/), and keep `WORKLOG.md` focused on current/open work.
- Preserve separation of concerns:
  - domain model, parsing, serialization, and editing rules stay outside UI classes
  - widgets, views, and scene items should not own file I/O or document parsing
  - services own external interactions such as file access, process execution, and metadata loading
- Favor straightforward, testable designs over speculative abstractions.
- Do not introduce new uses of deprecated functions or APIs. Prefer supported, current equivalents and update existing deprecated usages when touching related code.
- Keep files and translation units focused and reasonably sized. There is no hard line limit, but once a file starts mixing multiple responsibilities or becomes difficult to scan in one pass, split it by responsibility. Try to avoid files with >2000 lines if possible.
- Prefer splitting large features into focused types or files such as model, parser, serializer, widget, controller, scene item, or service components rather than accumulating unrelated behavior in one class.
- Avoid generic catch-all modules or class names such as Helpers, Utils, Misc, Support, or Manager unless the code is genuinely cohesive and no clearer responsibility word exists.
- Do not introduce dependencies outside Qt and the standard library without explicit justification.

## Architecture Guardrails

- Keep the intended dependency direction explicit:
  - presentation (`src/app/**` widgets, dialogs, views, scene items) may depend on application/core contracts and injected infrastructure interfaces
  - application workflow services should avoid QtWidgets and should be testable without launching the full GUI
  - core/domain code shall not depend on QtWidgets, QGraphicsView, dialogs, windows, or UI event classes
  - infrastructure/platform adapters may use Qt/platform APIs, but should expose narrow interfaces upward
- Do not add new business rules, persistence rules, parser/serializer logic, process orchestration, command-catalog loading, settings access, or platform-path decisions directly into widgets, dialogs, QGraphicsItems, or scene items.
- Do not add new direct `DocumentFile` calls from widgets. Route document loading, saving, and encoding decisions through focused document workflow/IO services.
- Treat `MainWindow`, `TextEditorTab`, `MapEditorTab`, and `ThreeDViewerTab` as orchestration shells under active reduction. When touching them, prefer extracting focused non-widget collaborators over adding more unrelated private state, slots, static helper functions, or workflow branches.
- Do not add convenience constructors to UI shells or controllers that silently instantiate real infrastructure adapters such as filesystem, settings/session stores, resource/catalog loaders, process runners, or platform services.
- Compose production infrastructure at an explicit composition boundary such as `main.cpp`, `MainWindow` bootstrap code, or a future composition-root class. Tests shall pass fakes or explicit test adapters rather than relying on hidden production defaults.
- Do not introduce static/global service access for dependencies that perform IO, read bundled/user resources, access settings, launch processes, cache mutable state, or depend on platform APIs. Static functions are acceptable only for deterministic, stateless transformations with no hidden external dependency.
- Add interfaces only where they provide a real seam: IO, settings, platform APIs, process execution, resource/catalog loading, multiple runtime implementations, or required test fakes. Prefer concrete pure functions/value services for deterministic logic.
- Avoid service-locator context structs. Context structs should stay narrow, typed, and workflow-specific; they should not become broad bags of unrelated pointers and callbacks.
- Avoid long-lived callbacks capturing widget pointers unless ownership and teardown behavior are explicit. Prefer named QObject signals/slots, narrow interfaces, or presenter/service methods for semantic workflows.
- Keep QGraphicsItems and map scene items presentation-focused: rendering, hit affordances, and local interaction state only. Source rewrite, catalog rules, persistence, and document mutation belong outside scene items.
- Long-running parsing, indexing, Therion execution, filesystem traversal, asset loading, and expensive map/background rendering shall be asynchronous, cancellable, debounced, or otherwise structured so they do not block the UI thread.
- Cross-platform behavior shall be centralized behind platform or infrastructure services when practical. New `Q_OS_*` checks should be limited to `src/platform/**`, startup/bootstrap code, packaging code, or a clearly justified platform adapter.
- Build/resource wiring should not duplicate source-of-truth lists unnecessarily. If resource catalogs are split into many files, prefer scoped CMake globs or generated lists with guardrails over manually duplicated file lists in multiple targets.
- Keep command catalog and map-style catalog loading at composition, startup, or explicit test setup boundaries. Do not reintroduce UI-side static catalog access or long-lived resource overrides when parser/generator support is the correct fix.
- For architecture-sensitive changes, check [ARCHITECTURE.md](ARCHITECTURE.md) before editing and update it in the same change when ownership boundaries, dependency direction, UI surface ownership, source model direction, or transaction rules change.
- For Therion language-sensitive changes, check [docs/THERION_COMPATIBILITY.md](docs/THERION_COMPATIBILITY.md) before editing and update it in the same change when namespace, reference, identifier uniqueness, file-role, validation, highlighting, source-preservation, or catalog behavior changes.

## Unified Source Architecture Direction

- The current architecture is one shared, lossless Therion source model for `.th`, `.th2`, and `thconfig` documents. Raw, Blocks, Map, Structure, syntax highlighting, completion/help, and compiler-facing workflows should remain projections of that shared source snapshot rather than independent reparsers.
- Do not add new editor-specific tokenizers, line splitters, numeric-token classifiers, option parsers, command serializers, or source-range heuristics when a core type can own the rule. Prefer extending focused core components such as `TherionSourceText`, `TherionSourceDocument`, `TherionSourceLogicalDocument`, `Th2GeometryProjection`, `TherionStringUtils`, `TherionTokenRules`, and `TherionCommandLineModel`.
- Treat `plans/archive/UNIFIED_SOURCE_DOM_PLAN.md` as completed migration history, not as an active implementation queue. New source-model work should be scoped as focused architecture maintenance, regression fixes, or feature support.
- Parser and projection work should preserve every physical source line, comments, blank lines, indentation where practical, original newline style, token spans, block ranges, source file type, and encoding metadata.
- Map, Structure, Background, Block, and syntax/help projections should consume shared parsed command/option data where available. Do not fix projection drift by copying parser logic into renderers, inspectors, scene items, or sidebar widgets.
- Cache parsed source snapshots and derived projections by document revision or file identity where practical. Avoid full document reparsing or scene rebuilding for UI-only events, cursor movement, appearance changes, or unrelated inspector updates.
- New direct `TherionDocumentParser::parseLine`, `parseTokenLines`, or `tokenizeLine` production call sites must stay inside the documented exception list enforced by `scripts/check_structure_constraints.py`. Prefer source snapshots/projections for UI, project, validation, and transaction workflows.
- Treat project validation as a target live diagnostic projection over the open project and open in-memory documents. New validation plumbing should be debounced, cancellable, revision/generation-keyed, and surfaced through the Validation panel rather than depending on a manual `Validate Project` button as the primary workflow.
- Keep problem reporting centralized in the Validation surface. Structure is an orientation/navigation projection and should not become a second general validator.
- Preserve Therion namespace semantics exactly. Qualified references use innermost-to-outermost order such as `object@child.parent`; do not convert this to filesystem-style `parent.child`. Follow [docs/THERION_COMPATIBILITY.md](docs/THERION_COMPATIBILITY.md) for duplicate identifier and reference-resolution behavior.

## Proposal Review Discipline

- If the user explicitly asks to review or describe the intended solution before making changes, treat that as a hard stop on edits and implementation for the current turn. Do not modify files, run formatters, apply patches, or execute build/test commands intended to validate a not-yet-approved implementation. First explain the proposed approach, risks, tradeoffs, verification strategy, and the smallest safe implementation slice; wait for explicit approval such as `ok`, `souhlas`, or `udělej to` before editing.
- Do not implement user-proposed designs mechanically when they introduce architectural debt, performance risk, UX regression, security exposure, portability problems, or testability loss.
- Before implementing a non-trivial user proposal, evaluate it against:
  - best practices for modern C++/Qt and the existing repository architecture
  - runtime performance and UI responsiveness
  - cross-platform behavior on macOS, Windows, and Linux
  - user experience, discoverability, accessibility, and platform conventions
  - data integrity, round-trip safety, and undo/redo semantics
  - security and safety, especially file access, process execution, resource loading, and installer/packaging behavior
  - testability and long-term maintenance cost
- If the proposed approach is sound, proceed and state the relevant assumptions briefly.
- If the proposed approach is risky or overcomplicated, say so directly and explain the concrete risk. Offer one or more practical alternatives, including the recommended option and tradeoffs.
- Prefer incremental implementation scenarios for risky areas: smallest safe slice first, focused tests, then follow-up expansion.
- Do not block momentum with theoretical objections. Challenge only when the concern can plausibly affect correctness, maintainability, performance, UX, security, portability, or project scope.
- When product behavior is ambiguous, distinguish between:
  - what is specified
  - what is inferred from existing implementation
  - what is a design recommendation
- For high-risk areas such as parsing, serialization, source rewrite, map/text synchronization, packaging, process execution, or platform paths, include an explicit verification strategy before treating the change as complete.

## Source File Splitting

- Treat a translation unit as too large once it mixes unrelated responsibilities or becomes hard to scan in one pass, even if it still compiles cleanly.
- Split by responsibility, not by line count: keep orchestration shells thin and move parsing, indexing, persistence, data transformation, and reusable UI behavior into focused companion types or files.
- Prefer extracting pure logic first, then moving stateful coordination, then narrowing the UI shell to event wiring and presentation.
- Keep public interfaces small and explicit; if a new split requires cross-file knowledge, add a narrow API rather than sharing internal state.
- When extracting behavior that affects user-visible workflows, add or update focused tests for the extracted logic before treating the split as complete.
- Preserve the spec-first boundary: if a split changes ownership or behavior, confirm the specification still describes the result or update it in the same change.

## Naming Conventions

- Treat `Raw`, `Block`, and `Map` as peer editor-mode domains over the same underlying document model.
- Use mode/domain-prefixed names for mode-specific implementation types:
  - `RawEditor*` for raw-text mode specific behavior
  - `BlockEditor*` for structured block-canvas mode specific behavior
  - `MapEditor*` for map/visual mode specific behavior
- Use container/shell-prefixed names for orchestration and hosting types:
  - `TextEditorTab*`, `MainWindow*`, and similar shell-level coordinators
- Use neutral/shared names only for logic that is intentionally cross-mode:
  - examples: `*Service`, `*Validation`, `*Controller` without mode prefix only when genuinely shared
- Keep filename and primary type name aligned exactly (for example, `BlockEditorOptionArgsController` in `BlockEditorOptionArgsController.h/.cpp`).
- Avoid near-duplicate names that differ only by a vague suffix while owning different responsibilities (for example, action execution vs. state computation). Names shall make the distinction explicit with concrete responsibility terms such as `*Executor`, `*StateController`, `*Model`, or `*View`.
- When introducing a new type in an area that already has similarly named types, prefer the most specific role word available over generic `Controller` naming.
- Avoid `*Support` for newly extracted modules when a concrete responsibility suffix such as `*Data`, `*Logic`, `*Resolver`, `*Parser`, `*Serializer`, `*Index`, or `*Renderer` describes the role more precisely.

## Directory Structure Stability

- The current editor-mode directory layout is stable and shall be treated as a repository contract:
  - `src/app/text_editor/` for raw editor shell and shared text-editor orchestration
  - `src/app/text_editor/raw_editor/` for raw-mode specific components (`RawEditor*`)
  - `src/app/text_editor/block_editor/` for block-mode specific components (`BlockEditor*`)
  - `src/app/text_editor/map_editor/` for map/visual-mode specific components (`MapEditor*`)
  - `src/app/three_d_viewer/` for read-only 3D viewer components (`ThreeDViewer*`)
- Do not introduce new editor-mode implementation files back into legacy top-level `src/app/` paths when they belong to the stable layout above.
- Do not move or rename these stable directories unless explicitly requested; such a change shall be handled as an intentional architecture migration.
- Any approved migration of this layout shall update in the same change:
  - build wiring (`CMakeLists.txt` and target source lists),
  - structure guardrails/tests (for example `scripts/check_structure_constraints.py` / `StructureConstraintsTest`),
  - relevant documentation (`AGENTS.md`, `WORKLOG.md`, and specification/manual where applicable).

## Data Integrity Rules

- Round-trip safety is a core requirement. Changes must not cause semantic loss in supported Therion documents.
- Preserve comments, unknown but valid directives, formatting where practical, and existing encodings unless the user explicitly requests conversion.
- Treat parsing, serialization, indexing, and map/text synchronization changes as high-risk areas that require careful verification.
- Any change that touches Therion parsing, serialization, rewrite behavior, or source-to-model synchronization should include explicit round-trip or behavioral verification and should call out remaining semantic-risk areas.
- For TH2 map-editor operations that mutate document source text, implementations shall route writes through the shared atomic map-source helper (`applySourceTextChangeWithSnapshot`) or an equivalent single-transaction abstraction that performs source replacement and undo snapshot creation together. Do not introduce new map-editor flows that rewrite text via ad hoc `replaceTextForCommand` / `rewrite*` calls with separate snapshot handling.
- New Raw, Blocks, Map, inspector, and sidebar workflows that mutate document source text should route through `TextEditorSourceTransactionController` or its successor shared source-change service. Any temporary exception shall be documented, narrowly scoped, and covered by focused undo/redo or round-trip tests.
- Every user-visible source mutation should be one logical transaction with a clear undo label, revision/snapshot handling, dirty-state update, projection refresh policy, and selection/cursor restoration policy.
- Do not reintroduce fallback source writes that silently skip undo snapshots, document revision checks, or post-change projection refresh.
- Prefer structured metadata over runtime heuristics when behavior depends on documented rules. In particular, style catalogs, highlight palettes, and help metadata should be data-driven rather than hardcoded ad hoc.
- For Therion completion/highlighting/help behavior, implement metadata correctness in parser/generator code first; do not solve these by editing generated catalog output or adding long-lived overrides.
- Use `resources/therion_catalog/overrides/*.override.json` only as a short-term fallback when parser extraction is not yet feasible, and remove such overrides once parser support is added.

## UI and Behavior Rules

- Preserve documented user workflows before pursuing architectural or visual cleanup.
- All user-visible UI text shall be translatable through Qt translation APIs and kept current in the Czech and Slovak `.ts` catalogs in the same change. Do not introduce hardcoded visible English UI strings, status messages, dialog text, tooltips, placeholders, or validation/problem text. Therion syntax, command names, option tokens, file extensions, source snippets, and user-authored document text may remain untranslated when they are shown as syntax or data rather than UI copy.
- Keep the UI responsive. Long-running work such as Therion execution, parsing, indexing, or asset loading must not block the UI thread.
- Avoid fixed-delay retry loops for selection restoration, scene refresh, indexing, or background loading. Prefer explicit completion signals, generation IDs, cancellable requests, or debounced refresh controllers so work runs once when the relevant snapshot is ready.
- Respect platform conventions for shortcuts, menus, dialogs, and file handling, but do not violate documented functional requirements.
- Treat keyboard shortcuts as cross-platform requirements: any shortcut-driven workflow shall be usable on macOS, Windows, and Linux, including a non-`Insert` fallback for actions that rely on keys commonly missing on Mac keyboards.
- Native platform behavior should win when it does not conflict with the specification.
- Prefer actionable user-facing errors and safe recovery paths. Do not swallow failures in parsing, file I/O, asset loading, or process execution when the user needs to understand or resolve the problem.

## Verification Expectations

- When a test fails, first treat the failure as evidence that the production code, its platform integration, or the test environment may be incorrect. Reproduce the failure, inspect and verify the code under test, and fix the production code when it is defective; do not change the test merely to make the failure pass.
- Do not weaken assertions, broaden tolerances, add skips, disable cases, or otherwise alter an existing test as the first response to a failure. Test changes are allowed only after concrete evidence shows that the test itself is incorrect, stale, or invalid for the specified behavior, and only after explicit user approval; document that evidence and the reason for the approved test change.
- Prefer focused automated tests for parser, serializer, project loading, indexing, command routing, session restore, and document-editing logic.
- Use Qt's QTest framework for all new C++ tests. Keep CTest as the test runner/orchestrator, but do not add new hand-rolled `main()`/`require()`/`expect()` test harnesses unless there is a narrowly documented reason.
- Organize QTest executables by dependency/runtime boundary, not by individual test class and not as one global runner. Use aggregate runners such as `TherionCoreQTests`, future app-service runners, text-editor runners, and map/UI runners when test files share the same dependencies and runtime setup.
- Put new core-only QTest files under `tests/core/` and add them to the shared `TherionCoreQTests` runner unless they require isolation for process, filesystem, timing, performance, or crash-containment reasons. Follow the same directory/runner pattern for future app-service, text-editor, and map-editor QTest groups.
- When touching existing hand-rolled tests, prefer migrating the affected test file or newly added cases to the appropriate QTest runner where practical. Migrations should be incremental and should preserve focused dependency boundaries rather than forcing unrelated tests into one executable.
- Keep large, slow, flaky, UI-heavy, process/integration, or special-resource tests as separate CTest executables when isolation is more valuable than reducing executable count.
- Before proposing a commit or opening a PR, run `python3 scripts/check_structure_constraints.py` locally and fix any violations in the same change.
- If a change affects externally visible behavior, verify it against the acceptance criteria in the specification.
- If automated verification is not possible, document a concrete manual verification pass with specific user workflows or scenarios.
- Do not claim behavior is implemented if the specification, tests, or verification steps do not support that claim.

## Change Discipline

- Keep changes tightly scoped to the request.
- Do not create commits without explicit user confirmation for that specific commit. After preparing changes, summarize the scope, verification, and proposed commit message, then wait for the user's approval before running `git commit`.
- Maintain a living [SPECIFICATION.md](SPECIFICATION.md): whenever important behavior, workflow, constraints, acceptance criteria, or architecture assumptions change, update the specification in the same change.
- Maintain a living [ARCHITECTURE.md](ARCHITECTURE.md): whenever architectural direction, ownership boundaries, source model strategy, transaction policy, or UI surface ownership changes, update it in the same change.
- Prefer specification wording that is framework-agnostic where practical so future reimplementations can reuse it as product/source-of-truth guidance.
- Update documentation when behavior, architecture, packaging, or verification expectations change.
- Maintain a living [docs/USER_MANUAL.md](docs/USER_MANUAL.md) as an end-user guide: keep only user-facing workflows, UI behavior, shortcuts, settings, and troubleshooting; do not include implementation internals, architecture plans, QA-only checklists, or packaging/developer instructions.
- Update [docs/USER_MANUAL.md](docs/USER_MANUAL.md) in the same change whenever user-visible features are added, behavior changes, or features are removed.
- For added features, document where the feature appears in the UI, how users use it, and any relevant shortcuts/constraints.
- For changed features, update the affected sections to match actual behavior and remove stale wording in the same edit.
- For removed features, delete obsolete instructions and document the replacement workflow when one exists.
- If the file does not exist yet, create it as part of the first user-visible workflow change.
- Surface ambiguities instead of silently choosing a product behavior that could ripple through the spec.
- When code is added later, prefer file and module names that reflect responsibility clearly.
- Document progress in the repository itself, not only in chat. Update [WORKLOG.md](WORKLOG.md) after every change so the current implementation state stays visible in the repo.
- Keep [WORKLOG.md](WORKLOG.md) focused on short-form planned/open/in-progress work (for example: `Current Focus`, `Active Work`, `Blocked / Needs Input`, `Backlog`) when practical. Put long phase plans and review plans in [plans/](plans/) instead of expanding `WORKLOG.md`.
- Do not keep completed-task items in [WORKLOG.md](WORKLOG.md); move completed history to archive files when needed.

## Commit Message Discipline

- Every commit requires explicit user confirmation before it is created, even when the user already requested code or documentation changes.
- Use short English commit subjects in the Conventional Commits style: `<type>: <imperative summary>`.
- Prefer these types:
  - `feat` for user-visible behavior or capability
  - `fix` for correctness, regression, or stale-state fixes
  - `docs` for documentation-only changes
  - `test` for test-only changes
  - `refactor` for behavior-preserving code structure changes
  - `perf` for performance-only improvements
  - `build` for build system, dependency, or generated-resource wiring
  - `chore` only for repository maintenance that does not fit a clearer type
- Keep the subject focused on what the commit actually changes, not the whole roadmap. Avoid vague subjects such as `update stuff`, `misc`, `cleanup`, `WIP`, or `next`.
- Use an optional scope only when it adds useful precision, for example `feat(validation): ...` or `docs(architecture): ...`.
- Keep the subject preferably under 72 characters and do not end it with a period.
- Use the imperative mood: `add`, `fix`, `document`, `suppress`, `extract`, not `added`, `fixes`, or `documented`.
- Split unrelated work into separate commits. In particular, keep generated catalog/resource updates separate from runtime logic unless they are inseparable.
- For non-trivial commits, include a body that briefly explains why the change was needed, important behavior or architecture consequences, and the verification that was run.
- If a commit intentionally leaves follow-up work, mention the follow-up in `WORKLOG.md` rather than burying it only in the commit body.
