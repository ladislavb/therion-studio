#!/usr/bin/env python3
"""Smoke-check installed Therion Studio runtime layout."""

from __future__ import annotations

import argparse
import os
import pathlib
import sys


def required_path_groups(platform_name: str) -> list[list[pathlib.Path]]:
    if platform_name == "windows":
        return [
            [pathlib.Path("bin/TherionStudio.exe")],
            [
                pathlib.Path("bin/platforms/qwindows.dll"),
                pathlib.Path("bin/platforms/qwindowsd.dll"),
            ],
            [
                pathlib.Path("bin/Qt6Core.dll"),
                pathlib.Path("bin/Qt6Cored.dll"),
            ],
            [
                pathlib.Path("bin/Qt6Gui.dll"),
                pathlib.Path("bin/Qt6Guid.dll"),
            ],
            [
                pathlib.Path("bin/Qt6Widgets.dll"),
                pathlib.Path("bin/Qt6Widgetsd.dll"),
            ],
            [
                pathlib.Path("bin/Qt6Svg.dll"),
                pathlib.Path("bin/Qt6Svgd.dll"),
            ],
            [
                pathlib.Path("bin/qml/QtQuick/Controls"),
            ],
            [
                pathlib.Path("bin/qml/QtQuick/Layouts"),
            ],
            [
                pathlib.Path("bin/qml/QtQuick/Templates"),
            ],
        ]
    if platform_name == "macos":
        return [
            [pathlib.Path("TherionStudio.app/Contents/MacOS/TherionStudio")],
            [pathlib.Path("TherionStudio.app/Contents/Info.plist")],
        ]
    if platform_name == "linux":
        return [
            [pathlib.Path("bin/TherionStudio")],
            [pathlib.Path("share/applications/therion-studio.desktop")],
            [pathlib.Path("share/icons/hicolor/256x256/apps/therion-studio.png")],
            [pathlib.Path("share/metainfo/therion-studio.metainfo.xml")],
        ]
    raise ValueError(f"Unsupported platform: {platform_name}")


def is_executable(path: pathlib.Path) -> bool:
    return os.access(path, os.X_OK)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True, choices=("windows", "macos", "linux"))
    parser.add_argument("--prefix", required=True, help="Install prefix directory to verify")
    args = parser.parse_args()

    prefix = pathlib.Path(args.prefix).resolve()
    if not prefix.exists():
        print(f"Install layout verification failed: prefix does not exist: {prefix}")
        return 1

    missing: list[list[pathlib.Path]] = []
    for alternatives in required_path_groups(args.platform):
        if not any((prefix / relative_path).exists() for relative_path in alternatives):
            missing.append(alternatives)

    if missing:
        print(f"Install layout verification failed for {args.platform} at {prefix}:")
        for alternatives in missing:
            if len(alternatives) == 1:
                print(f"  - missing: {alternatives[0].as_posix()}")
            else:
                alternatives_label = " or ".join(path.as_posix() for path in alternatives)
                print(f"  - missing one of: {alternatives_label}")
        return 1

    executable_relative = {
        "windows": pathlib.Path("bin/TherionStudio.exe"),
        "macos": pathlib.Path("TherionStudio.app/Contents/MacOS/TherionStudio"),
        "linux": pathlib.Path("bin/TherionStudio"),
    }[args.platform]
    executable_path = prefix / executable_relative
    if not is_executable(executable_path):
        print(
            f"Install layout verification failed for {args.platform}: "
            f"{executable_relative.as_posix()} is not executable"
        )
        return 1

    print(f"Install layout verification passed for {args.platform}: {prefix}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
