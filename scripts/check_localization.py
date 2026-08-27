#!/usr/bin/env python3
"""Validate shipped and staged Therion Studio localization assets."""

from __future__ import annotations

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SHIPPED_LANGUAGES = ("cs", "fr", "sk")
STAGED_LANGUAGES = ("de", "es", "it", "pt")
SOURCE_LANGUAGE = "en"
PLACEHOLDER_RE = re.compile(r"%\d+")


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def catalog_path(language: str) -> Path:
    return REPO_ROOT / "translations" / f"therion_studio_{language}.ts"


def manual_path(language: str) -> Path:
    return REPO_ROOT / "docs" / f"USER_MANUAL.{language}.md"


def message_text(element: ET.Element | None) -> str:
    if element is None:
        return ""
    return "".join(element.itertext())


def placeholders(text: str) -> list[str]:
    return sorted(set(PLACEHOLDER_RE.findall(text)))


def check_catalog(language: str, *, require_finished: bool) -> list[str]:
    errors: list[str] = []
    path = catalog_path(language)
    if not path.exists():
        return [f"missing catalog: {path.relative_to(REPO_ROOT)}"]

    try:
        root = ET.parse(path).getroot()
    except ET.ParseError as exc:
        return [f"{path.relative_to(REPO_ROOT)} is not valid XML: {exc}"]

    catalog_language = root.get("language") or ""
    if catalog_language != language and not catalog_language.startswith(f"{language}_"):
        errors.append(
            f"{path.relative_to(REPO_ROOT)} has language={catalog_language!r}, expected {language!r}"
        )
    if root.get("sourcelanguage") not in (None, SOURCE_LANGUAGE):
        errors.append(
            f"{path.relative_to(REPO_ROOT)} has sourcelanguage={root.get('sourcelanguage')!r}, expected {SOURCE_LANGUAGE!r}"
        )

    message_count = 0
    unfinished_count = 0
    empty_finished_count = 0
    for context in root.findall("context"):
        context_name = context.findtext("name") or ""
        for message in context.findall("message"):
            message_count += 1
            source = message_text(message.find("source"))
            translation_element = message.find("translation")
            translation = message_text(translation_element)
            unfinished = translation_element is None or translation_element.get("type") == "unfinished"
            if unfinished:
                unfinished_count += 1
            elif not translation.strip():
                empty_finished_count += 1

            if translation.strip() and placeholders(source) != placeholders(translation):
                errors.append(
                    f"{path.relative_to(REPO_ROOT)} [{context_name}] placeholder mismatch for {source!r}: "
                    f"{placeholders(source)} != {placeholders(translation)}"
                )

    if message_count == 0:
        errors.append(f"{path.relative_to(REPO_ROOT)} has no messages")
    if require_finished and unfinished_count:
        errors.append(f"{path.relative_to(REPO_ROOT)} has {unfinished_count} unfinished translation(s)")
    if require_finished and empty_finished_count:
        errors.append(f"{path.relative_to(REPO_ROOT)} has {empty_finished_count} empty finished translation(s)")

    return errors


def check_manuals(require_staged_manuals: bool) -> list[str]:
    errors: list[str] = []
    if not (REPO_ROOT / "docs" / "USER_MANUAL.md").exists():
        errors.append("missing source manual: docs/USER_MANUAL.md")
    for language in SHIPPED_LANGUAGES:
        if not manual_path(language).exists():
            errors.append(f"missing shipped manual: {manual_path(language).relative_to(REPO_ROOT)}")
    if require_staged_manuals:
        for language in STAGED_LANGUAGES:
            if not manual_path(language).exists():
                errors.append(f"missing staged manual: {manual_path(language).relative_to(REPO_ROOT)}")
    return errors


def check_cmake_lists() -> list[str]:
    errors: list[str] = []
    cmake = read_text(REPO_ROOT / "CMakeLists.txt")
    for language in SHIPPED_LANGUAGES:
        needle = f"translations/therion_studio_{language}.ts"
        if needle not in cmake:
            errors.append(f"CMakeLists.txt does not list shipped catalog {needle}")
    for language in STAGED_LANGUAGES:
        needle = f"translations/therion_studio_{language}.ts"
        if needle not in cmake:
            errors.append(f"CMakeLists.txt does not list staged catalog {needle}")
    return errors


def check_staged_not_user_selectable() -> list[str]:
    errors: list[str] = []
    settings_dialog = read_text(REPO_ROOT / "src/app/MainWindowSettingsDialog.cpp")
    startup = read_text(REPO_ROOT / "src/platform/ApplicationStartupBootstrap.cpp")
    plist = read_text(REPO_ROOT / "cmake/macos/Info.plist.in")
    for language in STAGED_LANGUAGES:
        literal = f'QStringLiteral("{language}")'
        if literal in settings_dialog:
            errors.append(f"staged language {language} is user-selectable in MainWindowSettingsDialog.cpp")
        if literal in startup:
            errors.append(f"staged language {language} is enabled in ApplicationStartupBootstrap.cpp")
        if f"<string>{language}</string>" in plist:
            errors.append(f"staged language {language} is advertised in Info.plist.in")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--strict-staged",
        action="store_true",
        help="require staged catalogs and manuals to be fully translated",
    )
    args = parser.parse_args()

    errors: list[str] = []
    for language in SHIPPED_LANGUAGES:
        errors.extend(check_catalog(language, require_finished=True))
    for language in STAGED_LANGUAGES:
        errors.extend(check_catalog(language, require_finished=args.strict_staged))
    errors.extend(check_manuals(require_staged_manuals=args.strict_staged))
    errors.extend(check_cmake_lists())
    if not args.strict_staged:
        errors.extend(check_staged_not_user_selectable())

    if errors:
        print("Localization check failed:")
        for error in errors:
            print(f"  - {error}")
        return 1

    mode = "strict staged" if args.strict_staged else "staged allowed"
    print(f"Localization check passed ({mode}).")
    print(f"  shipped: {', '.join(SHIPPED_LANGUAGES)}")
    print(f"  staged: {', '.join(STAGED_LANGUAGES)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
