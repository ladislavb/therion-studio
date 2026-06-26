# Windows Installer Packaging

Therion Studio uses CMake install rules plus CPack/NSIS for Windows installer generation.
The installer bundles Therion Studio and the Qt runtime deployed by Qt's CMake deployment API.
It does not bundle the external `therion.exe` compiler; users install/configure Therion separately.

## Local Release Build

Prerequisites:

- Windows 2022/11 development machine or GitHub Actions `windows-2022` runner
- Visual Studio 2022 Build Tools with the MSVC x64 toolchain
- CMake 3.21 or newer
- Ninja
- Qt 6.4 or newer for `msvc2022_64`, including `qtsvg` and `qttools`
- NSIS

Example PowerShell build:

```powershell
$QtRoot = "C:/Qt/6.8.3/msvc2022_64"
$Version = "2026.5.0"

cmake -S . -B build-win -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=$QtRoot `
  -DTHERION_STUDIO_VERSION=$Version

cmake --build build-win --target TherionStudio --parallel
cpack --config build-win/CPackConfig.cmake -C Release --verbose
```

Expected artifact:

```text
build-win/TherionStudio-<package_label>-Windows-x86_64.exe
```

## What CPack Does

The Windows CMake configuration:

- installs `TherionStudio.exe` under `bin/`
- links `TherionStudio.exe` as a Windows GUI application so launching it does not open a console window
- runs Qt deployment during install packaging so required Qt DLLs, plugins, and QML imports are included beside the installed executable, including `bin/platforms/qwindows.dll` and the `bin/qml/QtQuick` modules used by the 3D viewer inspector
- uses NSIS to create the installer
- includes the project `GPL-3.0-or-later` license from the root `LICENSE` file in CPack metadata
- assigns the bundled `resources/app/TherionStudio.ico` to the installer and installed app shortcut
- creates a Start Menu entry and desktop link for `Therion Studio`

## GitHub Actions Packaging

The repository includes `.github/workflows/windows-installer.yml` as a scheduled/manual packaging
workflow. Scheduled runs package the default branch with Qt 6.8.3 as `Release`. Manual runs accept
`source_ref`, `qt_version`, and `build_type` inputs, so the source can be built from `main`, a
release tag such as `v2026.5.0`, or a specific commit SHA.

If the checked-out commit is exactly tagged with a CalVer release tag such as `v2026.5.0`, the
workflow passes `-DTHERION_STUDIO_VERSION=2026.5.0` and
`-DTHERION_STUDIO_PACKAGE_LABEL=2026.5.0` to CMake. Branch and commit builds derive a development
version from the current UTC year/month, such as `2026.5.0`, and use a human-readable snapshot
package label such as `dev-a1b2c3d`, matching the Linux artifact convention.

Recommended release flow:

1. Tag the source, for example `v2026.5.0`.
2. Run the `Windows Installer` workflow manually from GitHub Actions.
3. Set `source_ref` to `main` for a current development installer or to the release tag for a tagged release build.
4. Download the uploaded installer artifact.
5. Attach the installer to the GitHub Release for the tag.

The workflow installs Qt directly with `aqtinstall`, installs NSIS/Ninja with Chocolatey,
configures a Release Ninja build, runs a staged `cmake --install`, verifies expected runtime layout
with `scripts/verify_install_layout.py` (including `bin/TherionStudio.exe` and
`bin/platforms/qwindows.dll`, plus Qt Quick Controls QML import directories), runs CPack, verifies the produced installer filename against the
resolved `THERION_STUDIO_PACKAGE_LABEL`, generates an installer manifest with SHA256 via
`scripts/verify_windows_installer_artifact.py`, and uploads both the `.exe` and manifest artifact.
The upload step uses a Node 24-compatible `actions/upload-artifact` version to avoid GitHub
Actions Node 20 deprecation warnings.

## Future Hardening

Before public distribution, add:

- Windows Authenticode signing for `TherionStudio.exe` and the NSIS installer
- release-only version stamping from tags
- installer smoke test on a clean Windows VM
- optional checksum generation for published artifacts
