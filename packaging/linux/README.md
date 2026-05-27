# Balenie pre Linux: Debian a AppImage

Tento adresár obsahuje informácie a pomocný skript na zostavenie natívneho Debian balíčka (`.deb`) a univerzálneho stand-alone balíčka (`.AppImage`) pre Linux.

## Vyžiadané závislosti pre kompiláciu

Pred spustením balenia sa uistite, že máte nainštalované nasledujúce závislosti (napr. na Ubuntu/Debian/Linux Mint):

```sh
sudo apt-get update && sudo apt-get install -y \
  qt6-base-dev qt6-base-dev-tools qt6-svg-dev qt6-tools-dev qt6-tools-dev-tools \
  build-essential cmake ninja-build curl
```

---

## Rýchle automatické zostavenie oboch balíkov

Pripravili sme pomocný skript, ktorý automaticky skompiluje aplikáciu v režime `Release` (s vypnutými testami pre rýchly build) a vygeneruje oba balíky naraz:

```sh
chmod +x packaging/linux/build-packages.sh
./packaging/linux/build-packages.sh
```

Po úspešnom dokončení nájdete balíky v priečinku `build/`:
- `build/therion-studio-*-Linux.deb`
- `build/Therion_Studio-x86_64.AppImage`

---

## Manuálny postup zostavenia

### 1. Zostavenie Debian balíčka (.deb)

Debian balíček sa generuje pomocou nástroja CPack, ktorý je priamo integrovaný v našom CMake projekte.

```sh
# 1. Konfigurácia projektu v režime Release s vypnutými testami
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF

# 2. Zostavenie projektu
cmake --build build --target TherionStudio

# 3. Generovanie .deb balíka
cd build
cpack -G DEB
```

### 2. Zostavenie AppImage

AppImage vyžaduje stiahnutie nástroja `linuxdeploy` a jeho Qt6 pluginu na správne zabalenie a izoláciu Qt knižníc.

```sh
cd build

# 1. Stiahnutie baliacich nástrojov
curl -L -O https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
curl -L -O https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage

# 2. Lokálna inštalácia do dočasného AppDir adresára
cmake --install . --prefix AppDir/usr

# 3. Príprava ikon a spúšťača
mkdir -p AppDir/usr/share/icons/hicolor/scalable/apps AppDir/usr/share/applications
cp ../resources/app/app-icon.svg AppDir/usr/share/icons/hicolor/scalable/apps/therion-studio.svg

# Vytvorenie spúšťacieho .desktop súboru pre AppImage
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

# 4. Spustenie linuxdeploy na vygenerovanie AppImage
PATH=".:$PATH" ./linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt --output appimage
```
