#!/usr/bin/env bash
set -euo pipefail

required_env=(
    APPIMAGETOOL_SHA256
    APPIMAGETOOL_URL
    APPIMAGE_QT_PACKAGES
    APPIMAGE_RUNTIME_SHA256
    APPIMAGE_RUNTIME_URL
    BUILD_TYPE
    THERION_STUDIO_APPIMAGE_ARCHITECTURE_LABEL
    THERION_STUDIO_DEBIAN_VERSION
    THERION_STUDIO_PACKAGE_LABEL
    THERION_STUDIO_VERSION
)

for name in "${required_env[@]}"; do
    if [[ -z "${!name:-}" ]]; then
        echo "Missing required environment variable: $name" >&2
        exit 2
    fi
done

IFS=, read -r -a qt_packages <<< "$APPIMAGE_QT_PACKAGES"

case "$THERION_STUDIO_APPIMAGE_ARCHITECTURE_LABEL" in
    x86_64 | aarch64) ;;
    *)
        echo "Unsupported AppImage architecture: $THERION_STUDIO_APPIMAGE_ARCHITECTURE_LABEL" >&2
        exit 2
        ;;
esac

export DEBIAN_FRONTEND=noninteractive
export LANG=C.UTF-8
export LC_ALL=C.UTF-8

apt-get update
apt-get install -y \
    ca-certificates \
    cmake \
    dpkg-dev \
    file \
    g++ \
    libgl1-mesa-dev \
    libsqlite3-dev \
    libxkbcommon-dev \
    libxkbcommon-x11-0 \
    libxcb-cursor0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-render-util0 \
    libxcb-xinerama0 \
    ninja-build \
    patchelf \
    python3 \
    wget \
    "${qt_packages[@]}"

rm -rf build-linux-appimage
mkdir -p build-linux-appimage
dpkg-query -W qt6-base-dev | cut -f2 > build-linux-appimage/appimage-qt-version.txt

cmake -S . -B build-linux-appimage \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DTHERION_STUDIO_VERSION="$THERION_STUDIO_VERSION" \
    -DTHERION_STUDIO_PACKAGE_LABEL="$THERION_STUDIO_PACKAGE_LABEL" \
    -DTHERION_STUDIO_DEBIAN_VERSION="$THERION_STUDIO_DEBIAN_VERSION" \
    -DTHERION_ENABLE_STRICT_WARNINGS=ON \
    -DTHERION_ENABLE_QT_LINUX_DEPLOY_INSTALL=ON
cmake --build build-linux-appimage --target TherionStudio --parallel

rm -rf build-linux-appimage/AppDir
DESTDIR="$PWD/build-linux-appimage/AppDir" cmake --install build-linux-appimage --prefix /usr

appdir="$PWD/build-linux-appimage/AppDir"
usr_dir="$appdir/usr"
desktop_file="$usr_dir/share/applications/therion-studio.desktop"
icon_file="$usr_dir/share/icons/hicolor/256x256/apps/therion-studio.png"
test -x "$usr_dir/bin/TherionStudio"
test -f "$desktop_file"
test -f "$icon_file"
python3 scripts/verify_install_layout.py --platform linux --prefix "$usr_dir"

cp "$desktop_file" "$appdir/therion-studio.desktop"
cp "$icon_file" "$appdir/therion-studio.png"
ln -sf therion-studio.png "$appdir/.DirIcon"
bash scripts/linux-packages/prepare_appimage_appdir.sh "$appdir"
if ! find "$appdir/usr/lib" -maxdepth 1 -name "libproxy.so.1*" | grep -q .; then
    echo "AppImage AppDir is missing bundled libproxy.so.1" >&2
    find "$appdir/usr/lib" -maxdepth 1 -type f -name "libproxy.so*" -print | sort >&2 || true
    exit 1
fi
if ! find "$appdir/usr/lib" -maxdepth 1 -name "libduktape.so*" | grep -q .; then
    echo "AppImage AppDir is missing bundled libduktape.so runtime dependency" >&2
    find "$appdir/usr/lib" -maxdepth 1 -type f -name "libduktape.so*" -print | sort >&2 || true
    exit 1
fi
if ! find "$appdir/usr/lib" -maxdepth 1 -name "libpxbackend-*.so*" | grep -q .; then
    echo "AppImage AppDir is missing bundled libproxy backend runtime dependency" >&2
    find "$appdir/usr/lib" -maxdepth 1 -type f -name "libpxbackend-*.so*" -print | sort >&2 || true
    exit 1
fi

pushd build-linux-appimage >/dev/null
wget -q "$APPIMAGETOOL_URL" -O appimagetool.AppImage
echo "$APPIMAGETOOL_SHA256  appimagetool.AppImage" | sha256sum --check --strict
runtime_file="runtime-${THERION_STUDIO_APPIMAGE_ARCHITECTURE_LABEL}"
wget -q "$APPIMAGE_RUNTIME_URL" -O "$runtime_file"
echo "$APPIMAGE_RUNTIME_SHA256  $runtime_file" | sha256sum --check --strict
chmod +x appimagetool.AppImage
ARCH="$THERION_STUDIO_APPIMAGE_ARCHITECTURE_LABEL" APPIMAGE_EXTRACT_AND_RUN=1 ./appimagetool.AppImage \
    --runtime-file "$runtime_file" \
    AppDir \
    "TherionStudio-${THERION_STUDIO_PACKAGE_LABEL}-Linux-${THERION_STUDIO_APPIMAGE_ARCHITECTURE_LABEL}.AppImage"
popd >/dev/null

if [[ -n "${HOST_UID:-}" && -n "${HOST_GID:-}" ]]; then
    chown -R "$HOST_UID:$HOST_GID" build-linux-appimage
fi
