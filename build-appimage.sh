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

# Copy binary
cp build/bin/tic80 "$APPDIR/usr/bin/"

# Copy desktop file (must be in AppDir root and named after the AppImage)
sed 's|Icon=tic80|Icon=tic80|' build/linux/tic80.desktop.in > "$APPDIR/TIC-80.desktop"

# Copy icon to AppDir root (AppImage standard location)
cp build/linux/tic80.png "$APPDIR/tic80.png"

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
