#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

# Colors for terminal output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Therion Studio - Linux Packaging Script ===${NC}"

# Navigate to repository root
cd "$(dirname "$0")/../.."

# 1. CMake Configure
echo -e "${BLUE}[1/6] Configuring CMake with Release and BUILD_TESTING=OFF...${NC}"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF

# 2. Build Target
echo -e "${BLUE}[2/6] Compiling TherionStudio...${NC}"
cmake --build build --target TherionStudio -j$(nproc)

# 3. Build DEB Package
echo -e "${BLUE}[3/6] Packaging DEB using CPack...${NC}"
cd build
cpack -G DEB

# 4. Download linuxdeploy for AppImage
echo -e "${BLUE}[4/6] Downloading linuxdeploy tools...${NC}"
curl -L -O https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
curl -L -O https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage

# 5. Install to AppDir
echo -e "${BLUE}[5/6] Preparing AppDir layout...${NC}"
rm -rf AppDir
cmake --install . --prefix AppDir/usr
mkdir -p AppDir/usr/share/icons/hicolor/scalable/apps AppDir/usr/share/applications
cp ../resources/app/app-icon.svg AppDir/usr/share/icons/hicolor/scalable/apps/therion-studio.svg

# Write AppImage desktop file
cat <<EOF > AppDir/usr/share/applications/therion-studio.desktop
[Desktop Entry]
Type=Application
Name=Therion Studio
Comment=Qt desktop editor for Therion cave surveying software
Exec=TherionStudio
Icon=therion-studio
Terminal=false
Categories=Development;Engineering;Science;
Keywords=speleology;cave;map;editor;survey;
StartupWMClass=TherionStudio
EOF

# 6. Generate AppImage
echo -e "${BLUE}[6/6] Building AppImage package...${NC}"
PATH=".:$PATH" ./linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt --output appimage

echo -e "${GREEN}=== Packaging Completed Successfully! ===${NC}"
echo -e "Packages are available in the build directory:"
ls -lh therion-studio-*-Linux.deb Therion_Studio-x86_64.AppImage
