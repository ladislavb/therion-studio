# Building Therion Studio

This document describes the current build and packaging workflow.

## Common Requirements

- CMake 3.21 or newer
- C++20 compiler
- Qt 6.4 or newer with `Widgets`, `Svg`, and `Concurrent`

The bundled command catalog, syntax palette, and UI icons are compiled into Qt resources.
Localization maintenance is documented in `docs/LOCALIZATION.md`.

## Optional LOX Corpus Tests

Mandatory `unit` tests use only fixtures committed under `tests/fixtures/`. Broader
real-project `.lox` coverage is opt-in because `sample_data/` is intentionally not
part of a clean checkout.

Configure and run the optional corpus test with:

```sh
cmake -S . -B build-corpus \
  -DCMAKE_BUILD_TYPE=Release \
  -DTHERION_STUDIO_ENABLE_LOX_CORPUS_TESTS=ON
cmake --build build-corpus --target therion_corpus_tests
ctest --test-dir build-corpus -L corpus --output-on-failure
```

The default corpus root is `<repository>/sample_data`. Override it when the corpus
is stored elsewhere:

```sh
cmake -S . -B build-corpus \
  -DTHERION_STUDIO_ENABLE_LOX_CORPUS_TESTS=ON \
  -DTHERION_STUDIO_LOX_CORPUS_ROOT=/path/to/sample_data
```

Each known fixture is an independent QTest data row. Available files are verified;
missing files report an intentional row-level skip. Enabling or partially populating
the optional corpus does not change the mandatory `unit` label.

## Map Object Style Gallery

The map object style gallery is a developer review artifact for bundled and user override styles.
It renders all command-catalog-supported `point`, `line`, and `area` type/subtype combinations,
including combinations with no dedicated style file that fall back to type or global defaults.

Configure the build tree first if it does not exist yet:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

Generate the gallery:

```sh
cmake --build build --target map_object_style_gallery
```

The default output is:

```text
build/style_gallery/index.html
```

Open `index.html` in a browser to review the generated PNG previews. The gallery labels each
entry by style coverage, for example `exact subtype style`, `type style`,
`inherited type style`, or `global default`.

To write the gallery to a custom directory, run the helper executable directly after building it:

```sh
cmake --build build --target MapObjectStyleGallery
QT_QPA_PLATFORM=offscreen ./build/MapObjectStyleGallery --output /tmp/therion-style-gallery
```

## macOS

macOS packaging is intended for Homebrew formula installation. The app bundle does not bundle Qt frameworks; the formula shall declare Qt dependencies instead.
CI targets the last two major macOS versions available as GitHub-hosted runners.

Configure and build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target TherionStudio
```

Install to a staging prefix:

```sh
cmake --install build --prefix /tmp/therion-studio-stage
```

The install step places `TherionStudio.app` in the prefix. Do not run `macdeployqt` for Homebrew formula builds.

Formula dependencies should include:

- `qt`
- `qttranslations`

The maintained source-build Homebrew formula is published in the dedicated tap repository:

- `https://github.com/ladislavb/homebrew-therion-studio`

Use the tap formula as the source of truth for current package metadata and dependency declarations.

The external Therion command-line executable is not bundled. Users configure it in the application runner settings, or install it separately through Homebrew.

## Windows

Windows packaging is intended to produce an installer containing:

- Therion Studio
- Qt runtime libraries and plugins required by the app

The installer shall not bundle the external `therion.exe` command-line executable.
The maintained Windows packaging notes and GitHub Actions workflow details are in
`packaging/windows/README.md`.

Recommended MSVC release build:

```powershell
cmake -S . -B build-win -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64 `
  -DTHERION_STUDIO_VERSION=2026.5.0

cmake --build build-win --target TherionStudio
cpack --config build-win\CPackConfig.cmake
```

Use a matching toolchain/Qt pair on Windows. If `CMAKE_PREFIX_PATH` points to
`...\msvc2022_64`, configure with MSVC (`cl`). Mixing MinGW (`g++`) with MSVC Qt
libraries causes link failures with unresolved `__imp__` Qt symbols.
The Qt installation must include Qt Declarative and Qt Quick Controls 2 modules
so CMake can resolve the `Qt6Quick`, `Qt6Qml`, `Qt6QuickWidgets`, and
`Qt6QuickControls2` packages used by the 3D viewer.

CPack runs the CMake install step internally. The install step runs Qt deployment on Windows,
copying the required Qt runtime next to the installed `bin/TherionStudio.exe`, including
`Qt6*.dll` runtime libraries and the Qt platform plugin at `bin/platforms/qwindows.dll`
(or Debug-build variants such as `Qt6Cored.dll` and `qwindowsd.dll` in Debug CI jobs).
CMake runs Qt's deployment helper and an explicit `windeployqt` install step so CI
install-smoke checks verify the same runtime layout that the NSIS package consumes. The
explicit `windeployqt` step passes `--qmldir resources/qml` so Qt Quick imports used by
the 3D viewer, including `QtQuick.Controls`, `QtQuick.Layouts`, and `QtQuick.Templates`,
are copied under the install-prefix `qml` directory used by the deployed Qt runtime. CPack is
configured to use NSIS for the Windows installer and emits
`TherionStudio-<package_label>-Windows-x86_64.exe` in the build directory.
The installer metadata uses the root `LICENSE` file for the project license.
The installed executable is linked as a Windows GUI application and shall not open a console
window when launched from Explorer, Start Menu, or the desktop shortcut.
Set `THERION_STUDIO_ENABLE_LOG=1` before launching the application to capture Qt and QML
diagnostics to the per-user application data log file `therion-studio.log`; set
`THERION_STUDIO_LOG_FILE` to override that path, for example:

```powershell
$env:THERION_STUDIO_ENABLE_LOG="1"
$env:THERION_STUDIO_LOG_FILE="C:\temp\therion-studio.log"
$env:QML_IMPORT_TRACE="1"
$env:QT_DEBUG_PLUGINS="1"
.\TherionStudio.exe
```

Required Windows packaging tools:

- Qt for MSVC
- Ninja or another CMake generator
- NSIS for installer generation

The GitHub Actions workflow `.github/workflows/windows-installer.yml` builds the same installer on
`windows-2022` and uploads it as an artifact. It runs manually and on a daily schedule. Scheduled
runs package the default branch with Qt 6.8.3 as `Release`; manual runs can select a branch, tag,
commit SHA, Qt version, and CMake build type. When the checked-out commit is exactly tagged with a
CalVer release tag such as `v2026.5.0`, the workflow derives
`THERION_STUDIO_VERSION=2026.5.0` and passes it to CMake so CPack uses the tag version without
editing `CMakeLists.txt`. Prerelease package builds may pass a version such as
`2026.5.0-beta.1`; CMake derives the numeric project and bundle version from the leading
numeric part while preserving the full application version string and package label. Branch and
SHA builds derive a CalVer development application version such as `2026.5.0` and a human-readable
package label such as `dev-a1b2c3d`, matching the Linux snapshot artifact convention. The workflow
also validates that the produced installer file name matches the resolved package label and emits a
manifest JSON with installer SHA256.

CI build workflows also run staged install-layout smoke checks via
`scripts/verify_install_layout.py`:

- Linux: verify `bin/TherionStudio`
- macOS: verify `TherionStudio.app/Contents/MacOS/TherionStudio`
- Windows: verify `bin/TherionStudio.exe` plus deployed Qt runtime layout, accepting release and Debug Qt runtime names

## Linux

Linux packaging currently targets two distributable artifact families:

- `.deb` packages for Ubuntu 26.04 on `amd64` and `arm64`
- AppImages as the portable Linux channel on `x86_64` and `aarch64`

The `.github/workflows/linux-packages.yml` workflow runs manually and on a daily schedule. Scheduled
runs package the default branch as `Release`; manual runs can select a branch, tag, commit SHA, and
CMake build type. The workflow builds the `.deb` package inside an `ubuntu:26.04` container and
builds the AppImage inside a `debian:13` container for each supported Linux architecture. `x86_64`
builds run on `ubuntu-24.04`; `aarch64` builds run on `ubuntu-24.04-arm`. It validates install
layout and artifact naming, then uploads:

- `therion-studio-<package_label>-ubuntu-26.04-amd64.deb`
- `therion-studio-<package_label>-ubuntu-26.04-arm64.deb`
- `TherionStudio-<package_label>-Linux-x86_64.AppImage`
- `TherionStudio-<package_label>-Linux-aarch64.AppImage`
- `TherionStudio-Linux-x86_64-artifacts-manifest.json` (SHA256 + build metadata)
- `TherionStudio-Linux-aarch64-artifacts-manifest.json` (SHA256 + build metadata)

The `.deb` artifact intentionally includes `ubuntu-26.04` in the file name because Qt package
dependency metadata is distribution-release-specific. Do not treat that `.deb` as the compatibility
path for Debian, older Ubuntu releases, or newer Ubuntu releases unless a matching package is built
and smoke-tested for that baseline.
Release-tagged builds use the CalVer tag as both the artifact label and Debian package version.
Snapshot builds use `dev-<short_sha>` as the artifact label and a Debian package version in the
form `<calver>+git<yyyymmdd>.g<short_sha>`, so artifact names stay readable while `apt` sees a
valid, monotonic package version.

The AppImage is built from a separate CMake tree inside a `debian:13` container with
`THERION_ENABLE_QT_LINUX_DEPLOY_INSTALL=ON` and Debian's distro Qt packages. The AppImage path
enables Qt's generated Linux deployment script during install so the AppDir receives Qt plugins and
deploy metadata. Because Debian's Qt shared libraries live in system library directories that Qt's
Linux deploy helper may exclude by default, the workflow also copies selected Qt plugin groups from
the distro Qt plugin directory, copies the `ldd`-resolved `libQt6*.so*` runtime families into
`AppDir/usr/lib`, stages selected non-glibc runtime libraries required by Debian's Qt stack such as
`libproxy.so*` and its dynamically loaded `libpxbackend-*.so*` backend, writes an `AppRun` wrapper
that sets `LD_LIBRARY_PATH` and `QT_PLUGIN_PATH`, and performs an offscreen AppDir launch sanity check before packaging through
`scripts/linux-packages/prepare_appimage_appdir.sh`. The workflow then packages the AppDir with pinned
architecture-specific `appimagetool` 1.9.1 and pinned architecture-specific `type2-runtime`
20251108 runtime inputs, all SHA256-verified before execution/use. The manifest records the
`.deb` and AppImage Qt package sources, versions, package sets, architecture labels, `appimagetool`,
and runtime provenance.

The same workflow also runs follow-up smoke jobs in `ubuntu:26.04` and `debian:13` containers.
The Ubuntu targets install the produced architecture-specific `.deb` packages, verify installed
paths, and perform offscreen `.deb` launch sanity checks. Both Ubuntu 26.04 and Debian 13 launch
the generated architecture-specific AppImages. The `.deb` package is the Ubuntu 26.04 coverage path
only. The AppImage should be documented as tested on Debian 13 and Ubuntu 26.04 only when both smoke
jobs pass for that architecture.

To reproduce the Ubuntu 26.04 `.deb` build locally, run the same container script used by the
workflow. This requires Docker and network access for `apt-get`:

```sh
scripts/linux-packages/build_deb_package_docker.sh
```

The expected local output is `build-linux-packages/therion-studio-dev-<short_sha>-ubuntu-26.04-amd64.deb`.

To build the Ubuntu 26.04 ARM64 `.deb` locally, set the Linux architecture:

```sh
THERION_STUDIO_LINUX_ARCH=arm64 scripts/linux-packages/build_deb_package_docker.sh
```

The ARM64 output is `build-linux-packages/therion-studio-dev-<short_sha>-ubuntu-26.04-arm64.deb`.

For a release-like local build, override the version fields:

```sh
THERION_STUDIO_VERSION=2026.5.1 \
THERION_STUDIO_PACKAGE_LABEL=2026.5.1 \
THERION_STUDIO_DEBIAN_VERSION=2026.5.1 \
scripts/linux-packages/build_deb_package_docker.sh
```

To reproduce the Debian 13 AppImage build locally, run:

```sh
scripts/linux-packages/build_appimage_package_docker.sh
```

The expected local output is `build-linux-appimage/TherionStudio-dev-<short_sha>-Linux-x86_64.AppImage`.

To build the ARM64 AppImage locally, run:

```sh
THERION_STUDIO_LINUX_ARCH=arm64 scripts/linux-packages/build_appimage_package_docker.sh
```

The ARM64 output is `build-linux-appimage/TherionStudio-dev-<short_sha>-Linux-aarch64.AppImage`.

After building local artifacts, smoke-test installation and launch behavior in Docker:

```sh
scripts/linux-packages/smoke_deb_package_docker.sh
scripts/linux-packages/smoke_appimage_package_docker.sh ubuntu:26.04 build-linux-appimage
scripts/linux-packages/smoke_appimage_package_docker.sh debian:13 build-linux-appimage
```

For ARM64 local smoke tests, use the same commands with `THERION_STUDIO_LINUX_ARCH=arm64`.
The smoke scripts accept either a direct local build directory such as `build-linux-packages` or
`build-linux-appimage`, or an `actions/download-artifact` extraction root containing those build
subdirectories.

To run the full local Linux package pass in one command, including both builds and all smoke tests,
run:

```sh
scripts/linux-packages/build_and_smoke_packages_docker.sh
```

For the full ARM64 pass, run:

```sh
THERION_STUDIO_LINUX_ARCH=arm64 scripts/linux-packages/build_and_smoke_packages_docker.sh
```

The `.deb` smoke test installs the package in `ubuntu:26.04`, verifies installed desktop/icon/metainfo
paths, and launches `TherionStudio` with the offscreen Qt platform. The AppImage smoke tests launch
the generated AppImage with `APPIMAGE_EXTRACT_AND_RUN=1` in the requested container image.

Do not use mutable `linuxdeployqt` `continuous` AppImage downloads or unmaintained Qt
deployment plugins for production release artifact generation.

Regular `build-linux.yml` CI intentionally validates the fast source-build path only on the hosted
Ubuntu 24.04 runner. Ubuntu 26.04 and Debian 13 coverage belongs to the Linux packaging workflow,
which builds and smoke-tests generated release artifacts in containers.

On supported Ubuntu and Debian-family systems, install the development dependencies with:

```sh
sudo apt-get install \
  libsqlite3-dev \
  qt6-base-dev \
  qt6-base-dev-tools \
  qt6-declarative-dev \
  qml6-module-qtqml-workerscript \
  qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts \
  qml6-module-qtquick-templates \
  qml6-module-qtquick-window \
  qt6-svg-dev \
  qt6-tools-dev \
  qt6-tools-dev-tools
```

Then use the same CMake configure/build flow as macOS.

Linux packaging details are documented in `packaging/linux/README.md`.
