# Validation Accuracy Plan

Date: 2026-10-15

Status: V1a, V1b, V2a, V2b, V3, V4, V4b, V5, V5b, V6, and V6b done; V6c and V6d done (2026-10-23). Updated 2026-10-21 with GitHub issues #49 and #50, help linking, bracketed
values, Therion's line splitting (`thsplit_args`) first in two parts (V1a, V1b), code blocks as a potential slice,
and slices renumbered.

Scope: remove false project-validation findings so that the Validation panel can be trusted, starting with the
classes measured on a real multi-survey cave project, with tests that stop the same classes from returning.

## Why

On a local real cave project (about 180 source files, one root `thconfig` and several per-survey configs), project
validation reported 243 findings. Checked against Therion itself and the source files, about 231 were false and four
causes explain nearly all of them. Users stop reading a panel with that much noise, so real problems are missed.

## Measured Findings

| Findings | Verdict | Cause |
| --- | --- | --- |
| 59 warnings `unknown-option`, all `export ... -proj` | false | The command catalog is generated from the Therion book, which documents `-projection` only. Therion's option table (`thtt_expmap_opt` in `src/therion-core/thexpmap.h`) accepts both `-proj` and `-projection`; `-pro` is rejected, so this is an alias, not prefix matching. The same `export` catalog entry also lists Therion command-line switches (`-d`, `-q`, `-L`, `--print-symbols`, ...) that are not export options. |
| 155 errors `unknown-station-reference` "file is not included in the project source graph" | false as errors | Every one is a station point in a drawing the selected target config does not read. Some of those drawings belong to separate per-survey configs in the same folder (their `.th` inputs them; they are only commented out of the main survey); others are not read by any config. Each station of such a drawing is reported as an error. |
| 8 findings in the root `thconfig` (unclosed `layout`, unclosed `survey`, unmatched `endlayout`, 4 `invalid-command-context`) | false, one cause | A quoted `map-comment "...<br>\` value continued over several lines with `\`. The source model breaks the quoted value, so a continued line starting `Survey by ...` is read as a `survey` command and the rest follows. Therion accepts the file. |
| 9 errors `unknown-join-reference` to objects in a `-namespace off` survey | false | Already fixed on the separate `NamespaceOffBug` branch (objects in a `-namespace off` survey resolve through the survey name); waiting for its PR. |
| 1 error `missing-source-reference` | real | A per-survey config sources a file name that does not exist (a misspelling); that config would fail in Therion. |
| 12 warnings `empty-scrap-object` (`line ...` immediately followed by `endline`) | probably real | Harmless empty leftovers; a warning is appropriate. |

To collect the same report again, open the project and use `Export Markdown...` in the Validation panel (or a
scratch tool around `ProjectValidationScanner`; one is kept outside the repository).

## Principles

- Therion is the referee. A finding is an error only when Therion itself rejects the input. Therion warnings, and
  input that is very likely a mistake, are warnings. Style or heuristic notes are at most informational.
- Metadata comes from Therion's own sources. Fix catalog gaps in the generator, not by editing generated catalog
  output or adding long-lived overrides (see `AGENTS.md`, "Data Integrity Rules").
- All slices are on one branch, `ValidationAccuracy` from `main`, one commit (or a short series) per slice in number
  order, because later slices build on earlier ones (V2's checks read values through V1's splitting). Each slice
  has focused tests built from small repository fixtures and its own documentation updates (specification,
  compatibility notes, manuals, translations, WORKLOG); no local cave data in the repository, commit messages, or
  WORKLOG.
- Parser and source-model changes are high risk: they need round-trip tests and must fix the shared model, not one
  projection.

## V1 - Line Splitting Matches Therion's `thsplit_args`

Goal: Therion Studio splits a command line into values exactly as Therion does, so every later check and test reads
the same values Therion reads. GitHub issue #49 (bracketed label text).

Therion splits every command line in `thsplit_args` (`src/therion-core/thparse.cxx`, called from `thinput.cxx`) before
any keyword lookup. Its rules:

- a plain word ends at a space; `#` at the start of a word starts a comment;
- `"..."` is one value; `""` inside it is a literal quote;
- `[...]` is one value with the brackets removed; brackets nest, so `[a [b] c]` stays one value;
- a quote or bracket inside a plain word does not start a quoted value;
- an unclosed quoted or bracketed value runs to the end of the line;
- single quotes, `%`, and a `\` that does not end the line are ordinary characters (a `\` at the end of a line joins
  it to the next, in `thinput.cxx`).

Studio does not call or link `thsplit_args`; it returns values without their source positions and with brackets and
quotes removed, works on byte strings, and would bring in Therion's core. Studio's own tokenizer follows the same
rules, written from Therion's source; the Therion version they were taken from is recorded in
`docs/THERION_COMPATIBILITY.md`, and the table-driven test below keeps them in step. A later change in Therion is not
caught automatically (V8 would catch it).

The tokenizer (`TherionDocumentParser::parseLine`) is shared by every line of `.th`, `.th2`, and `thconfig` files and
by values typed into fields (Blocks and `Edit All Object Settings` split typed option values with it in
`TherionCommandSyntax`; the map editor splits lines it builds from inspector input). It also runs on lines inside
`code ... endcode` blocks (TeX and MetaPost in `layout`), which Therion passes on without splitting; those lines are
marked as raw content afterwards and not validated, so for them the splitting affects only highlighting.

V1 is done in two commits, each with its tests and documentation.

### V1a - Bracketed Values

- First, characterisation tests with no behaviour change: what the tokenizer returns today for brackets, quotes,
  `%`, `#`, and `\`, in command lines, in code block lines, and in typed field values, so each later change shows
  exactly what it alters.
- Make the tokenizer return a bracketed value as one token that keeps its brackets in its text and span, so edits
  keep them and highlighting and underlines still line up. Uses that need the words inside (scrap `-scale [...]`)
  split the content on whitespace, as Therion's `thsplit_words` does (`TherionDocumentParser::bracketedValueSpans`).
  Label text is shown without the outer brackets (`therionValueText`), as quoted text is shown without quotes.
- Move the places that rejoin bracketed values today onto the shared rule and remove their own code:
  `TherionCommandLineModel` (option boundaries and values), `TherionDocumentEditor` (option ranges),
  `MapEditorInspectorData` (label text and scrap scale), `MapEditorSceneItems` (scrap scale), and
  `ProjectStructureIndex` (map offsets). The book-syntax readers that treat `[...]` as "optional" are not source text
  and stay as they are.
- Editing keeps the user's form: a bracketed label stays bracketed when edited, and is not turned into quotes.
- Check what grouping brackets does to highlighting of TeX and MetaPost lines in code blocks.
- Tests: table-driven cases for the bracket rules; round-trip of old files with bracketed labels (text unchanged); a
  bracketed multi-word label with `<br>` shows in full in the inspector and edits without error; scrap scale and map
  offsets read the same numbers as before.
- Documentation: `SPECIFICATION.md` (bracketed values are one value and keep their form when edited);
  `docs/THERION_COMPATIBILITY.md` (the bracket rules, citing `thsplit_args` and the Therion version). Check the user
  manuals' label editing text and update it in all four languages if it describes quoting.

### V1b - Quotes, Comments, And Backslashes

- Single quotes become ordinary characters (`-text Dan's`), `%` is no longer a comment, `#` starts a comment only at
  the start of a word, a `\` that does not end the line is an ordinary character, a quote inside a word does not start
  a quoted value, and `""` inside a quoted value is a literal quote. Only a final `\` continues a line (Therion does
  not count backslashes); the parser and the logical document share that rule.
- Generated edits write quotes as Therion reads them: double quotes with `""` for a quote, no backslash escapes, from
  one shared writer (`quoteTherionArgument`). Studio used to write `\"`, which Therion rejects.
- In command lines this follows Therion: a line Therion would reject may now get a finding it did not get before
  (for example text after a `%` that Studio used to ignore).
- In code blocks the only effect is highlighting: TeX and MetaPost `%` comments are no longer coloured as comments,
  and TeX macro arguments such as `#1` are no longer shown as comments, until V6.
- Tests: table-driven cases for each rule; the characterisation tests from V1a show every change, and an existing
  test that expects the old behaviour is changed only with the evidence and approval (`AGENTS.md`).
- Documentation: `SPECIFICATION.md` and `docs/THERION_COMPATIBILITY.md` (the rules above).

V1 comes first because V2's tests and checks read option values through this splitting.

## V2 - Catalog Options From Therion's Option Tables

Done in two parts (decided 2026-10-21): V2a for the object commands whose options Therion reads through
`get_cmd_option_desc` (`point`, `line`, `area`, `scrap`, `survey`, `centreline`, `map`, `surface`, `join`, `import`,
`scan`, `grade`, `lookup`), with the options all Therion objects share listed on every one and `map -colour` removed
(the book lists it, Therion rejects it); V2b for `export` and `layout`, whose value counts are written case by case
in each exporter's and the layout's parsing code (this fixes the `-proj` warnings). V2b also gives the commands
inside a `layout` block Therion's value counts (`page-setup` 7, `statistics` 2, `units` 1), as Therion reads those
lines against the same layout option table.

Found while doing V2a: the lock named Therion v6.3.4, but the catalog had been generated from the v6.4.0 book; the
generator's source step looked for `thpoint.h` at the top level and silently did nothing since Therion moved its code
to `src/therion-core`. Both are fixed; the snapshot now includes `src/therion-core`.

Goal: option names, aliases, whether an option takes a value, and allowed values match what Therion accepts. GitHub
issue #50 (`-scale` rejected for labels).

- Extend `scripts/generate_therion_catalog.py` to read Therion's `thstok` tables (for example `thtt_expmap_opt`,
  `thtt_expmap_fmt` in `thexpmap.h`, and the equivalent tables for the other commands) from the same Therion checkout
  it already reads the book from, and merge them with the book-derived metadata: the tables give the exact option
  and value tokens; the book still gives summaries, syntax, and contexts.
- The generator reads Therion's source files as text when it is run; Therion code is not called or linked, and the app
  reads only the generated catalog. The Therion version is recorded in `docs/therion-source.lock` with the book files,
  and new Therion options reach Studio only when the generator is run again.
- Value arity comes from Therion, not from the book's signature. Today an option whose book heading shows no
  placeholder is generated as taking no value: `point` `-align`, `-scale`, `-text`, `-value` and `line` `-scale`.
  `Edit All Object Settings` then rejects `-scale l` (#50) and treats a multi-word `-text` as stray tokens (the other
  half of #49). Where the tables give only names, read the arity from the option's parsing code in Therion (for
  example `thpoint.cxx` `set`), or record it in the generator with a comment pointing to that code.
- Treat options that the tables do not list for a command as suspect; this removes the command-line switches that were
  attached to `export`.
- Help stays linked to the book. Option entries are matched to book text by option name; an alias (for example
  `-proj`) shows the help of its full name (`-projection`). An option Therion accepts but the book does not describe
  is still accepted and shows a short "not described in the Therion book" note.
- Have the generator write a report of differences between the book and the tables (options or aliases in one but not
  the other, and options whose book heading omits their value).
- One catalog serves every user of it (validation, highlighting, completion, context help, Blocks, the map inspector,
  and `Edit All Object Settings`), so this fixes all of them together; no per-surface option lists are added.
- Tests (no test per option):
  - one table-driven test that, for every command and option in the extracted tables, the generated catalog has the
    same name, aliases, and value arity;
  - generator unit tests on small extracted table and book snippets, including alias-to-help linking;
  - a catalog regression test that `export` accepts `-proj`, still accepts `-o`/`-output`, and no longer lists
    command-line switches, and that `point -scale l`, `-align`, and `-text` take values.
- Keep generated catalog updates in their own commit, separate from generator code (`AGENTS.md`).
- Documentation:
  - `SPECIFICATION.md`: replace "Catalog-derived zero-value option arity shall not by itself be treated as a validation
    error ..." with the rule that option names, aliases, and arity come from Therion's option tables, and describe the
    note shown for options the book does not describe;
  - `docs/THERION_COMPATIBILITY.md`, `docs/THERION_COMMAND_CATALOG_PIPELINE.md`, and
    `resources/therion_catalog/README.md`: the tables as the source of option metadata, the book as the source of
    help text, and the difference report;
  - user manuals (English, Czech, French, Slovak): context help for options the book does not describe;
  - translations for the new note.
- Upstream: send the book fixes from the difference report to the Therion project as a separate change (missing
  aliases such as `-proj`, option headings that omit their value such as `-scale`, `-text`, `-align`, `-value`).
  Therion Studio does not wait for it; help text improves as the book is fixed.

## V3 - Line Continuation Inside Quoted Values

Goal: a quoted value continued with a trailing `\` stays one value in every projection.

Done (2026-10-21). The cause was not quoting: the logical document already joined the lines, but the block structure
was built line by line and read a continuation line starting with a command word (`Survey by ...`) as a `survey`
command. Continuation lines are now part of their command in the block structure too (`continuesPreviousLine`).
Inside `code ... endcode` the physical `endcode` still closes the block; the V5 warning covers a TeX line ending in
`\\` before it. Not changed: Studio joins continued lines with one space, while Therion joins them with no space and
keeps the next line's leading spaces; this only affects the joined value text.

Follow-up found by V3 (decided 2026-10-21): a layout's `code` block without `endcode` made Studio read the following
layouts as TeX, because only `endcode` closed a code block. Therion ends the layout, and the code block with it, at
`endlayout` (checked with Therion 6.4.0). Studio now does the same. Therion also reads a layout command inside an open
code block (`scale`, `legend`) as a layout setting, so both cases give one warning per code block (the earlier check
gave an error); the fix inserts `endcode` before the first layout command, or before `endlayout`, and is offered only
when the block has no `endcode` of its own and no code follows that point.

- Find where the shared source model (`TherionSourceText` / `TherionSourceLogicalDocument`) joins continued physical
  lines and why a quoted value spanning lines is split. Fix it there, not in validation, using V1's splitting rules.
- Tests: round-trip of a multi-line quoted `map-comment` in a `layout` block (text unchanged, one logical command,
  correct physical ranges); no `survey`/`endlayout` findings for that fixture; existing parser and highlighting tests
  unchanged.
- Documentation: `SPECIFICATION.md` (logical command lines keep a continued quoted value as one value) and
  `docs/THERION_COMPATIBILITY.md` (the continuation rule). No user manual change expected, as this is a bug fix.

## V4 - Drawings Outside The Target Config's Source Graph

Goal: no per-station errors for drawings the selected target config does not read.

Done (2026-10-21). Studio gained a third severity, `Information` (decided with the user): listed in the Validation
panel in blue, not counted as a problem by the rail indicator, shown by default. Each unread `.th2` drawing gets one
`drawing-not-read` information finding. Measured: with a whole multi-cave folder open and one cave's config as target,
1931 per-station errors in unread drawings (the result was cut at 2000) become one note per drawing.

## V4b - Document Type From How Therion Reads A File

Goal: a file's commands are checked against the file type Therion reads it as, not its extension.

Done (2026-10-21). The project snapshot follows `input` from every config (and inputs of inputs) and records those
files as read as config (`ProjectSourceDocument::readAsType`); project validation uses that type, and each scan passes
it to open editor tabs so their own checks agree. Measured: the 50 `invalid-document-type` warnings in a shared layout
file are gone. Not changed: completion in such a tab still offers commands for the type its name gives.

Studio decides a file's type from its name (`.th`, `.th2`, `thconfig`). Therion goes by how the file is read: a file a
config reads with `input` is read as config text, whatever it is called. A shared layout file such as
`common_layout.th`, read by a `thconfig` through `input`, holds `layout` and other config commands, so Studio reports
each of them as "not listed for document type `th`" (50 warnings on the measured project).

- In the project source graph, record the document type each file is read as (config through `source`/`input` from a
  config, survey source through `source` or `input` from a `.th` file, drawings through `input` of `.th2`).
- Validate a file against the type it is read as; for a file read in more than one way, accept commands valid for any
  of them. Files nobody reads keep the extension-based type.
- Tests: a `thconfig` that inputs `common_layout.th` holding a layout; no document-type findings for it; a `.th` read
  as survey source keeps today's checks.
- Documentation: `SPECIFICATION.md` and `docs/THERION_COMPATIBILITY.md` (file roles); user manuals if visible.

- Do not validate station references in drawings outside the selected config's source graph.
- Instead report the drawing once, as an informational finding, for example "not read by the selected Target Config",
  so users who expect it to be read can see why it is not.
- Tests: a fixture with a root config, an unread drawing with station points, and a drawing read through `input`;
  only the unread drawing gets one informational finding; reachable drawings keep today's checks.
- Documentation: `SPECIFICATION.md` (project validation scope follows the target config, and the informational
  finding); `docs/THERION_COMPATIBILITY.md` (the target-config scope rule); user manuals' Validation section in all
  four languages; translations for the new finding text.

## V5 - Severity Policy

Goal: errors mean "Therion would fail".

Done (2026-10-21). Every error class was checked against Therion 6.4.0 on small projects:

| Finding | Therion | Result |
| --- | --- | --- |
| `unknown-station-reference` (`point station -name` not in the centreline) | `station does not exist` | error |
| `duplicate-object-id` | `duplicate object name` | error |
| `unknown-area-line-reference` | `object does not exist` | error |
| `unmatched-block-close` (`endscrap`, `endsurvey`, `endlayout`) | `missing scrap before endscrap` and similar | error |
| `unmatched-block-close` (`endcode` in a layout with no code block) | accepted, ignored | now a warning, with a fix that removes the line |
| `unclosed-block` at the end of a file (scrap, survey, layout) | `incomplete survey - endsurvey pair` and similar | error |
| `missing-argument`, `missing-option-value`, `wrong-option-value-count` | `not enough command/option arguments` | error |
| `malformed-option-token`: quoted option-like token, or `-subtype` with one | `unknown option`, `invalid line subtype` | error |
| `malformed-option-token`: the same option and value repeated | accepted | now a warning |
| unknown and ambiguous map, scrap, and join references | `object does not exist` | error |
| `unknown-join-line-point-mark` | `marked station not found` | error |
| `missing-source-reference` | `can't open file for input` | error |

Warnings stay warnings where Therion would fail but Studio's knowledge may be incomplete (unknown commands and
options, unknown values). Added: `code-line-joins-endcode`, a warning for a code line ending in `\` before `endcode`.

`SPECIFICATION.md` lists what validation checks but has no rule for which findings are errors, warnings, or
informational. This slice is useful on its own: it gives one rule for every check, and applying it removes false
errors that remain after V1-V4. The referee test (V8) would only automate checking that the rule is kept.

- Write the policy (the first principle above) into `SPECIFICATION.md` next to the project validation requirements.
- Review every diagnostic code against it and adjust severities; list each change and its reason in the commit.
- Tests: the existing tests for each changed code are updated to the new severity only where the policy requires it
  (with the evidence, as `AGENTS.md` asks); a small fixture per changed class.
- New warning: a line inside `code ... endcode` that ends in `\` (for example the TeX line break `\\`) directly
  before `endcode`. Therion joins the two lines and passes `endcode` to TeX or MetaPost as text without an error
  (checked with Therion 6.4.0), so the output silently changes. A warning, as Therion accepts the file; translations
  for the new text.
- Documentation: user manuals (all four languages) say what error, warning, and informational mean;
  `docs/THERION_COMPATIBILITY.md` where it names severities.

## V5b - Drawing Object IDs Are Unique In The Survey

Goal: report a duplicate `point`, `line` or `area` `-id` wherever Therion reports it, and only there.

Done (2026-10-22). The real project used for the findings table has no duplicate IDs and gains no findings; IDs
repeated across scraps and IDs equal to a station name were added by hand in the app and reported as expected. A
trial merge with `NamespaceOffBug` is clean. That branch makes objects in a `-namespace off` survey resolve through
the survey name, and this slice puts their IDs in the parent's namespace, which is the same Therion rule seen from
lookup and from uniqueness. Also found while testing, the index scanned files in hash order, so a duplicate across
two files could be reported on the earlier one; it now scans in source graph order and reports the later one, as
Therion does.

Therion keys every named object by its survey namespace, not its scrap (`thdatabase.cxx`, `duplicate object name`,
the map is keyed by name and `get_nss()` survey). Drawing object IDs, `survey`, `map` and `scrap` names all share
that map, so an ID must be unique across every scrap and `.th2` file the survey reads. Area borders are also looked
up in the survey, then must be in the area's scrap (`thdb2d.cxx`, `line outside of current scrap`), which the
existing `unknown-area-line-reference` check already covers.

Both duplicate checks key drawing object IDs by their scrap, so duplicates in different scraps or files of one
survey go unreported (Therion fails with an error).

- Raw editor (`TherionSourceValidator.cpp`): `duplicateObjectIdKey` uses `objectIdScopeKey`, the whole block stack,
  which includes the scrap. Key drawing object IDs with the same scope as `survey`, `map` and `scrap` names, built
  from the survey frames only and skipping `-namespace off` surveys like the index does, and put both in one key
  space, so a point `-id` equal to a scrap name is also found. In a `.th2` without survey blocks the scope is the
  file. One key function replaces `duplicateObjectIdKey` and `duplicateNamespaceNameKey`.
- Project index (`ProjectStructureIndex.cpp`): `duplicateIdentityLookupKey` keys drawing objects by
  `parentObjectId` (the scrap). Use `entry.namespacePath`, which the source graph scan already sets from the survey
  that inputs the drawing (with `-namespace off` applied), in the same `namespace@` key space as survey, map and scrap
  names. This finds duplicates across `.th2` files read by one survey. `structureEntryUsesScrapObjectIdNamespace`
  goes.
- Drawings with no owning survey: with a target config, only files it reads are scanned (V4), so nothing changes.
  Without a config the index falls back to every unincluded file as a root, and unrelated drawings would all share
  the empty namespace. In that case key them by file, so unrelated drawings are not reported against each other.
- Station names: centreline stations go into the same map, so a drawing object `-id` equal to a station name in the
  same survey also fails. Checked with Therion 6.4.0, a label `-id 3` in a survey with station `3` gives
  `object already exist -- 3`, reported on the centreline data line. The project index already collects stations
  by survey namespace for the station-reference check (`stationReferencesFromProject`), so look each drawing object
  ID up there and report a `duplicate-object-id` at the `-id` value, naming the station. Project check only, as the
  Raw editor sees one file.
- Case: Therion compares names with `strcmp`, but the index lowercased IDs before comparing, which the wider scope
  would turn into more false duplicates (`wall` and `Wall`). Compare IDs case-sensitively, as the Raw editor already
  does.
- Severity stays error (V5 table, `duplicate object name`, `object already exist`).
- Tests (QTest, small fixtures)
  - Raw editor: the same `-id` in two scraps of one file is reported (update `reportsDuplicateObjectIdsInSameSourceScope`,
    which expects it not to be); a point `-id` equal to a scrap name is reported; the same `-id` in two surveys of one
    `.th` is not.
  - Index: two `.th2` files input by one survey with the same `-id`, reported once at the later one; the same files
    input by two surveys, not reported; a `-namespace off` survey shares its parent's namespace; no config and two
    unrelated drawings, not reported; a drawing `-id` equal to a station name in the same survey, reported, and in
    another survey, not reported.
  - Check each fixture with Therion 6.4.0, as for V5.
- Measure on the real project used for the findings table: count new `duplicate-object-id` findings and confirm
  each against Therion, before and after.
- Documentation: `SPECIFICATION.md` (duplicate IDs within the survey namespace, across scraps and files); the
  validation paragraph of the four manuals; `docs/THERION_COMPATIBILITY.md` (lines 37 and 45 already corrected, cherry-picked from
  `docs/scraps-are-not-namespaces`; add that station names share the namespace with drawing object IDs);
  translations for any new text; WORKLOG (remove the follow-up line that commit added).

Not in this slice
- The map editor makes new IDs (line split) unique only in the open file, so it can still create a duplicate with
  another file of the survey. The validator would now report it; making the editor avoid it is a separate change.

## V6 - Code Blocks Highlighted By Their Own Rules

Goal: lines inside `code ... endcode` are coloured as TeX or MetaPost, not as Therion commands.

Done (2026-10-22, was potential). Only highlighting is affected. Therion itself reads code lines with its own
rules (it joins a final `\` and ends the block at `endcode`), and the source model already follows them (V3) and
already marks code lines as raw block content that validation does not check as commands. The highlighter is the one
consumer that does not know this, because it parses each line on its own.

- Method: no change to the shared tokenizer or source model. The highlighter already keeps a whole-document source
  snapshot per revision for its validation underlines; in the same refresh it records, for each line, whether it is
  raw code and in which language, from the enclosing `code <type>` line. This reuses the model's block rules
  (continuation, `endlayout` closing an open code block) instead of a second copy. Block state carries the language
  so that typing `code` or `endcode` recolours the lines after it.
- Languages from Therion's code types (`thlayout.h`): `metapost` and `mpost` are MetaPost, `tex-map` and `tex-atlas`
  are TeX. A missing or unknown type gets no language colouring.
- Rules, in a small core file with unit tests: TeX, a `%` not escaped by `\` starts a comment (`\%` is a literal
  percent, `\\%` a line break then a comment); MetaPost, a `%` outside a `"..."` string starts a comment. Code lines
  get no Therion keyword, option, identifier, number, or string colouring, so `#1` and Therion words in TeX or
  MetaPost are plain text. Validation underlines still apply.
- A code line whose first non-blank character is `#` is coloured as a comment in any language. Therion drops it
  before writing the code out (`thinput.cxx`; checked with Therion 6.4.0, a `#` line with an undefined macro in a
  `tex-map` block builds, while the same line without `#` stops TeX). No finding, as it is valid Therion and the
  cleaner way to comment out code.
- Tests: core QTest for the language and comment rules; a highlighter test with a layout code block in each
  language (comment coloured, Therion keyword inside code not coloured, `endcode` and the following Therion line
  coloured as Therion again).
- Documentation: `SPECIFICATION.md` (editor highlighting of code blocks); `docs/THERION_COMPATIBILITY.md` (code block
  lines follow TeX or MetaPost comment rules for display only); the four manuals where highlighting is described.

## V6b - Station Points Repeated In A Drawing

Goal: find a `point ... station -name` that repeats a station already drawn in the same `.th2` file, which Therion
accepts silently.

Done (2026-10-22), in `TherionRepeatedStationPoints.cpp`, called from the Raw editor check. On the measured project it
reports the counts below exactly (6 warnings, 11 information findings).

Therion 6.4.0 compiles all three cases below without an error or a warning (checked on small projects), so by the
severity policy none of them is an error. A station point is a morphing control point, so two of the same station in
one scrap at different places pull the drawing towards both, which is very likely a mistake. Measured on the real
project used for the findings table: 4 repeats in one scrap at different places (one is station 17 in
`ConnectionSP1`, about 4 m apart), 2 repeats in one scrap at the same place, and 11 stations shared by two scraps of
the same projection in one file, mostly neighbouring scraps where they join (for example `NorthRiftSP2` and
`NorthRiftSP3`), which is how Therion joins scraps, hence information only.

| Case | Finding | Fix |
| --- | --- | --- |
| Same station twice in one scrap, different places | warning | none, the user decides which is right |
| Same station twice in one scrap, same place | warning | `Apply Fix` removes the later point |
| Same station in two scraps of the same projection in one file | information | none |

- Same file only: drawings in other files are not compared, so `16` in two caves' drawings is never reported.
- Same station: the `-name` value compared case-sensitively after the scrap's `-station-names` transform, so
  `17` and `17@sub` are different.
- Same place: identical coordinates as written. In the measured project every same-place repeat is exact and the
  nearest different-place repeat is 4.4 drawing units apart, so no tolerance is needed yet.
- Where: all three are single-file checks in `TherionSourceValidator` (Raw editor and Validation panel), reported
  at the later point's `-name` value and naming the line of the first; the cross-scrap case once per later scrap.
- Projections: only scraps of the same projection are compared (`plan` with `plan`, `extended` with `extended`, each
  `[elevation <angle>]` with the same angle); `-projection none` cross sections are skipped, as every station
  normally appears in plan, extended and elevation drawings.
- Tests: each case in one file; transformed names (`-station-names`); the same name in two files not reported;
  different projections and cross sections not compared; the fix removes only the later point.
- Documentation: `SPECIFICATION.md` validation list, the four manuals, `docs/THERION_COMPATIBILITY.md` (Therion
  accepts repeated station points), translations for the new messages.

Answered (2026-10-22): warning for the different-place repeat (Therion compiles it, so not an error under V5); same
file only; no scrap naming convention, as names do not reliably say which scraps are neighbours.

## V6c - Map Items That Do Not Exist

Goal: report every name in a `map` that is not a scrap, map, or survey, as Therion does.

Done (2026-10-23), in `scanMapReferences` (`ProjectStructureIndex.cpp`). Found on a real project: a map listed a
scrap that its drawing did not define, Therion stopped with `object does not exist`, and project validation said
nothing.

Therion looks every map item up as an object (`thdb2d::process_map_references` in `src/therion-core/thdb2d.cxx`)
and stops with `object does not exist` when nothing of that name exists, whatever the name looks like. Therion
Studio resolved maps and scraps, and for a name it could not resolve guessed what it was meant to be from its
ending, `.m` for a map and `.s` for a scrap. Any other name was skipped, so only projects using that naming scheme
got the error.

- Every map item that is not a map, scrap, or survey (looked up in the map's namespace, `@` references included) is
  an error at the item.
- Surveys are looked up in the namespace their name belongs to (the parent survey, or the same one for
  `-namespace off`), so a survey listed in a map is not reported.
- The `.m` and `.s` endings only choose the wording (`unknown-map-reference`, `unknown-map-scrap-reference`). Every
  other missing name gets the neutral `unknown-map-item-reference` and does not count as map or scrap content, so it
  cannot also cause the mixed map and scrap warning.
- `.m`/`.s` is one naming scheme. Other schemes (for example `<survey>M...` maps and `<survey>S...` scraps) could
  choose the wording too, but none is configured yet, and the error does not depend on it.
- A map item naming a point, line, or area `-id` is reported with the same `does not exist` wording. Therion
  rejects it too, but as `invalid map object`.
- Tests: `ProjectStructureIndexQTest::reportsMapItemsThatDoNotExistWhateverTheirName`.
- Documentation: `SPECIFICATION.md` validation list, the four manuals, `docs/THERION_COMPATIBILITY.md`,
  translations for the new messages.

## V6d - Map Content Checks Therion Makes

Goal: report the other map errors Therion stops on, so a project the Validation panel passes also passes Therion's
map processing.

Group 1 (checks 1, 3, 4, 9, 10) done (2026-10-23): item and preview syntax in `TherionMapItemRules.cpp`, the rest
in `scanMapReferences`. Group 2 (checks 5 and 2) done (2026-10-23) in `ProjectMapCompositionCheck.cpp`, with
projection reading and identity shared with V6b in `TherionProjectionRules.cpp` (which also makes V6b read `-proj`).
Each message checked against Therion 6.4.0. Therion reports a loop of two or more maps as `recursive map reference`
without a line, and a map that lists itself as `map cannot contain only previews`; the validator reports both as a
recursive map reference at the item. Group 3 (checks 6, 7, 8) done (2026-10-23) in the same walk. Therion reads
a shift on the first scrap of a map but rejects it on any later scrap, even `[0 0 m] none`, and the validator
follows that. A preview that is missing gets the V6c `does not exist` finding; one naming a scrap, survey, or
drawing object gets `preview-not-map`.

Therion checks every map in the files it reads, used or not, in
`thdb2d::process_map_references` and `thdb2d::postprocess_map_references` (`src/therion-core/thdb2d.cxx`), and the
item syntax in `thmap::parse_item` and `thmap::parse_preview` (`thmap.cxx`). Each check below is a fatal error in
Therion, so by the V5 policy each is an error here. Therion stops at the first one; the validator reports all of
them.

| # | Therion error | Cause | Validator now |
| --- | --- | --- | --- |
| 1 | `not a scrap reference`, `not a map reference` | Scraps mixed with maps or surveys in one map. The first item decides which kind the map holds. | Warning, and surveys are not counted |
| 2 | `incompatible scrap projection`, `incompatible map projection` | Items of different projections in one map, or items that do not match the map's own `-projection` | Not checked |
| 3 | `invalid map object` | A point, line, or area `-id` (anything but a map, scrap, or survey) listed in a map | Reported as `does not exist` (V6c) |
| 4 | `empty map not allowed`, `map cannot contain only previews` | A map with no items, or only `preview` items | Not checked |
| 5 | `recursive map reference` | A map that contains itself, directly or through child maps | Not checked |
| 6 | `shift is not allowed for scrap` | A scrap item with `[x y units] above/below`; Therion reads the shift only with `maps-offset` on, its default | Not checked |
| 7 | `no projection for survey`, `unsupported projection for survey` | A survey item in a map without `-projection`, or with `none` | Not checked |
| 8 | `object does not exist`, `not a map reference`, `incompatible map projection` for `preview` | A `preview` item that is missing, is not a map, or has another projection | Missing previews not reported |
| 9 | `invalid map item`, `unsupported preview type` | An item line with 2 or more than 3 values, or a shift or preview type other than `above`/`below` | Not checked |
| 10 | `invalid survey reference` | `map -survey` naming something that is not a survey | Not checked |

How:

- Projections compare as Therion compares them, by type, `:index`, and the elevation angle as written
  (`thdb2d::parse_projection`), so `[elevation 315]` and `[elevation 315.0]` are different projections, as in
  Therion. A scrap without `-projection` is `plan`. A child map's projection is the projection of its content,
  worked out through its own child maps (the same walk finds recursion, check 5). A survey item takes the map's
  `-projection`. Reuse `scrapProjection` from `TherionRepeatedStationPoints.cpp` by moving it to a shared core rule
  rather than copying it.
- `ProjectStructureIndex.cpp` is already over 2000 lines, so the per-map checks go in a new focused core file (for
  example `TherionMapCompositionRules`), called from `scanMapReferences`, which keeps resolving names.
- A check is skipped when an item it needs did not resolve uniquely (missing or ambiguous), so one mistake gives one
  finding. Check 3 replaces the V6c wording for a resolved point, line, or area.
- Check 6 is reported unless the target config sets `maps-offset off`.
- Each check gets its own finding code, a translated title and message close to Therion's wording, and a Structure
  row where the Structure view already shows map problems.
- Tests: one QTest per check in `ProjectStructureIndexQTest`, plus a map Therion accepts that mixes nothing,
  previews a compatible map, and lists a survey with `-projection plan` (no findings). Each message checked against
  Therion 6.4.0 on small made-up projects.
- Documentation: `SPECIFICATION.md` (validation list, and the Structure warning row for mixed content),
  `docs/THERION_COMPATIBILITY.md`, the four manuals, cs/fr/sk translations, WORKLOG.

Approved (2026-10-23): check 1 makes mixed content an error instead of a warning, so
`tests/ProjectValidationScannerTest.cpp` now expects an error (Therion's `not a scrap reference` and `not a map
reference`). The ambiguous-reference fixture in `tests/ProjectStructureIndexTest.cpp` has two empty maps, which now
also give two empty-map errors (Therion's `empty map not allowed`).

Order: 1, 3, 4, 9 and 10 are small and go first; then 5 and 2 (they share the projection walk); then 6, 7 and 8.
One commit per group, each with its tests and documentation.

## V7 (potential) - Validate Each Drawing Against The Config That Reads It

Potential, not scheduled. When another config in the project folder reads a drawing (for example a per-survey
`thconfig`), validate the drawing against that config instead of skipping it. This needs a project index per root
config (or a multi-root index), the rules for choosing the config when several read a file, and a performance check,
because each extra root multiplies index work. Revisit after V4, once the remaining false findings are known.

## V8 (potential) - Therion Referee Test

Potential, not scheduled. A corpus test that runs Therion on small fixture projects and fails if Therion Studio
reports an error for a project Therion compiles without error. It would be a separate CTest executable (process and
Therion dependency) that skips with a clear message when Therion is not installed. This is a first step towards the
WORKLOG backlog item "Compiler-confirmed project-index comparison". Revisit after V5.

## Slices

| Slice | What | Issues |
| --- | --- | --- |
| V1a | Line splitting matches Therion's `thsplit_args`: bracketed values | #49 |
| V1b | Line splitting matches Therion's `thsplit_args`: quotes, comments, and backslashes | |
| V2a | Object command options, aliases, and arity from Therion's `thstok` tables; help linked to the book | #50, #49 |
| V2b | The same for `export` and `layout` (the `-proj` warnings) | |
| V3 | Line continuation inside quoted values | |
| V4 | Drawings outside the target config's source graph (Information severity) | |
| V4b | Document type from how Therion reads a file | |
| V5 | Severity policy | |
| V5b | Drawing object IDs unique in the survey, not the scrap | |
| V6 | Code blocks highlighted by their own rules (TeX and MetaPost) | |
| V6b | Station points repeated in a drawing | |
| V6c | Map items that do not exist, whatever their name | |
| V6d | Map content checks Therion makes (mixed content, projections, empty and recursive maps, previews) | |
| V7 (p) | Validate each drawing against the config that reads it | |
| V8 (p) | Therion referee test | |

Work in number order; (p) marks potential slices that are not scheduled. Each slice carries its own tests and
documentation, so the branch is complete after every slice.

Already done elsewhere: the `-namespace off` join references are fixed on the `NamespaceOffBug` branch.

## Open Questions

- Answered (2026-10-21): the generator reads the tables from the same pinned checkout as the book, Therion v6.4.0
  (`docs/therion-source.lock`), which is also the version the line splitting rules cite.
- Answered (2026-10-21): information findings (V4) are shown by default and are not counted as problems.
