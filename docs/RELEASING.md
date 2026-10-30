# Releasing Therion Studio

This document describes the current release workflow for Therion Studio.

## 1. Scope and Current Channels

Current release channels:

- Windows: NSIS installer artifact from GitHub Actions
- Linux: `.deb` and AppImage artifacts from GitHub Actions
- macOS: Homebrew tap formula in `ladislavb/homebrew-therion-studio`

Not finalized yet:

- Linux APT repository publishing is not yet implemented

## 2. Versioning

Therion Studio uses CalVer tags:

```text
vYYYY.M.PATCH
```

Example:

```text
v2026.5.1
```

## 3. Pre-Release Checklist

For the next release notes, include opt-in local checkpoints and restore,
private GitHub repository setup, one-click project uploads with **GitHub OK**
status, and the optional exit prompt. Mention that remote backups are
unencrypted and one-way, not multi-computer synchronization. The English,
Czech and Slovak user manuals describe these workflows.

Before tagging:

1. Ensure `main` is up to date and clean.
2. Confirm user-facing docs are current:
   - `README.md`
   - `docs/USER_MANUAL.md`
   - packaging docs if changed
3. Confirm CI workflow files are on `main` and valid.
4. Decide final tag value (for example `v2026.5.1`).

## 4. Create and Push the Release Tag

```sh
git checkout main
git pull --ff-only origin main
git tag -a v2026.5.1 -m "Therion Studio v2026.5.1"
git push origin v2026.5.1
```

Optional safety check:

```sh
git tag --list 'v2026.5.1'
```

## 5. Run CI Validation Workflows

Trigger these GitHub Actions workflows manually (`workflow_dispatch`):

1. `Build Linux` with `run_ui_smoke_tests=true`
2. `Build macOS` with `run_ui_smoke_tests=true`
3. `Build Windows` with `run_ui_smoke_tests=true`
4. `Linux Packages` with `source_ref` set to the target tag/commit

All should pass before publishing release assets.

## 6. Build Windows Installer Artifact

Run workflow:

- `Windows Installer`

Inputs:

- `source_ref = v2026.5.1`
- `qt_version = 6.8.3` (or current supported Qt version)
- `build_type = Release`

Expected artifact pattern:

```text
TherionStudio-<package_label>-Windows-x86_64.exe
TherionStudio-Windows-installer-manifest.json
```

Release-tagged builds use the tag version as the Windows installer artifact label. Snapshot builds
from branches or raw SHAs use `dev-<short_sha>` as the artifact label while keeping the generated
CalVer application/installer version in metadata.

## 6b. Build Linux Package Artifacts

Run workflow:

- `Linux Packages`

Inputs:

- `source_ref = v2026.5.1`
- `build_type = Release`

Expected artifact patterns:

```text
therion-studio-<package_label>-ubuntu-26.04-amd64.deb
therion-studio-<package_label>-ubuntu-26.04-arm64.deb
TherionStudio-<package_label>-Linux-x86_64.AppImage
TherionStudio-<package_label>-Linux-aarch64.AppImage
TherionStudio-Linux-x86_64-artifacts-manifest.json
TherionStudio-Linux-aarch64-artifacts-manifest.json
```

Release-tagged builds use the tag version as the Linux artifact label and Debian package version.
Snapshot builds from branches or raw SHAs use `dev-<short_sha>` as the artifact label and
`<calver>+git<yyyymmdd>.g<short_sha>` as the Debian package `Version` field.

The `.deb` artifact name includes `ubuntu-26.04` because the package is built and validated only
against Ubuntu 26.04. The Ubuntu-built `.deb` is not the Debian compatibility path and is not a
general Ubuntu-family package because Qt package dependency metadata can differ between distro
releases. The workflow includes follow-up Ubuntu 26.04 and Debian 13 container smoke jobs for both
Linux architectures; Ubuntu 26.04 installs and launches the generated `.deb`, while both targets
launch the generated AppImage.

For a release, treat Ubuntu 26.04 as a tested `.deb` target only when the Ubuntu `.deb` smoke job
passes in the release workflow run. Treat the AppImage as tested on Debian 13 and Ubuntu 26.04 only
when both AppImage smoke checks pass.

The AppImage is generated in a `debian:13` container from Qt's CMake deployment script using
Debian distro Qt packages, plus the `scripts/linux-packages/prepare_appimage_appdir.sh` AppRun wrapper and
explicit Qt plugin staging plus `ldd`-resolved `libQt6*.so*` staging into `AppDir/usr/lib`. The
workflow launch-tests the AppDir before packaging, then uses pinned architecture-specific
`appimagetool` and pinned architecture-specific AppImage runtime inputs. Do not replace this with
mutable `continuous` downloads.

## 7. Publish GitHub Release

1. Create GitHub Release for tag `v2026.5.1`.
2. Attach the Windows installer artifact from the workflow run.
3. Attach Linux artifacts from `Linux Packages`:
   - `therion-studio-<package_label>-ubuntu-26.04-amd64.deb`
   - `therion-studio-<package_label>-ubuntu-26.04-arm64.deb`
   - `TherionStudio-<package_label>-Linux-x86_64.AppImage`
   - `TherionStudio-<package_label>-Linux-aarch64.AppImage`
4. Keep generated manifest JSON files with release records:
   - `TherionStudio-Windows-installer-manifest.json`
   - `TherionStudio-Linux-x86_64-artifacts-manifest.json`
   - `TherionStudio-Linux-aarch64-artifacts-manifest.json`
5. Add release notes under `docs/releases/` (key features/fixes/known limitations).

## 8. Update Homebrew Tap Formula (macOS)

Homebrew formula is maintained in:

- `https://github.com/ladislavb/homebrew-therion-studio`

Update `therion-studio.rb`:

1. Set `url` to the new tag tarball.
2. Compute and set `sha256` from the same tarball.
3. Keep platform/dependency policy aligned with this repo docs.

Tarball and hash example:

```sh
VERSION=2026.5.1
TAG=v$VERSION
URL="https://github.com/ladislavb/therion-studio/archive/refs/tags/${TAG}.tar.gz"

curl -fL "$URL" -o "/tmp/therion-studio-${TAG}.tar.gz"
shasum -a 256 "/tmp/therion-studio-${TAG}.tar.gz"
```

Validate formula in tap repo:

```sh
brew style ./therion-studio.rb
brew audit --strict --online ./therion-studio.rb
brew install --build-from-source ./therion-studio.rb
```

Then commit and push in the tap repository.

## 9. Post-Release Verification

1. Confirm GitHub Release is visible with attached Windows installer.
2. Confirm Homebrew tap installs the released version on macOS Sequoia+.
3. Sanity-check app launch and basic open/save workflow.
4. Update `WORKLOG.md` with completed release tasks.

## 10. Known Gaps

- Code signing/notarization strategy is not yet fully documented/exercised end-to-end.
