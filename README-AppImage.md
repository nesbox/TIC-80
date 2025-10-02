# TIC-80 AppImage Build Instructions

This document describes how to build a TIC-80 AppImage for Linux distribution.

## Prerequisites

- Linux system (Ubuntu, Fedora, etc.)
- TIC-80 must be already built (binary at `build/bin/tic80`)

## Building TIC-80 First

```bash
mkdir build
cd build
cmake .. -DBUILD_SOKOL=ON -DBUILD_WITH_ALL=ON
make -j$(nproc)
cd ..
```

## Building the AppImage

```bash
./build-appimage.sh
```

This script will:
1. Verify the TIC-80 binary exists
2. Create the AppDir structure
3. Copy the tic80 binary, desktop files, and icons
4. Download appimagetool if needed
5. Generate the TIC-80.AppImage file

## What the AppImage contains

The AppImage includes:
- TIC-80 executable (tic80 binary)
- Desktop integration files (.desktop, icon)
- AppRun script for proper execution

## Distribution

The resulting `TIC-80.AppImage` file can be distributed and run on any Linux system without installation. Users just need to:

```bash
chmod +x TIC-80.AppImage
./TIC-80.AppImage
```

## CI/CD Integration

The AppImage is automatically built and deployed via GitHub Actions:

- **Workflow**: `.github/workflows/build.yml`
- **Job**: `linux-gcc12` (Ubuntu build)
- **Artifact**: `tic80-appimage` - downloadable from GitHub Actions runs

## Notes

- TIC-80 is designed to be statically linked, so the AppImage should not require external libraries
- Desktop integration works automatically when the AppImage is run
- For development tools and demo assets, users should download the full TIC-80 distribution
