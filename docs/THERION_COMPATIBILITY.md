# Therion Compatibility Guidelines

This document records Therion language concepts that must stay stable across parser, validator, project-index, completion, highlighting, and map-editor work.

Therion Studio is an editor and early-feedback tool. The Therion compiler remains authoritative for final compilation and export behavior. When this document, implementation, tests, generated metadata, and upstream Therion documentation disagree, resolve the conflict explicitly and add focused regression coverage.

## Source Compatibility Principles

- Preserve canonical Therion spelling in source files, examples, generated edits, completion entries, and diagnostics. UI text may be localized, but command names, option names, object identifiers, and source snippets remain Therion syntax.
- Preserve source text losslessly unless the user performs an explicit edit. Comments, blank lines, unknown but valid directives, ordering, continuation lines, indentation where practical, source encoding, and newline style are part of document integrity.
- Prefer conservative diagnostics over speculative errors. If Therion accepts a construct or the rule is not fully modeled, avoid blocking or alarming the user.
- Prefer source-range-aware core parsing over UI heuristics. Completion, highlighting, context help, validation, Structure, and map-editor projections should consume shared logical command/source metadata.
- Use real Therion examples or reduced fixtures from real patterns when adding or changing parser, namespace, reference, or validation behavior.

## File Roles

- `.th` files define survey structure, centerline data, maps, scraps imported through `input`, and higher-level project source.
- `.th2` files define map drawings: `scrap`, `point`, `line`, `area`, and related drawing/source metadata.
- Therion config files (`thconfig`, `thconfig.*`, `*.thconfig`) define build selection and export workflow using commands such as `source`, `select`, `layout`, and `export`.
- `input` and `source` resolve file references relative to the file containing the command unless Therion syntax or project configuration says otherwise. The project validator should use the current saved project files plus open in-memory document contents. Relative references with a leading `./` and Windows-style backslash separators should resolve to the same project files as their forward-slash equivalents, but source edits and quick fixes should prefer portable `/` separators.
- `code ... endcode` blocks, including code blocks inside `layout`, contain raw backend code. Their body rows should not be interpreted as nested Therion commands or block openers; only the matching `endcode` line closes the block. A parent-context command such as layout `scale`, `legend`, or another `code` before `endcode` should report the active `code` block as unclosed. Closing directives should validate their catalog context against the parent block after the close, so `endcode` inside `layout` is evaluated in `layout` context rather than `code` context.

## Namespaces And References

- `survey` is the main hierarchical data structure. Surveys may be nested.
- Each survey creates a namespace from its `<id>` unless Therion options explicitly disable namespace creation, for example `-namespace off`.
- Therion qualified references are written from innermost referenced survey outward. For example, object `27` in survey `hp` nested in survey `stara_vetrna` is referenced as `27@hp.stara_vetrna` from outside that branch.
- From inside the owning namespace, shorter relative references are valid. For example, within the relevant parent context, `27@hp` may refer to the same station as `27@hp.stara_vetrna`.
- Object identifiers only need to be unique in their owning namespace. Identical station or object names in different surveys are valid and common.
- Reference resolution should be contextual: resolve names relative to the owner command's survey/scrap/map namespace before considering broader project matches.
- Do not change namespace order to filesystem-like `parent.child`; Therion reference order is `child.parent`.

## Identifier Uniqueness

- `survey`, `map`, and `scrap` names share the parent survey namespace for reference purposes. A duplicate among these names in the same parent namespace is a compatibility risk and should be diagnosed conservatively when source ranges are known.
- Map drawing object IDs (`point -id`, `line -id`, `area -id`) share the containing scrap namespace. They are not unique only per object type.
- Station names are scoped by survey/centerline namespace. Repeated station names in different surveys are valid.
- Duplicate diagnostics should point at the later repeated identifier token, not the whole line, when the source range is available.
- Cross-file duplicate checks must respect the effective Therion namespace, not only the physical file path.

## Map And Scrap Concepts

- `map ... endmap` contains references to child maps or scraps. These references are composition relationships, not ownership.
- `scrap ... endscrap` is a drawing namespace for map objects. Area border references resolve to `line -id` values in the same scrap unless Therion semantics provide a more specific scope.
- `join` may reference line endpoints and marked line points. `:0` and `:end` identify the start and end of a resolved line; named suffixes such as `:k1` should resolve to a `mark k1` row inside that target line when the line reference itself is unambiguous.
- `.th2` visual edits must serialize canonical Therion source. Map-editor conveniences must not invent alternate source syntax.
- XTherion metadata comments such as `##XTHERION## ...` and Mapiah background metadata comments such as `##MAPIAH## image_insert_v1 ...` are source data and should be preserved unless a specific map operation intentionally updates them.
- Simple background placement should keep XTherion-compatible `xth_me_image_insert` metadata. Advanced background transforms that XTherion cannot represent, such as rotation, non-uniform scale, or a custom pivot, should use Mapiah `image_insert_v1` metadata rather than duplicating placement in an editor-specific parallel record.

## Validation And Project Indexing

- Validation owns problem reporting. Structure owns project orientation and navigation. Do not turn Structure into a general validator.
- The project index is a lightweight projection for navigation, relationships, and early feedback. It must not be treated as a full Therion compiler.
- Project-index diagnostics should be conservative and source-range-aware. Prefer suppressing uncertain findings over producing false positives in valid Therion projects.
- Active-document validation and project validation should share rules where practical so open saved and unsaved documents behave consistently.
- Inline highlighter diagnostics should use the same source ranges and severity as the validation diagnostics they represent.

## Metadata Sources

- Command, option, help, style, and symbol metadata should come from the generated Therion catalog where possible. See `docs/THERION_COMMAND_CATALOG_PIPELINE.md`.
- Catalog option names ending in `xxx` represent Therion option-family metadata, not literal source options. `export map -layout-xxx` should match only concrete options known on the `layout` command, for example `-layout-scale` matching layout option `-scale`; literal `-layout-xxx` source text should remain an unknown option.
- Do not solve command semantics by hardcoding UI-side lists when the catalog generator or source model can own the rule.
- Local catalog overrides are a short-term compatibility patch, not the default path for durable Therion language behavior.

## Verification Requirements

Changes touching Therion parsing, source ranges, namespace/reference resolution, duplicate detection, serialization, map/source synchronization, command metadata, or validation should include focused tests for:

- valid nested survey references using Therion order (`child.parent`),
- relative references from inside nested namespaces,
- same-name objects in different surveys,
- duplicate names/IDs in the same namespace,
- `.th`, `.th2`, and config document-type differences when relevant,
- unsaved editor contents participating in project validation,
- source ranges for diagnostics and highlighter overlays.
