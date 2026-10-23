# Therion Compatibility Guidelines

This document records Therion language concepts that must stay stable across parser, validator, project-index, completion, highlighting, and map-editor work.

Therion Studio is an editor and early-feedback tool. The Therion compiler remains authoritative for final compilation and export behavior. When this document, implementation, tests, generated metadata, and upstream Therion documentation disagree, resolve the conflict explicitly and add focused regression coverage.

## Source Compatibility Principles

- Preserve canonical Therion spelling in source files, examples, generated edits, completion entries, and diagnostics. UI text may be localized, but command names, option names, object identifiers, and source snippets remain Therion syntax.
- Preserve source text losslessly unless the user performs an explicit edit. Comments, blank lines, unknown but valid directives, ordering, continuation lines, indentation where practical, source encoding, and newline style are part of document integrity.
- Prefer conservative diagnostics over speculative errors. If Therion accepts a construct or the rule is not fully modeled, avoid blocking or alarming the user. A finding is an error only when Therion rejects the input; Therion accepts a stray `endcode` in a layout and a repeated option, so those are warnings.
- Prefer source-range-aware core parsing over UI heuristics. Completion, highlighting, context help, validation, Structure, and map-editor projections should consume shared logical command/source metadata.
- Quoted option values remain values even when their text begins with `-`; projections shall preserve token quoting information rather than reclassifying a quoted value as an option from its text alone.
- Option metadata for the object commands and `layout` (`point`, `line`, `area`, `scrap`, `survey`, `centreline`, `map`, `surface`, `join`, `import`, `scan`, `grade`, `lookup`, `layout`) comes from Therion's option tables in the pinned source (`docs/therion-source.lock`): the `thstok` table each class's `get_cmd_option_desc` reads, its parent classes, and the value count that function (or `thlayout::get_default_cod`) returns, one value unless it says otherwise. Options whose `set` rejects them on the command line (`not a command line option`) are left out. `export` options are the union of `thexport` and the exporters (`thexpmap`, `thexpmodel`, `thexptable`, `thexpdb`), each taking one value as their `parse_options` reads it; `-layout-xxx` stands for the layout options `export map` passes on. Inside a `layout` block Therion reads each line against the same layout option table, so a layout block command takes as many values as its layout option; a command name shared with another block (`units` in a centreline) is only relaxed to the smaller count. The book supplies help text only. Differences between the two are listed in `docs/THERION_BOOK_DIFFERENCES.md`.
- Command lines are split into values as Therion's `thsplit_args` does (`src/therion-core/thparse.cxx`, Therion 6.4.0, upstream commit `963230bb`). A value that starts with `[` at the start of a word is one value up to its matching `]`: brackets nest, `#`, `%`, and quotes inside it are ordinary characters, and an unclosed bracket runs to the end of the line (a trailing line-continuation `\` stays separate). The token keeps its brackets so edits can preserve them; code that needs the words inside splits them on whitespace, as Therion's `thsplit_words` does (`TherionDocumentParser::bracketedValueSpans`). A `[` or `"` inside a word does not start a bracketed or quoted value. Inside a quoted value `""` is one quote; generated edits write quotes that way and never use backslash escapes. `#` starts a comment only at the start of a word; single quotes, `%`, and a backslash inside a line are ordinary characters. A line whose last non-space character is `\` continues on the next line (`thinput.cxx`); Therion joins the lines before reading any command, so a continuation line never opens or closes a block, whatever word it starts with. This applies inside `code ... endcode` too: a TeX line ending in the line break `\\` joins the next line, so if that line is `endcode`, Therion passes `endcode` to TeX as text (checked with Therion 6.4.0). Lines inside `code ... endcode` are split by the same rules; for display only, the editor colours them by their block's language instead (TeX for `tex-map` and `tex-atlas`, where a `%` not escaped by `\` starts a comment, MetaPost for `metapost` and `mpost`, where a `%` outside a `"..."` string starts a comment). Therion drops any line whose first non-blank character is `#`, inside code blocks too (`thinput.cxx`), so TeX or MetaPost never sees it (checked with Therion 6.4.0); the editor colours such a line as a comment.
- Use real Therion examples or reduced fixtures from real patterns when adding or changing parser, namespace, reference, or validation behavior.

## File Roles

- `.th` files define survey structure, centerline data, maps, scraps imported through `input`, and higher-level project source.
- `.th2` files define map drawings: `scrap`, `point`, `line`, `area`, and related drawing/source metadata.
- Therion config files (`thconfig`, `thconfig.*`, `*.thconfig`) define build selection and export workflow using commands such as `source`, `select`, `layout`, and `export`.
- A file's role follows how Therion reads it, not only its name: a file a config reads with `input` (and anything that file inputs) is config text, whatever its extension, for example a shared `common_layout.th` holding `layout` blocks. Project validation and the open editor check such a file as a config; a file read through `source`, or input from a `.th` file, keeps the role its name gives.
- `input` and `source` resolve file references relative to the file containing the command unless Therion syntax or project configuration says otherwise. The project validator should use the current saved project files plus open in-memory document contents. Relative references with a leading `./` and Windows-style backslash separators should resolve to the same project files as their forward-slash equivalents, but source edits and quick fixes should prefer portable `/` separators.
- `code ... endcode` blocks, including code blocks inside `layout`, contain raw backend code. Their body rows should not be interpreted as nested Therion commands or block openers; only the matching `endcode` line closes the block, except that the enclosing block's close (`endlayout`) also ends it, as Therion ends a layout at `endlayout` even inside a code block. Therion reads a line inside a code block whose first word is a layout command (`scale`, `legend`, another `code`) as a layout setting, not as code (checked with Therion 6.4.0, keeping its TeX output). Studio reports either case as one warning per code block; its fix inserts `endcode` before the first layout command, or before `endlayout` when there is none, and is offered only when the block has no `endcode` of its own and no code follows that point. Closing directives should validate their catalog context against the parent block after the close, so `endcode` inside `layout` is evaluated in `layout` context rather than `code` context.

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
- Map drawing object IDs (`point -id`, `line -id`, `area -id`) are not scoped by scrap. They share the survey namespace with each other and with `survey`, `map` and `scrap` names, so an ID must be unique across every scrap and `.th2` file in that survey. Scraps are not namespaces.
- Station names are scoped by survey/centerline namespace. Repeated station names in different surveys are valid.
- Centreline station names share the survey namespace with drawing object IDs, so a `point`, `line` or `area` `-id` equal to a station name in the same survey fails in Therion (`object already exist`).
- Therion compares names case-sensitively, so `wall` and `Wall` are different IDs.
- Therion accepts a station point repeated in a scrap, at the same place or elsewhere, and a station drawn in several scraps, without an error or a warning (checked with Therion 6.4.0). A station point is a morphing control point, so a repeat at a different place in one scrap is very likely a mistake, while a station shared by two scraps of the same projection is how scraps join.
- Duplicate diagnostics should point at the later repeated identifier token, not the whole line, when the source range is available.
- Cross-file duplicate checks must respect the effective Therion namespace, not only the physical file path.

## Map And Scrap Concepts

- `map ... endmap` contains references to child maps or scraps. These references are composition relationships, not ownership.
- Therion looks up every map item as an object and stops with `object does not exist` when none has that name, whatever the name looks like (`thdb2d::process_map_references`). A survey is also a valid map item, looked up in the namespace its name belongs to. Naming schemes such as `.m`/`.s` endings are user conventions, not Therion rules, so they may only choose a diagnostic's wording, never whether an unresolved item is reported.
- Therion also stops on a map that mixes scraps with maps or surveys (the first item decides; a survey counts as a map), a point, line, or area `-id` as a map item (`invalid map object`), an empty or preview-only map, an item line that is not `<name>` or `<name> [<x> <y> [<units>]] above|below|none` (the shift is read only with `maps-offset on`, the default), a `preview` type other than `above` or `below`, and a `map -survey` that is not a survey looked up from the map's own survey (so a map cannot name its enclosing survey). The type words are case-sensitive.
- Every item of a map must have one projection, and the map's `-projection` (or `-proj`) when it has one. Therion compares projections by type, `:index`, and the elevation angle as written (`thdb2d::parse_projection`), so `[elevation 315]` and `[elevation 315.0]` differ while `[elevation 315 deg]` matches `[elevation 315]`. A scrap without `-projection` is `plan`; a survey item takes the map's projection; a child map has the projection of its content. A loop of maps is an error (`recursive map reference`, given without a line); a map that lists itself is also rejected, with the misleading `map cannot contain only previews`.
- Therion reads a shift on the first scrap of a map but rejects one on any later scrap (`shift is not allowed for scrap`), even `[0 0 m] none`; maps may be shifted. A survey item that comes first needs the map's `-projection`, and a survey cannot be drawn in projection `none`. A `preview` must name an existing map (`not a map reference` for a scrap, `invalid map object` for anything else) with the same projection as the map.
- `scrap ... endscrap` groups map objects but is not a namespace. Area border references resolve to `line -id` values in the survey namespace, and Therion requires the line to be in the same scrap as the area.
- `join` may reference line endpoints and marked line points. `:0` and `:end` identify the start and end of a resolved line; named suffixes such as `:k1` should resolve to a `mark k1` row inside that target line when the line reference itself is unambiguous.
- `.th2` visual edits must serialize canonical Therion source. Map-editor conveniences must not invent alternate source syntax.
- XTherion metadata comments such as `##XTHERION## ...` and Mapiah background metadata comments such as `##MAPIAH## image_insert_v1 ...` are source data and should be preserved unless a specific map operation intentionally updates them.
- Simple background placement should keep XTherion-compatible `xth_me_image_insert` metadata. Advanced background transforms that XTherion cannot represent, such as rotation, non-uniform scale, or a custom pivot, should use Mapiah `image_insert_v1` metadata rather than duplicating placement in an editor-specific parallel record.

## Validation And Project Indexing

- Validation owns problem reporting. Structure owns project orientation and navigation. Do not turn Structure into a general validator.
- The project index is a lightweight projection for navigation, relationships, and early feedback. It must not be treated as a full Therion compiler.
- Project-index diagnostics should be conservative and source-range-aware. Prefer suppressing uncertain findings over producing false positives in valid Therion projects.
- Active-document validation and project validation should share rules where practical so open saved and unsaved documents behave consistently.
- A scrap option `-station-names <prefix> <suffix>` adds the prefix and suffix to station names used by station points in that scrap; `[]` denotes an empty prefix or suffix. Project-index station-reference validation shall apply that transform before resolving the reference in the owning survey namespace.
- A `.th2` file outside the project source graph has no owning survey namespace, and Therion does not compile it.
  Project validation reports such a drawing once, as an `Information` note (`drawing-not-read`), and does not check its
  station references.
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
