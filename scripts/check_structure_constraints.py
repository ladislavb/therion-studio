#!/usr/bin/env python3
"""Fail when key large translation units grow beyond ratcheted thresholds."""

from __future__ import annotations

import pathlib
import re
import sys
from collections import Counter


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent


# Ratchet baseline: keep current large files from growing while extraction work continues.
LINE_LIMITS = {
    "src/app/text_editor/TextEditorTab.cpp": 1087,
    "src/app/text_editor/raw_editor/RawEditorCompletionController.cpp": 267,
    "src/app/text_editor/map_editor/MapEditorTabInspectorPanelUi.cpp": 567,
    "src/app/text_editor/map_editor/MapEditorTabWorkspaceDelegates.cpp": 415,
    "src/app/MainWindow.cpp": 2203,
}


# Stable editor-mode layout contract: required paths must exist and legacy paths
# from pre-reorganization layout must stay absent.
REQUIRED_PATHS = (
    "src/app/text_editor/TextEditorTab.cpp",
    "src/app/text_editor/TextEditorTab.h",
    "src/app/text_editor/raw_editor",
    "src/app/text_editor/block_editor",
    "src/app/text_editor/map_editor",
    "src/app/text_editor/map_editor/MapEditorTab.h",
    "src/app/text_editor/map_editor/MapEditorTabWorkspace.cpp",
    "src/app/text_editor/map_editor/MapEditorTabInspectorPanelUi.cpp",
)

FORBIDDEN_PATHS = (
    "src/app/TextEditorTab.cpp",
    "src/app/TextEditorTab.h",
    "src/app/MapEditorTab.cpp",
    "src/app/MapEditorTab.h",
    "src/app/MapEditorBackgroundLayers.cpp",
    "src/app/MapEditorInputPolicy.cpp",
    "src/app/MapEditorInputPolicy.h",
    "src/app/MapEditorSceneInternals.h",
    "src/app/MapEditorSceneItems.cpp",
    "src/app/MapEditorSceneRenderer.cpp",
    "src/app/MapEditorSceneSupport.h",
    "src/app/MapEditorTabWorkspace.cpp",
)

TEXT_EDITOR_BLOCK_EDITOR_FORBIDDEN_PATTERNS = (
    ("src/app/text_editor/TextEditorTab.h", "friend class BlockEditor", "TextEditorTab shall not expose private state to BlockEditor friends"),
)

BLOCK_EDITOR_FORBIDDEN_PATTERNS = (
    ("TextEditorTab *owner", "BlockEditor classes shall use explicit context objects instead of TextEditorTab owner pointers"),
    ("TextEditorTab* owner", "BlockEditor classes shall use explicit context objects instead of TextEditorTab owner pointers"),
    ("TextEditorTab *owner_", "BlockEditor classes shall use explicit context objects instead of TextEditorTab owner pointers"),
    ("TextEditorTab* owner_", "BlockEditor classes shall use explicit context objects instead of TextEditorTab owner pointers"),
)

MAP_EDITOR_TAB_FORBIDDEN_PATTERNS = (
    (
        "src/app/text_editor/map_editor/MapEditorTab.h",
        "friend class MapEditorInspectorBackgroundController",
        "MapEditorInspectorBackgroundController shall use an explicit context object",
    ),
    (
        "src/app/text_editor/map_editor/MapEditorTab.h",
        "friend class MapEditorSceneLifecycleController",
        "MapEditorSceneLifecycleController shall use an explicit context object",
    ),
    (
        "src/app/text_editor/map_editor/MapEditorTab.h",
        "friend class MapEditorSceneRefreshController",
        "MapEditorSceneRefreshController shall use an explicit context object",
    ),
    (
        "src/app/text_editor/map_editor/MapEditorTab.h",
        "friend class MapEditorObjectDetailsEditController",
        "MapEditorObjectDetailsEditController shall use an explicit context object",
    ),
    (
        "src/app/text_editor/map_editor/MapEditorTab.h",
        "friend class MapEditorObjectDetailsPanelController",
        "MapEditorObjectDetailsPanelController shall use an explicit context object",
    ),
    (
        "src/app/text_editor/map_editor/MapEditorTab.h",
        "friend class MapEditorInspectorObjectController",
        "MapEditorInspectorObjectController shall use an explicit context object",
    ),
    (
        "src/app/text_editor/map_editor/MapEditorTab.h",
        "friend class MapEditorInteractiveDrawController",
        "MapEditorInteractiveDrawController shall use an explicit context object",
    ),
    (
        "src/app/text_editor/map_editor/MapEditorTab.h",
        "friend class MapEditorSelectionController",
        "MapEditorSelectionController shall use an explicit context object",
    ),
    (
        "src/app/text_editor/map_editor/MapEditorTab.h",
        "friend class MapEditorViewportInputController",
        "MapEditorViewportInputController shall use an explicit context object",
    ),
    (
        "src/app/text_editor/map_editor/MapEditorTab.h",
        "friend class MapEditorCanvasEditController",
        "MapEditorCanvasEditController shall use an explicit context object",
    ),
)

MAP_EDITOR_INSPECTOR_BACKGROUND_FORBIDDEN_PATTERNS = (
    (
        "MapEditorTab *owner",
        "MapEditorInspectorBackgroundController shall use an explicit context object instead of MapEditorTab owner pointers",
    ),
    (
        "MapEditorTab* owner",
        "MapEditorInspectorBackgroundController shall use an explicit context object instead of MapEditorTab owner pointers",
    ),
    (
        "MapEditorTab *owner_",
        "MapEditorInspectorBackgroundController shall use an explicit context object instead of MapEditorTab owner pointers",
    ),
    (
        "MapEditorTab* owner_",
        "MapEditorInspectorBackgroundController shall use an explicit context object instead of MapEditorTab owner pointers",
    ),
)

MAP_EDITOR_CONTEXT_CONTROLLER_FORBIDDEN_PATTERNS = (
    (
        "MapEditorTab *owner",
        "MapEditor controllers converted to explicit contexts shall not regain MapEditorTab owner pointers",
    ),
    (
        "MapEditorTab* owner",
        "MapEditor controllers converted to explicit contexts shall not regain MapEditorTab owner pointers",
    ),
    (
        "MapEditorTab *owner_",
        "MapEditor controllers converted to explicit contexts shall not regain MapEditorTab owner pointers",
    ),
    (
        "MapEditorTab* owner_",
        "MapEditor controllers converted to explicit contexts shall not regain MapEditorTab owner pointers",
    ),
    (
        '#include "MapEditorTab.h"',
        "MapEditor controllers converted to explicit contexts shall not include MapEditorTab directly",
    ),
)

MAP_EDITOR_SOURCE_MUTATION_LOW_LEVEL_PATTERNS = (
    "->replaceTextForCommand(",
    "TherionDocumentEditor::replaceTextForCommand(",
    "TherionDocumentEditor::replaceTextForCommandWithUndo(",
    "->rewritePointCoordinates(",
    "->rewriteLineAreaVertex(",
    "->rewriteStructureEntryName(",
    "->rewriteLineOptionToggle(",
    "textEditor_->setPlainText(",
    "textEditor_->insertPlainText(",
    "textEditor->setPlainText(",
    "textEditor->insertPlainText(",
    "context_.textEditor->setPlainText(",
    "context_.textEditor->insertPlainText(",
)

# Low-level TextEditorTab source writes are intentionally confined to:
# - the shared text-editor source transaction controller outside map_editor,
#   which creates one source replacement + undo snapshot transaction, and
# - QUndoCommand implementations that store before/after snapshots internally.
MAP_EDITOR_SOURCE_MUTATION_ALLOWED_PATTERNS_BY_FILE = {}

DOM_LEGACY_PARSER_CALL_PATTERN = re.compile(
    r"TherionDocumentParser::(?:parseLine|parseTokenLines|tokenizeLine)\("
)

# Unified Source DOM migration guardrail: document parsing should flow through
# TherionSourceDocument/TherionSourceLogicalDocument snapshots. The remaining
# direct parser calls are deliberately limited to parser/core construction,
# token-level source rewrites, validation safe-fix snippets, command syntax
# option snippets, and map pending-insert snippets that do not have source text yet.
DOM_LEGACY_PARSER_ALLOWED_PATTERNS_BY_FILE = {
    "src/core/TherionDocumentParser.cpp": (
        "TherionParsedLine TherionDocumentParser::parseLine(",
        "QVector<TherionParsedLine> TherionDocumentParser::parseTokenLines(",
        "QStringList TherionDocumentParser::tokenizeLine(",
    ),
    "src/core/TherionSourceLogicalDocument.cpp": (
        "TherionDocumentParser::parseLine(command.text, command.startLineNumber)",
    ),
    "src/core/TherionSourceValidator.cpp": (
        "TherionDocumentParser::parseLine(lineText)",
    ),
    "src/core/TherionCommandSyntax.cpp": (
        "TherionDocumentParser::tokenizeLine(value)",
    ),
    "src/core/TherionDocumentEditor.cpp": (
        "TherionDocumentParser::parseLine(lineText)",
    ),
    "src/app/text_editor/map_editor/MapEditorObjectDetailsPanelController.cpp": (
        'TherionDocumentParser::parseLine(QStringLiteral("point 0 0 %1").arg(pendingFields->type.trimmed()))',
        'TherionDocumentParser::parseLine(QStringLiteral("line %1").arg(pendingFields->type.trimmed()))',
    ),
    "src/app/text_editor/map_editor/MapEditorTabSourceEditWorkflow.cpp": (
        'TherionDocumentParser::parseLine(QStringLiteral("line %1").arg(interactiveDrawState_.pendingInsertFields_.type.trimmed()))',
    ),
}

CMAKE_SOURCE_PATH_PATTERN = re.compile(r"src/[A-Za-z0-9_./+-]+\.(?:cpp|h)")
CMAKE_SOURCE_SUFFIXES = {".cpp", ".h"}
CMAKE_UNIQUE_SOURCE_SETS = (
    "THERION_CORE_SOURCES",
    "THERION_APP_SOURCES",
    "THERION_MAIN_WINDOW_SOURCES",
    "THERION_TEXT_EDITOR_SHARED_SOURCES",
    "THERION_MAP_EDITOR_SOURCES",
    "THERION_TEXT_EDITOR_TAB_SOURCES",
    "THERION_APPLICATION_BOOTSTRAP_SOURCES",
)


def count_lines(path: pathlib.Path) -> int:
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        return sum(1 for _ in handle)


def contains_text(path: pathlib.Path, text: str) -> bool:
    return text in path.read_text(encoding="utf-8", errors="replace")


def map_source_mutation_is_allowed(relative_path: str, line_text: str) -> bool:
    allowed_patterns = MAP_EDITOR_SOURCE_MUTATION_ALLOWED_PATTERNS_BY_FILE.get(relative_path, set())
    return any(pattern in line_text for pattern in allowed_patterns)


def dom_legacy_parser_call_is_allowed(relative_path: str, line_text: str) -> bool:
    allowed_patterns = DOM_LEGACY_PARSER_ALLOWED_PATTERNS_BY_FILE.get(relative_path, ())
    return any(pattern in line_text for pattern in allowed_patterns)


def repository_source_paths() -> list[str]:
    source_root = REPO_ROOT / "src"
    return sorted(
        path.relative_to(REPO_ROOT).as_posix()
        for path in source_root.rglob("*")
        if path.is_file() and path.suffix in CMAKE_SOURCE_SUFFIXES
    )


def cmake_source_paths(cmake_text: str) -> set[str]:
    return set(CMAKE_SOURCE_PATH_PATTERN.findall(cmake_text))


def cmake_set_source_paths(cmake_text: str, variable_name: str) -> list[str]:
    paths: list[str] = []
    in_set = False
    set_prefix = f"set({variable_name}"

    for line in cmake_text.splitlines():
        stripped = line.strip()
        if not in_set:
            if not stripped.startswith(set_prefix):
                continue
            in_set = True
            stripped = stripped[len(set_prefix):]

        line_before_close = stripped.split(")", 1)[0]
        paths.extend(CMAKE_SOURCE_PATH_PATTERN.findall(line_before_close))
        if ")" in stripped:
            break

    return paths


def main() -> int:
    violations: list[tuple[str, int, int]] = []
    checked: list[tuple[str, int, int]] = []
    layout_violations: list[str] = []
    dependency_violations: list[str] = []
    dom_parser_violations: list[str] = []
    source_list_violations: list[str] = []

    for relative_path, limit in LINE_LIMITS.items():
        absolute_path = REPO_ROOT / relative_path
        if not absolute_path.exists():
            violations.append((relative_path, -1, limit))
            continue

        actual = count_lines(absolute_path)
        checked.append((relative_path, actual, limit))
        if actual > limit:
            violations.append((relative_path, actual, limit))

    for relative_path in REQUIRED_PATHS:
        absolute_path = REPO_ROOT / relative_path
        if not absolute_path.exists():
            layout_violations.append(f"missing required path: {relative_path}")

    for relative_path in FORBIDDEN_PATHS:
        absolute_path = REPO_ROOT / relative_path
        if absolute_path.exists():
            layout_violations.append(f"legacy path must stay absent: {relative_path}")

    for relative_path, pattern, message in TEXT_EDITOR_BLOCK_EDITOR_FORBIDDEN_PATTERNS:
        absolute_path = REPO_ROOT / relative_path
        if absolute_path.exists() and contains_text(absolute_path, pattern):
            dependency_violations.append(f"{relative_path}: forbidden `{pattern}` ({message})")

    block_editor_root = REPO_ROOT / "src/app/text_editor/block_editor"
    for absolute_path in sorted(block_editor_root.glob("BlockEditor*.[ch]*")):
        relative_path = absolute_path.relative_to(REPO_ROOT).as_posix()
        file_text = absolute_path.read_text(encoding="utf-8", errors="replace")
        for pattern, message in BLOCK_EDITOR_FORBIDDEN_PATTERNS:
            if pattern in file_text:
                dependency_violations.append(f"{relative_path}: forbidden `{pattern}` ({message})")
        if '#include "../TextEditorTab.h"' in file_text:
            dependency_violations.append(
                f"{relative_path}: forbidden direct TextEditorTab include "
                "(BlockEditor implementation shall depend on explicit contexts)"
            )

    for relative_path, pattern, message in MAP_EDITOR_TAB_FORBIDDEN_PATTERNS:
        absolute_path = REPO_ROOT / relative_path
        if absolute_path.exists() and contains_text(absolute_path, pattern):
            dependency_violations.append(f"{relative_path}: forbidden `{pattern}` ({message})")

    map_inspector_background_root = REPO_ROOT / "src/app/text_editor/map_editor"
    for absolute_path in sorted(map_inspector_background_root.glob("MapEditorInspectorBackgroundController*.[ch]*")):
        relative_path = absolute_path.relative_to(REPO_ROOT).as_posix()
        file_text = absolute_path.read_text(encoding="utf-8", errors="replace")
        for pattern, message in MAP_EDITOR_INSPECTOR_BACKGROUND_FORBIDDEN_PATTERNS:
            if pattern in file_text:
                dependency_violations.append(f"{relative_path}: forbidden `{pattern}` ({message})")
        for pattern, message in MAP_EDITOR_CONTEXT_CONTROLLER_FORBIDDEN_PATTERNS:
            if pattern in file_text:
                dependency_violations.append(f"{relative_path}: forbidden `{pattern}` ({message})")

    for controller_name in (
        "MapEditorSceneLifecycleController",
        "MapEditorSceneRefreshController",
        "MapEditorObjectDetailsEditController",
        "MapEditorObjectDetailsPanelController",
        "MapEditorInspectorObjectController",
        "MapEditorInteractiveDrawController",
        "MapEditorSelectionController",
        "MapEditorViewportInputController",
        "MapEditorCanvasEditController",
    ):
        for absolute_path in sorted(map_inspector_background_root.glob(f"{controller_name}*.[ch]*")):
            relative_path = absolute_path.relative_to(REPO_ROOT).as_posix()
            file_text = absolute_path.read_text(encoding="utf-8", errors="replace")
            for pattern, message in MAP_EDITOR_CONTEXT_CONTROLLER_FORBIDDEN_PATTERNS:
                if pattern in file_text:
                    dependency_violations.append(f"{relative_path}: forbidden `{pattern}` ({message})")

    for absolute_path in sorted(map_inspector_background_root.rglob("*.[ch]*")):
        relative_path = absolute_path.relative_to(REPO_ROOT).as_posix()
        with absolute_path.open("r", encoding="utf-8", errors="replace") as handle:
            for line_number, line_text in enumerate(handle, start=1):
                for pattern in MAP_EDITOR_SOURCE_MUTATION_LOW_LEVEL_PATTERNS:
                    if pattern not in line_text:
                        continue
                    if map_source_mutation_is_allowed(relative_path, line_text):
                        continue
                    dependency_violations.append(
                        f"{relative_path}:{line_number}: forbidden low-level map source mutation `{pattern}` "
                        "(map editor source writes shall use applySourceTextChangeWithSnapshot "
                        "or an approved snapshot command)"
                    )

    for relative_path in repository_source_paths():
        absolute_path = REPO_ROOT / relative_path
        with absolute_path.open("r", encoding="utf-8", errors="replace") as handle:
            for line_number, line_text in enumerate(handle, start=1):
                if not DOM_LEGACY_PARSER_CALL_PATTERN.search(line_text):
                    continue
                if dom_legacy_parser_call_is_allowed(relative_path, line_text):
                    continue
                dom_parser_violations.append(
                    f"{relative_path}:{line_number}: forbidden direct TherionDocumentParser call "
                    "(use TherionSourceDocument/TherionSourceLogicalDocument snapshots or add a documented guardrail exception)"
                )

    cmake_text = (REPO_ROOT / "CMakeLists.txt").read_text(encoding="utf-8", errors="replace")
    repository_sources = repository_source_paths()
    listed_sources = cmake_source_paths(cmake_text)

    for relative_path in repository_sources:
        if relative_path not in listed_sources:
            source_list_violations.append(f"{relative_path}: missing from CMakeLists.txt")

    for relative_path in sorted(listed_sources):
        if not (REPO_ROOT / relative_path).exists():
            source_list_violations.append(f"{relative_path}: listed in CMakeLists.txt but file is missing")

    for source_set in CMAKE_UNIQUE_SOURCE_SETS:
        set_sources = cmake_set_source_paths(cmake_text, source_set)
        for relative_path, count in sorted(Counter(set_sources).items()):
            if count > 1:
                source_list_violations.append(
                    f"{source_set}: {relative_path} is listed {count} times"
                )

    if violations or layout_violations or dependency_violations or dom_parser_violations or source_list_violations:
        print("Structure constraint violations detected:")
        for relative_path, actual, limit in violations:
            if actual < 0:
                print(f"  - {relative_path}: file missing (expected max {limit} lines)")
            else:
                print(f"  - {relative_path}: {actual} lines (max {limit})")
        for violation in layout_violations:
            print(f"  - {violation}")
        for violation in dependency_violations:
            print(f"  - {violation}")
        for violation in dom_parser_violations:
            print(f"  - {violation}")
        for violation in source_list_violations:
            print(f"  - {violation}")
        print("")
        print(
            "Keep large UI translation units from growing, preserve stable editor-mode/dependency layout, "
            "keep map source mutations atomic, and keep CMake source lists aligned with src/."
        )
        return 1

    print("Structure constraints passed:")
    for relative_path, actual, limit in checked:
        print(f"  - {relative_path}: {actual}/{limit} lines")
    print("Layout constraints passed:")
    for relative_path in REQUIRED_PATHS:
        print(f"  - required present: {relative_path}")
    for relative_path in FORBIDDEN_PATHS:
        print(f"  - legacy absent: {relative_path}")
    print("Dependency constraints passed:")
    print("  - TextEditorTab has no BlockEditor friend declarations")
    print("  - BlockEditor controllers use explicit contexts instead of TextEditorTab owner pointers")
    print("  - BlockEditor implementation files do not include TextEditorTab directly")
    print("  - MapEditorInspectorBackgroundController uses an explicit context instead of MapEditorTab friendship")
    print("  - MapEditor scene lifecycle/refresh controllers use explicit contexts instead of MapEditorTab friendship")
    print("  - MapEditor object-details controllers use explicit contexts instead of MapEditorTab friendship")
    print("  - MapEditor inspector-object controller uses an explicit context instead of MapEditorTab friendship")
    print("  - MapEditor interactive-draw controller uses an explicit context instead of MapEditorTab friendship")
    print("  - MapEditor selection controller uses an explicit context instead of MapEditorTab friendship")
    print("  - MapEditor viewport-input controller uses an explicit context instead of MapEditorTab friendship")
    print("  - MapEditor canvas-edit controller uses an explicit context instead of MapEditorTab friendship")
    print("  - MapEditor source mutations are confined to the atomic helper or approved snapshot commands")
    print("  - Direct TherionDocumentParser calls are confined to documented DOM migration exceptions")
    print("Source-list hygiene passed:")
    print(f"  - {len(repository_sources)} src/**/*.cpp and src/**/*.h files are listed in CMakeLists.txt")
    print("  - CMakeLists.txt does not reference missing src/**/*.cpp or src/**/*.h files")
    for source_set in CMAKE_UNIQUE_SOURCE_SETS:
        print(f"  - {source_set} has no duplicate direct source entries")
    return 0


if __name__ == "__main__":
    sys.exit(main())
