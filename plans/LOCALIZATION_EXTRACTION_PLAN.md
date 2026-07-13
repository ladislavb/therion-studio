# Localization Extraction And Visible String Plan

Date: 2026-07-13

Review finding: P1-7, with focused P2-3/P2-4 extractions.

Status: active as an independent quality-gate chain.

Scope: fix the known visible strings that bypass Qt translation extraction and add a deterministic CI audit proving
that supported source keys participate in a fresh `lupdate` extraction.

## Translation Boundary

Translate UI labels, help prose, status/error messages, tooltips, placeholders, and dialog text. Do not translate
Therion commands/options, serialized source, canonical catalog tokens, raw arity values used as syntax, paths, or
user-authored document content.

Service/domain code should prefer stable error codes plus parameters where presentation chooses wording. A focused
QObject translation context is acceptable for application services when the message is inherently application copy
and no UI presenter seam is needed.

## Non-Goals

- Do not wrap every `QStringLiteral` in `tr()` mechanically.
- Do not manually edit generated catalog output without a source/extraction change.
- Do not redesign the Therion command catalog pipeline.
- Do not split `MapEditorSceneRenderer.cpp` broadly while extracting help content.
- Do not make all localization warnings fatal in one step without measuring current noise.

## L1 — Fix Known Unextractable Surfaces

### L1A Map help content

Extract visible Map help-page construction from `MapEditorSceneRenderer.cpp` into a focused
`MapEditorHelpContent.*` formatter or an existing appropriate help module. Use an explicit translation context for
labels/prose while leaving Therion syntax/examples unchanged.

Tests assert English source keys and, with a test translator, that labels are translated without changing syntax.

### L1B Raw command option help

Extract visible labels such as Option, Description, Value Arity, and Accepted Values from
`RawEditorCommandMetadataLoader.cpp` into a focused formatter. Convert internal arity metadata to human-readable,
translatable UI copy only at presentation time; retain canonical metadata values internally.

### L1C Project-template errors

Change `ProjectTemplateService` failures to a stable error enum/code plus parameters. Map those errors to translated
messages in `MainWindowProjectTemplate.cpp` or a focused presenter. Preserve detailed path/error parameters and existing
tests of failure causes.

Each sub-slice is a separate commit. Update Czech and Slovak catalogs in the same sub-slice.

## L2 — Add A Temporary Fresh-Extraction Audit

Add `scripts/check_localization_extraction.py` or extend the existing checker with a clearly separate mode.

Required behavior:

1. Locate the repository's supported `lupdate` executable deterministically.
2. Run extraction into a temporary directory; never rewrite committed `.ts` files during checking.
3. Parse the temporary catalog and compare required context/source keys for known critical surfaces.
4. Verify committed Czech/Slovak catalogs contain those keys, completed translations, and matching placeholders.
5. Report missing source extraction separately from missing translation.
6. Remove/let the temporary directory clean itself on success and failure.

Start with an explicit critical-surface manifest for Map help, Raw help labels, and project-template errors. Do not
claim that arbitrary source-code scanning can perfectly classify every literal.

Stop conditions:

- `lupdate` is unavailable in a supported CI image;
- extraction output is nondeterministic because source/build roots are not normalized;
- the proposed rule cannot distinguish visible copy from syntax/catalog data.

In those cases, keep current checks, document the environment gap, and do not introduce a noisy regex gate as a false
substitute.

## L3 — Add A Narrow Source Denylist Audit

After L2 is stable, inspect only high-confidence visible-string construction patterns, for example known help template
builders and user-facing service error returns. Maintain explicit exclusions for syntax, test fixtures, logs, and
generated resources.

Rules:

- every denylist match includes file/line and remediation guidance;
- allowlist entries require a reason and narrow location/pattern;
- baseline existing debt explicitly before making the rule fatal;
- new/changed violations may fail CI before historical debt is fully removed.

## L4 — CI And Runtime Verification

- Run existing `scripts/check_localization.py` and the new extraction audit in the same CI quality stage.
- Ensure Linux/macOS/Windows can locate the intended Qt tool or centralize the audit on one documented CI platform
  while all platforms continue compiling `.ts` resources.
- Add runtime/component tests for Map help, Raw help labels, and template error mapping.
- Verify English fallback contains no empty labels and placeholders remain intact.
- Manually smoke Czech and Slovak on the three fixed surfaces.

## Touched-Test And File-Splitting Rules

- New formatter/error-mapping logic uses QTest in the matching core/app/editor runtime boundary.
- Migrate `ProjectTemplateServiceTest.cpp` only as needed for the new result type; do not migrate unrelated tests.
- Extract only proven content/formatting responsibility from large files.
- Do not combine localization fixes with style/catalog/cache or renderer behavior changes.

## Exit Gate

- Known Map help, Raw help, and project-template strings are extractable and translated in Czech/Slovak.
- A fresh temporary `lupdate` audit detects removal/bypass of critical source keys.
- Existing catalog completeness and placeholder checks remain green.
- Therion syntax and user-authored content remain canonical and untranslated.
