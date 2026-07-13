#!/usr/bin/env bash
set -euo pipefail

required_env=(
    BUILD_TYPE
    DEB_QT_PACKAGES
    THERION_STUDIO_DEB_ARCHITECTURE_LABEL
    THERION_STUDIO_DEB_PLATFORM_LABEL
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

IFS=, read -r -a qt_packages <<< "$DEB_QT_PACKAGES"

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
    python3 \
    "${qt_packages[@]}"

rm -rf build-linux-packages
mkdir -p build-linux-packages
dpkg-query -W qt6-base-dev | cut -f2 > build-linux-packages/deb-qt-version.txt

cmake -S . -B build-linux-packages \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DTHERION_STUDIO_VERSION="$THERION_STUDIO_VERSION" \
    -DTHERION_STUDIO_PACKAGE_LABEL="$THERION_STUDIO_PACKAGE_LABEL" \
    -DTHERION_STUDIO_DEBIAN_VERSION="$THERION_STUDIO_DEBIAN_VERSION" \
    -DTHERION_STUDIO_DEB_PLATFORM_LABEL="$THERION_STUDIO_DEB_PLATFORM_LABEL" \
    -DTHERION_STUDIO_DEB_ARCHITECTURE_LABEL="$THERION_STUDIO_DEB_ARCHITECTURE_LABEL" \
    -DTHERION_ENABLE_STRICT_WARNINGS=ON

cmake --build build-linux-packages --target TherionStudio --parallel
cmake --install build-linux-packages --prefix "$PWD/build-linux-packages/install-smoke"
python3 scripts/verify_install_layout.py --platform linux --prefix "$PWD/build-linux-packages/install-smoke"
cpack \
    --config build-linux-packages/CPackConfig.cmake \
    -G DEB \
    -B build-linux-packages \
    -C "$BUILD_TYPE" \
    --verbose

if [[ -n "${HOST_UID:-}" && -n "${HOST_GID:-}" ]]; then
    chown -R "$HOST_UID:$HOST_GID" build-linux-packages
fi
