# Therion Command Catalog Pipeline

This repository keeps generated Therion command metadata in:

- `resources/therion_command_catalog.json` (combined file)
- `resources/therion_catalog/commands/*.json` (per-command files)

The catalog is generated from a pinned snapshot of Therion (currently v6.4.0):

- `therion/thbook/ch02.tex`
- `therion/thbook/ch03.tex`
- `therion/thbook/thbook.tex`
- `therion/src/therion-core/*.h` and `*.cxx` (option tables and option parsing code)

The book gives commands, syntax, contexts, and help text. For the object commands, option names, aliases, and
value counts come from Therion's option tables instead (`thstok` tables read by each class's
`get_cmd_option_desc`, and each exporter's `parse_options` for `export`), so the catalog accepts what Therion accepts. The generator writes the differences between the
two to `docs/THERION_BOOK_DIFFERENCES.md`, the list of candidate fixes for the book. The snapshot directory
`therion/` is not committed (it is ignored); recreate it with `scripts/update_thbook_snapshot.sh`.

Source snapshot metadata is tracked in:

- `docs/therion-source.lock`

## Why this pipeline exists

- Keep completion/help/configuration metadata data-driven.
- Avoid hand-maintaining command/option/value lists in UI code.
- Make updates reproducible when upstream Therion docs change.

## Scripts

- `scripts/update_thbook_snapshot.sh`
  - Pulls `thbook` files and `src/therion-core` from a specific upstream repo + ref (tag/commit).
  - Rewrites `docs/therion-source.lock` including SHA-256 hashes.

- `scripts/generate_therion_catalog.py`
  - Parses `ch02.tex` and `ch03.tex`.
  - Produces `resources/therion_command_catalog.json`.
  - Produces per-command files in `resources/therion_catalog/commands/`.
  - Merges local per-command overrides from `resources/therion_catalog/overrides/`.
  - Applies Therion's option tables and writes `docs/THERION_BOOK_DIFFERENCES.md` (`--report`).

- `scripts/refresh_therion_catalog.sh`
  - End-to-end helper: optional snapshot update + catalog regeneration.

- `scripts/check_therion_catalog.sh`
  - CI/local check: fails if generated output differs from committed catalog.

## Recommended update workflow

1. Update snapshot from upstream Therion:

```bash
scripts/update_thbook_snapshot.sh \
  --repo https://github.com/therion/therion.git \
  --ref <tag-or-commit>
```

2. Regenerate catalog:

```bash
scripts/refresh_therion_catalog.sh --skip-update
```

3. Validate generated output:

```bash
scripts/check_therion_catalog.sh
```

4. Review and commit together:
- `therion/thbook/ch02.tex`
- `therion/thbook/ch03.tex`
- `therion/thbook/thbook.tex`
- `docs/therion-source.lock`
- `docs/THERION_BOOK_DIFFERENCES.md`
- `resources/therion_command_catalog.json`
- `resources/therion_catalog/commands/*.json`
- `resources/therion_catalog/overrides/*.override.json` (if changed)

## Override policy

Use `resources/therion_catalog/overrides/*.override.json` only for:

- alias normalization (for example `centreline` / `centerline`)
- missing data that cannot be extracted reliably from TeX
- explicit compatibility patches

Parser-level extraction is the default and required path for Therion metadata behavior changes; do not add persistent overrides for data that can be parsed from source.

Do not hardcode Therion command semantics in UI code when it can be represented in catalog JSON or per-command overrides.

## Schema extension policy

When UI behavior needs metadata that is not currently present in `resources/therion_command_catalog.json`, follow this order:

1. extend `scripts/generate_therion_catalog.py` (parser/generator level),
2. regenerate `resources/therion_command_catalog.json` (and `resources/therion_catalog/commands/*.json`),
3. update consumers to use the new generated fields.

Do not hand-edit generated catalog output to add persistent fields.

## Option-shape fields (current generator output)

Each parsed option entry includes normalized shape hints for UI/autocomplete:

- `option_key`: normalized key (`-id`, `-recursive`, `-d`, ...)
- `value_arity`: `0`, `1`, or `N`
- `value_domain`: coarse domain hint (`enum`, `number`, `string`, `path`, `keyword`, ...)

These fields are heuristic and should be improved by parser refinements and regression tests rather than edited manually in generated JSON.

## Centerline inline commands

`centreline`/`centerline` includes a dedicated structured field:

- `inline_commands`: inline centerline subcommands parsed from `\comopt` blocks (for example `data`, `date`, `team`, `units`, `station`, `extend`)

This field is intentionally separate from `options` to avoid mixing real dash-options with argument placeholders or prose tokens.

Generator also emits standalone centerline-context command entries for inline commands that do not have dedicated top-level sections, so autocomplete/help can use per-inline-command value metadata (for example `infer` -> `on/off`, `walls` -> `auto/on/off`, `mark` -> `fixed/painted/temporary`).

## Block-structure fields

Generator now emits directive-normalized and block-structure hints derived from syntax:

- command-level `directive`: normalized command token (`centreline` -> `centerline`)
- command-level `block`: structural role metadata (`leaf`, `container_open`, `container_close`)
- root-level `block_pairs`: derived open/close directive pairs used by block-aware editors

## Future-proofing guidance

- Prefer upstream tags for `--ref` (stable releases) over floating branches.
- Keep lock file as the source-of-truth for provenance and reproducibility.
- If TeX structure changes upstream, fix parser extraction in `generate_therion_catalog.py` and regenerate; avoid manual edits to generated JSON.
- Run `scripts/check_therion_catalog.sh` in CI to prevent stale catalog drift.
