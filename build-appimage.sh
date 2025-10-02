#!/bin/bash

# Build TIC-80 AppImage for Linux

set -e

# Check if we're on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo "This script must be run on Linux to build AppImage"
    exit 1
fi

# Check if tic80 binary exists
if [ ! -f "build/bin/tic80" ]; then
    echo "Error: TIC-80 binary not found at build/bin/tic80"
    echo "Please build TIC-80 first with: mkdir build && cd build && cmake .. && make"
    exit 1
fi

# Create AppDir structure
APPDIR="TIC-80.AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$APPDIR/usr/share/metainfo"

# Copy binary
cp build/bin/tic80 "$APPDIR/usr/bin/"

# Copy desktop file
sed 's|Icon=tic80|Icon=tic80.png|' build/linux/tic80.desktop.in > "$APPDIR/usr/share/applications/tic80.desktop"

# Copy icon
cp build/linux/tic80.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/"

# Configure and copy metainfo
VERSION_MAJOR=$(grep "VERSION_MAJOR" cmake/version.cmake | sed 's/.*VERSION_MAJOR \([0-9]*\).*/\1/')
VERSION_MINOR=$(grep "VERSION_MINOR" cmake/version.cmake | sed 's/.*VERSION_MINOR \([0-9]*\).*/\1/')
VERSION_REVISION=$(grep "VERSION_REVISION" cmake/version.cmake | sed 's/.*VERSION_REVISION \([0-9]*\).*/\1/')
VERSION_YEAR=$(date +%Y)
VERSION_MONTH=$(date +%m)
VERSION_DAY=$(date +%d)

sed -e "s/@PROJECT_VERSION@/$VERSION_MAJOR.$VERSION_MINOR.$VERSION_REVISION/g" \
    -e "s/@VERSION_YEAR@/$VERSION_YEAR/g" \
    -e "s/@VERSION_MONTH@/$VERSION_MONTH/g" \
    -e "s/@VERSION_DAY@/$VERSION_DAY/g" \
    build/linux/com.tic80.TIC_80.metainfo.xml.in > "$APPDIR/usr/share/metainfo/com.tic80.TIC_80.metainfo.xml"

# Create AppRun script
cat > "$APPDIR/AppRun" << 'EOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "${0}")")"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"
exec "${HERE}/usr/bin/tic80" "$@"
EOF
chmod +x "$APPDIR/AppRun"

# Copy required libraries (if any - TIC-80 should be statically linked)
# ldd build/bin/tic80 | grep "=>" | awk '{print $3}' | xargs -I {} cp {} "$APPDIR/usr/lib/" 2>/dev/null || true

# Download appimagetool if not present
if [ ! -f "appimagetool.AppImage" ]; then
    echo "Downloading appimagetool..."
    wget -O appimagetool.AppImage https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x appimagetool.AppImage
fi

# Create AppImage
echo "Creating AppImage..."
./appimagetool.AppImage "$APPDIR" TIC-80.AppImage

echo "AppImage created: TIC-80.AppImage"
