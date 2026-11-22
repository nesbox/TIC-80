[![Build Status](https://github.com/nesbox/TIC-80/workflows/Build/badge.svg)](https://github.com/nesbox/TIC-80/actions?query=workflow%3ABuild)

![TIC-80](https://tic80.com/img/logo64.png)
**TIC-80 TINY COMPUTER** — [tic80.com](https://tic80.com)

- [About](#about)
  - [Features](#features)
- [Binary Downloads](#binary-downloads)
  - [Nightly builds](#nightly-builds)
- [Pro Version](#pro-version)
  - [Pro Features](#pro-features)
- [Community](#community)
- [Contributing](#contributing)
- [Build Instructions](#build-instructions)
  - [Windows](#windows)
    - [MSVC (Microsoft Visual C++)](#msvc-microsoft-visual-c)
      - [Windows 10 / 11 64-bit (x64)](#windows-10--11-64-bit-x64)
      - [Windows XP / Windows 7 32-bit (x86)](#windows-xp--windows-7-32-bit-x86)
    - [MSYS2 / MINGW](#msys2--mingw)
      - [Windows 10 / 11 64-bit (x64)](#windows-10--11-64-bit-x64-1)
  - [Linux](#linux)
    - [Ubuntu 22.04 / 24.04](#ubuntu-2204--2404)
    - [Arch](#arch)
    - [Fedora 40+](#fedora-40)
    - [openSUSE](#opensuse)
    - [Raspberry Pi OS (64-Bit)](#raspberry-pi-os-64-bit)
  - [Mac](#mac)
  - [FreeBSD](#freebsd)
  - [Other platforms](#other-platforms)
- [Install Instructions](#install-instructions)
  - [Linux](#linux-1)
  - [Android](#android)
  - [Legacy iOS / tvOS](#legacy-ios--tvos)
  - [Credits](#credits)
- [License](#license)

# About
TIC-80 is a free and open source fantasy computer for making, playing and sharing tiny games.

With TIC-80 you get built-in tools for development: code, sprites, maps, sound editors and the command line, which is enough to create a mini retro game.

Games are packaged into a cartridge file, which can be easily distributed. TIC-80 works on all popular platforms. This means your cartridge can be played in any device.

To make a retro styled game, the whole process of creation and execution takes place under some technical limitations: 240x136 pixel display, 16 color palette, 256 8x8 color sprites, 4 channel sound, etc.

![TIC-80](https://user-images.githubusercontent.com/1101448/92492270-d6bcbc80-f1fb-11ea-9d2d-468ad015ace2.gif)

### Features
- Multiple programming languages: [Lua](https://www.lua.org),
  [Moonscript](https://moonscript.org),
  [YueScript](https://yuescript.org),
  [Javascript](https://developer.mozilla.org/en-US/docs/Web/JavaScript),
  [Fennel](https://fennel-lang.org),
  [Scheme](https://www.scheme.org/),
  [Squirrel](https://www.squirrel-lang.org),
  [Ruby](https://www.ruby-lang.org/en),
  [Wren](https://wren.io/),
  [Janet](https://janet-lang.org),
  [Python](https://www.python.org),
  [MiniScript](https://miniscript.org/),
  [Gravity](https://marcobambini.github.io/gravity/), and
  [Forth](https://forth-standard.org/), plus languages that compile to
  [WASM](https://github.com/nesbox/TIC-80/wiki/WASM).
- Games can have mouse and keyboard as input
- Games can have up to 4 controllers as input (with up to 8 buttons, each)
- Built-in editors: for code, sprites, world maps, sound effects and music
- Memory banks: up to 8 banks in a cart (PRO) to swap assets while a game runs
- Native builds for Windows, Linux (x64/arm64), macOS (Apple Silicon/Intel), Android, FreeBSD, Raspberry Pi, Nintendo 3DS, Nintendo Switch, Tanmatsu and libretro; the web player runs through WebGL or WebGPU
- Moderated community

# Binary Downloads

## Stable Builds
You can download compiled versions for the major operating systems directly from our [Releases](https://github.com/nesbox/TIC-80/releases) page.

## Nightly Builds
Can be downloaded from official [nightly.link](https://nightly.link/nesbox/TIC-80/workflows/build/main) page or from the [Github Actions](https://github.com/nesbox/TIC-80/actions?query=branch%3Amain) page.

# Pro Version
To help support TIC-80 development, we have a [PRO Version](https://nesbox.itch.io/tic80).

This version has a few additional features and binaries can only be downloaded on our itch.io page.

Users who can't afford the program can easily build the pro version from the source code using the `cmake .. -DBUILD_PRO=On` command.

## Pro Features
- Save/load cartridges as text projects, and create your game in any editor you want, also useful for version control systems.
- Even more memory banks: instead of having only 1 memory bank you have 8.
- Export your game without editors, and then publish it to app stores.

# Community
You can play and share games, tools and music at [tic80.com/play](https://tic80.com/play).

The community also hangs out and discusses on [Telegram](https://t.me/tic80) or [Discord](https://discord.gg/HwZDw7n4dN).

# Contributing
You can contribute by reporting a bug or requesting a new feature on our [Issues](https://github.com/nesbox/TIC-80/issues) page.
Keep in mind when engaging on a discussion to follow our [Code of Conduct](https://github.com/nesbox/TIC-80/blob/main/CODE_OF_CONDUCT.md).

You can also contribute by reviewing or improving our [Wiki](https://github.com/nesbox/TIC-80/wiki).
The wiki holds TIC-80 documentation, code snippets and game development tutorials.

# Build instructions

## Windows

### MSVC (Microsoft Visual C++)

#### Windows 10 / 11 64-bit (x64)
This guide assumes you're running PowerShell with an elevated prompt.

- Install [Git](https://git-scm.com/download/win), [CMake](https://cmake.org/download), [Visual Studio 2022 Build Tools](https://winstall.app/apps/Microsoft.VisualStudio.2022.BuildTools) and [Ruby+Devkit 2.7.8 x64](https://github.com/oneclick/rubyinstaller2/releases/download/RubyInstaller-2.7.8-1/rubyinstaller-devkit-2.7.8-1-x64.exe) manually or with [WinGet](https://github.com/microsoft/winget-cli):
```
winget install Git.Git Kitware.CMake Microsoft.VisualStudio.2022.BuildTools RubyInstallerTeam.RubyWithDevKit.2.7
```
- Install the necessary dependencies within VS2022:
  - Launch "Visual Studio Installer"
  - Click "Modify"
  - Check "Desktop Development with C++"
  - Make sure the following components are installed:
    - Windows 10/11 SDK
    - MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)
  - Click "Modify"
- Run `ridk install` with options `1,3` to set up [MSYS2](https://www.msys2.org) and development toolchain
- Add MSYS2's [`gcc`](https://gcc.gnu.org) at `C:\Ruby27-x64\msys64\mingw64\bin` to your `$PATH` [manually](https://www.java.com/en/download/help/path.html#:~:text=Mac%20OS%20X.-,Windows,-Windows%2010%20and) or with the following PowerShell command:

```
[Environment]::SetEnvironmentVariable('Path', $env:Path + ';C:\Ruby27-x64\msys64\mingw64\bin', [EnvironmentVariableTarget]::Machine)
```

- Open a new elevated prompt and run the following commands:

```
git clone --recursive https://github.com/nesbox/TIC-80 && cd .\TIC-80\build
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_ALL=On -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ..
cmake --build . --config Release --parallel
```

You'll find `tic80.exe` in `TIC-80\build\bin`.

The default system layer is sokol. To build the SDL version instead, add `-DBUILD_SDL=On -DBUILD_SOKOL=Off -DBUILD_SDLGPU=On` to the `cmake` line.

#### Windows XP / Windows 7 32-bit (x86)
This is a legacy target and is not exercised by CI. The build process was tested on Windows 11 64-bit (x64); it should run on Windows 7 SP1 32-bit (x86) as well. This guide assumes you're running an elevated Command Prompt.

- Install [Git](https://git-scm.com/download/win), [CMake](https://cmake.org/download), [Visual Studio 2019 Build Tools](https://winstall.app/apps/Microsoft.VisualStudio.2019.BuildTools) and [Ruby+Devkit 2.7.8 x86](https://github.com/oneclick/rubyinstaller2/releases/download/RubyInstaller-2.7.8-1/rubyinstaller-devkit-2.7.8-1-x86.exe)
- Install the necessary dependencies within VS2019:
  - Launch "Visual Studio Installer"
  - Click "Modify"
  - Check "Desktop Development with C++"
  - Go to "Individual components"
  - Search for "v141"
  - Install:
    - C++ Windows XP Support for VS 2017 (v141) tools [Deprecated]
    - MSVC v141 - VS 2017 C++ x64/x86 build tools (v14.16)
  - Click "Modify"
- Run `ridk install` with options `1,3` to set up [MSYS2](https://www.msys2.org) and development toolchain
- Add MSYS2's [`gcc`](https://gcc.gnu.org) at `C:\Ruby27\msys32\mingw32\bin` to your `$PATH` [(guide)](https://www.java.com/en/download/help/path.html#:~:text=your%20java%20code.-,Windows%207,-From%20the%20desktop)

- Open a new elevated Command Prompt and run the following commands:

```
git clone --recursive https://github.com/nesbox/TIC-80 && cd .\TIC-80\build
cmake -G "Visual Studio 16 2019" -A Win32 -T v141_xp -DCMAKE_BUILD_TYPE=Release -DBUILD_SDL=On -DBUILD_SOKOL=Off -DBUILD_WITH_ALL=On -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ..
cmake --build . --config Release --parallel
```

You'll find `tic80.exe` in `TIC-80\build\bin`.

### MSYS2 / MINGW

#### Windows 10 / 11 64-bit (x64)
This guide assumes you're running PowerShell with an elevated prompt.

- Install [Git](https://git-scm.com/download/win), [CMake](https://cmake.org/download) and [Ruby+Devkit 2.7.8 x64](https://github.com/oneclick/rubyinstaller2/releases/download/RubyInstaller-2.7.8-1/rubyinstaller-devkit-2.7.8-1-x64.exe) manually or with [WinGet](https://github.com/microsoft/winget-cli):
```
winget install Git.Git Kitware.CMake RubyInstallerTeam.RubyWithDevKit.2.7
```
- Run `ridk install` with options `1,3` to set up [MSYS2](https://www.msys2.org) and development toolchain
- Add MSYS2's [`gcc`](https://gcc.gnu.org) at `C:\Ruby27-x64\msys64\mingw64\bin` to your `$PATH` [manually](https://www.java.com/en/download/help/path.html#:~:text=Mac%20OS%20X.-,Windows,-Windows%2010%20and) or with the following PowerShell command:

```
[Environment]::SetEnvironmentVariable('Path', $env:Path + ';C:\Ruby27-x64\msys64\mingw64\bin', [EnvironmentVariableTarget]::Machine)
```

- Open a new elevated prompt and run the following commands:

```
git clone --recursive https://github.com/nesbox/TIC-80 && cd .\TIC-80\build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_ALL=On -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ..
$numCPUs = [Environment]::ProcessorCount
mingw32-make "-j$numCPUs"
```

You'll find `tic80.exe` in `TIC-80\build\bin`.

## Linux

The commands below build with GLES (`-DUSE_GLES2=On`), as the release Linux binaries do: sokol then needs the X11/Xi/Xcursor, EGL/GLES and ALSA headers. Drop that flag on a machine that can provide an OpenGL 4.3 core context (the GL build uses GLX). Pulse/audio/samplerate are optional audio backends the package lists also carry. For the SDL build instead, configure with `-DBUILD_SDL=On -DBUILD_SOKOL=Off` (and `-DBUILD_SDLGPU=On` for the GPU renderer).

### Ubuntu 22.04 / 24.04
Run the following commands from a terminal:

```
sudo apt update && sudo apt -y install build-essential cmake pkg-config git libx11-dev libxi-dev libxcursor-dev libegl1-mesa-dev libgles2-mesa-dev libasound2-dev libpulse-dev libaudio-dev libsamplerate0-dev libcurl4-openssl-dev ruby-dev rake
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_ALL=On -DUSE_GLES2=On -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .. && cmake --build . --parallel
```

Install with [Install Instructions](#install-instructions)

### Arch
Run the following commands in the Terminal:

```
sudo pacman -S --needed base-devel cmake git ruby ruby-rake libx11 libxi libxcursor mesa libglvnd alsa-lib libpulse curl
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_ALL=On -DUSE_GLES2=On -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .. && cmake --build . --parallel
```

Install with [Install Instructions](#install-instructions)

### Fedora 40+
Run the following commands from a terminal:

```
sudo dnf -y groupinstall "Development Tools"
sudo dnf -y install cmake git ruby ruby-devel rubygem-rake libX11-devel libXi-devel libXcursor-devel libglvnd-devel mesa-libEGL-devel alsa-lib-devel pulseaudio-libs-devel libcurl-devel
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_ALL=On -DUSE_GLES2=On -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .. && cmake --build . --parallel
```

Install with [Install Instructions](#install-instructions)

### openSUSE
Run the following commands from a terminal:

```
sudo zypper refresh
sudo zypper install --no-confirm --type pattern devel_basis
sudo zypper install --no-confirm cmake git ruby ruby-devel rubygem-rake libX11-devel libXi-devel libXcursor-devel Mesa-libEGL-devel Mesa-libGLESv2-devel alsa-devel libpulse-devel libcurl-devel
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_ALL=On -DUSE_GLES2=On -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .. && cmake --build . --parallel
```

Install with [Install Instructions](#install-instructions)

### Raspberry Pi OS (64-Bit)
Run the following commands from a terminal:

```
sudo apt update && sudo apt -y install build-essential cmake pkg-config git libx11-dev libxi-dev libxcursor-dev libegl1-mesa-dev libgles2-mesa-dev libasound2-dev libpulse-dev libaudio-dev libsamplerate0-dev libcurl4-openssl-dev ruby-dev rake
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_ALL=On -DUSE_GLES2=On -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .. && cmake --build . --parallel 2
```

Install with [Install Instructions](#install-instructions)

The bare-metal Raspberry Pi builds (Circle) live in `src/system/baremetalpi` and are produced by the `rpi-baremetal` jobs in the build workflow.

## Mac
install `Command Line Tools for Xcode` and `brew` package manager

### Standard build (sokol)
Run the following commands in the Terminal:
```
brew install git cmake
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_ALL=On -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ..
cmake --build . --parallel
```

### Native Apple Backend (Metal / AppKit / Swift)
TIC-80 also includes an optional native Apple backend built with Metal, AppKit, AVFoundation, GameController, and Swift. It is a macOS backend; it does not build for iOS or tvOS. The `-G Xcode` generator needs a full Xcode installation, not only the Command Line Tools:
```
brew install git cmake
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80
cmake -G Xcode -B build -DBUILD_APPLE=ON -DBUILD_STATIC=ON -DBUILD_RENDER_CACHE=ON -DBUILD_WITH_ALL=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --config Release
```

To create an application icon for a development version, use the plist that the configure step generates and the icon shipped in `build/macosx`. The Xcode generator puts the binary in the `Release` configuration folder:
```
mkdir -p ~/Applications/tic80dev.app/Contents/{MacOS,Resources}
cp -f build/macosx/tic80.plist ~/Applications/tic80dev.app/Contents/Info.plist
cp -f build/macosx/tic80.icns ~/Applications/tic80dev.app/Contents/Resources
cat > ~/Applications/tic80dev.app/Contents/MacOS/tic80 <<EOF
#!/bin/sh
exec "$PWD/build/bin/Release/tic80" --skip >/dev/null
EOF
chmod +x ~/Applications/tic80dev.app/Contents/MacOS/tic80
```

## FreeBSD
run the following commands in the Terminal
```
sudo pkg install gcc git cmake ruby libglvnd libglu freeglut mesa-devel mesa-dri alsa-lib
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_ALL=On -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ..
cmake --build . --parallel
```

Mesa looks for swrast_dri.so from the wrong path, so also symlink it:

```
sudo ln -s /usr/local/lib/dri/swrast_dri.so /usr/local/lib/dri-devel/
```

## Other platforms

- **libretro**: configure with `-DBUILD_LIBRETRO=On`.
- **Nintendo 3DS / Nintendo Switch**: devkitPro toolchains; see the `nintendo-3ds` and `nintendo-switch` jobs in `.github/workflows/build.yml`.
- **Tanmatsu / Konsool**: the ESP-IDF project in `src/system/tanmatsu` and its README.
- **Android**: the SDL build is the Gradle project in `build/android`, the sokol build is `build/android-sokol` with its `apk.sh`; see the `android` and `sdl-android` jobs in `.github/workflows/build.yml`.

# Install instructions

## Linux
- Build with `cmake --build . --parallel` and locate the binary at `TIC-80/build/bin/tic80`.
- To install system-wide run `sudo cmake --install .` from the build directory.

TIC-80 can now be run with `tic80` (if installed) or `./tic80` (with no installation).

## Legacy iOS / tvOS
There is no iOS or tvOS build in the releases; the native Apple backend is macOS only. These community forks were built from much older versions:

- 0.60.3: https://github.com/brunophilipe/TIC-80
- 0.45.0: https://github.com/CliffsDover/TIC-80

## Android
You can find the compiled version ready download and install [on F-Droid](https://f-droid.org/packages/com.nesbox.tic/):  
[<img alt="Get it on F-Droid" src="https://fdroid.gitlab.io/artwork/badge/get-it-on.png" width="256">](https://f-droid.org/packages/com.nesbox.tic/)

## Credits
* Filippo Rivato — [Twitter @HomineLudens](https://twitter.com/HomineLudens)
* Fred Bednarski — [Twitter @FredBednarski](https://twitter.com/FredBednarski)
* Al Rado — [Twitter @alrado2](https://twitter.com/alrado2)
* Trevor Martin — [Twitter @trelemar](https://twitter.com/trelemar)
* MonstersGoBoom — [Twitter @MonstersGoBoom](https://twitter.com/MonstersGo)
* Matheus Lessa — [Twitter @matheuslrod](https://twitter.com/matheuslrod)
* CliffsDover — [Twitter @DancingBottle](https://twitter.com/DancingBottle)
* Frantisek Jahoda — [GitHub @jahodfra](https://github.com/jahodfra)
* Guilherme Medeiros — [GitHub @frenetic](https://github.com/frenetic)
* Andrei Rudenko — [GitHub @AndreiRudenko](https://github.com/AndreiRudenko)
* Phil Hagelberg — [@technomancy](https://technomancy.us/colophon)
* Rob Loach — [Twitter @RobLoach](https://twitter.com/RobLoach) [GitHub @RobLoach](https://github.com/RobLoach)
* Wade Brainerd — [GitHub @wadetb](https://github.com/wadetb)
* Paul Robinson — [GitHub @paul59](https://github.com/paul59)
* Stefan Devai — [GitHub @stefandevai](https://github.com/stefandevai) [Blog stefandevai.me](https://stefandevai.me)
* Damien de Lemeny — [GitHub @ddelemeny](https://github.com/ddelemeny)
* Adrian Siekierka — [GitHub @asiekierka](https://github.com/asiekierka) [Website](https://asie.pl/)
* Jay Em (Sweetie16 palette) — [Twitter @GrafxKid](https://twitter.com/GrafxKid)
* msx80 — [Twitter @msx80](https://twitter.com/msx80) [Github msx80](https://github.com/msx80)
* Josh Goebel — [Twitter @dreamer3](https://twitter.com/dreamer3) [Github joshgoebel](https://github.com/joshgoebel)
* Joshua Minor — [GitHub @jminor](https://github.com/jminor)
* Julia Nelz — [Github @remi6397](https://github.com/remi6397) [WWW](https://nelz.pl)
* Thorben Krüger — [Mastodon @benthor@chaos.social](https://chaos.social/@benthor)
* David St—Hilaire — [GitHub @sthilaid](https://github.com/sthilaid)
* Alec Troemel — [Github @alectroemel](https://github.com/AlecTroemel)
* Kolten Pearson — [Github @koltenpearson](https://github.com/koltenpearson)
* Cort Stratton — [Github @cdwfs](https://github.com/cdwfs)
* Alice — [Github @aliceisjustplaying](https://github.com/aliceisjustplaying)
* Sven Knebel — [Github @sknebel](https://github.com/sknebel)
* Graham Bates — [Github @grahambates](https://github.com/grahambates)
* Kii — [Github @kiikrindar](https://github.com/kiikrindar)
* Matt Westcott — [Github @gasman](https://github.com/gasman)
* NuSan — [Github @TheNuSan](https://github.com/thenusan)
* Li Jin — [Github @pigpigyyy](https://github.com/pigpigyyy)
* Dania Rifki — [Github @Kaleidosium](https://github.com/Kaleidosium)
* Carsten Teibes — [GitHub @carstene1ns](https://github.com/carstene1ns)

# License

TIC-80 is released under the MIT License; see [LICENSE](LICENSE). The vendored third-party components and their licenses are listed in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) and the [LICENSES](LICENSES) folder.
