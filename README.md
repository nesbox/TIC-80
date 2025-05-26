[![Build Status](https://github.com/nesbox/TIC-80/workflows/Build/badge.svg)](https://github.com/nesbox/TIC-80/actions?query=workflow%3ABuild)

![TIC-80](https://tic80.com/img/logo64.png)
**TIC-80 TINY COMPUTER** — [tic80.com](https://tic80.com)

- [About](#about)
  - [Features](#features)
- [Binary Downloads](#binary-downloads)
  - [Nightly builds](#nightly-builds)
  - [Unofficial Linux/arm64 nightly builds](#unofficial-builds)
- [Pro Version](#pro-version)
  - [Pro Features](#pro-features)
- [Community](#community)
- [Contributing](#contributing)
- [Build Instructions](#build-instructions)
  - [Windows](#windows)
    - [MSVC (Microsoft Visual C++)](#msvc-microsoft-visual-c)
      - [Windows XP / Windows 7 32-bit (x86)](#windows-xp--windows-7-32-bit-x86)
      - [Windows 10 / 11 64-bit (x64)](#windows-10--11-64-bit-x64)
    - [MSYS2 / MINGW](#msys2--mingw)
      - [Windows 10 / 11 64-bit (x64)](#windows-10--11-64-bit-x64-1)
  - [Linux](#linux)
    - [Ubuntu](#ubuntu)
      - [Ubuntu 22.04 (Jammy Jellyfish)](#ubuntu-2204-jammy-jellyfish)
      - [Ubuntu 24.04 (Noble Numbat)](#ubuntu-2404-noble-numbat)
    - [Arch](#arch)
    - [Fedora](#fedora)
      - [Fedora 36](#fedora-36)
      - [Fedora 40](#fedora-40)
    - [openSUSE](#opensuse)
      - [openSUSE Tumbleweed / Leap 16.0](#opensuse-tumbleweed--leap-160)
    - [Raspberry Pi](#raspberry-pi)
      - [Raspberry Pi OS (64-Bit) (Bookworm)](#raspberry-pi-os-64-bit-bookworm)
      - [Raspberry Pi (Retropie)](#raspberry-pi-retropie)
  - [Mac](#mac)
  - [FreeBSD](#freebsd)
- [Install Instructions](#install-instructions)
  - [Linux](#linux-1)
  - [Android](#android)
  - [iOS / tvOS](#ios--tvos)
  - [Credits](#credits)

# About
TIC-80 is a free and open source fantasy computer for making, playing and sharing tiny games.

With TIC-80 you get built-in tools for development: code, sprites, maps, sound editors and the command line, which is enough to create a mini retro game.

Games are packaged into a cartridge file, which can be easily distributed. TIC-80 works on all popular platforms. This means your cartridge can be played in any device.

To make a retro styled game, the whole process of creation and execution takes place under some technical limitations: 240x136 pixel display, 16 color palette, 256 8x8 color sprites, 4 channel sound, etc.

![TIC-80](https://user-images.githubusercontent.com/1101448/92492270-d6bcbc80-f1fb-11ea-9d2d-468ad015ace2.gif)

### Features
- Multiple programming languages: [Lua](https://www.lua.org),
  [Moonscript](https://moonscript.org),
  [Javascript](https://developer.mozilla.org/en-US/docs/Web/JavaScript),
  [Ruby](https://www.ruby-lang.org/en),
  [Wren](https://wren.io/),
  [Fennel](https://fennel-lang.org),
  [Squirrel](https://www.squirrel-lang.org),
  [Janet](https://janet-lang.org), and
  [Python](https://www.python.org).
- Games can have mouse and keyboard as input
- Games can have up to 4 controllers as input (with up to 8 buttons, each)
- Built-in editors: for code, sprites, world maps, sound effects and music
- An additional memory bank: load different assets from your cartridge while your game is executing
- Moderated community

# Binary Downloads

## Stable Builds
You can download compiled versions for the major operating systems directly from our [Releases](https://github.com/nesbox/TIC-80/releases) page.

## Nightly Builds
Can be downloaded from official [nightly.link](https://nightly.link/nesbox/TIC-80/workflows/build/main) page or from the [Github Actions](https://github.com/nesbox/TIC-80/actions?query=branch%3Amain) page.

## Unofficial Builds
Linux (arm64) builds can be downloaded from _aliceisjustplaying_ [nightly.link](https://nightly.link/aliceisjustplaying/TIC-80/workflows/build-linux-arm64/main?preview) page. Tested on Raspberry Pi OS (64-bit) (Bookworm), Asahi Linux (Fedora Remix), Ubuntu 22.04 and Fedora 40.

# Pro Version
To help support TIC-80 development, we have a [PRO Version](https://nesbox.itch.io/tic80).

This version has a few additional features and binaries can only be downloaded on our itch.io page.

For users who can't afford the program can easily build the pro version from the source code using `cmake .. -DBUILD_PRO=On` command.

## Pro Features
- Save/load cartridges in text format, and create your game in any editor you want, also useful for version control systems.
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

#### Windows XP / Windows 7 32-bit (x86)
The build process has been tested on Windows 11 64-bit (x64); all this should run on Windows 7 SP1 32-bit (x86) as well. This guide assumes you're running an elevated Command Prompt.

- Install [Git](https://git-scm.com/download/win), [CMake](https://cmake.org/download), [Visual Studio 2019 Build Tools](https://winstall.app/apps/Microsoft.VisualStudio.2019.BuildTools) and [Ruby+Devkit 2.7.8 x86](https://github.com/oneclick/rubyinstaller2/releases/download/RubyInstaller-2.7.8-1/rubyinstaller-devkit-2.7.8-1-x86.exe)
- Install the neccessary dependencies within VS2019:
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
copy /y .\build\janet\janetconf.h .\vendor\janet\src\conf\janetconf.h
cmake -G "Visual Studio 16 2019" -A Win32 -T v141_xp -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_WITH_ALL=On ..
cmake --build . --parallel
```

You'll find `tic80.exe` in `TIC-80\build\bin`.

#### Windows 10 / 11 64-bit (x64)
This guide assumes you're running PowerShell with an elevated prompt.

- Install [Git](https://git-scm.com/download/win), [CMake](https://cmake.org/download), [Visual Studio 2019 Build Tools](https://winstall.app/apps/Microsoft.VisualStudio.2019.BuildTools) and [Ruby+Devkit 2.7.8 x64](https://github.com/oneclick/rubyinstaller2/releases/download/RubyInstaller-2.7.8-1/rubyinstaller-devkit-2.7.8-1-x64.exe) manually or with [WinGet](https://github.com/microsoft/winget-cli):
```
winget install Git.Git Kitware.CMake Microsoft.VisualStudio.2019.BuildTools RubyInstallerTeam.RubyWithDevKit.2.7
```
- Install the neccessary dependencies within VS2019:
  - Launch "Visual Studio Installer"
  - Click "Modify"
  - Check "Desktop Development with C++"
  - Make sure the following components are installed:
    - Windows 10 SDK (10.0.19041.0)
    - MSVC v142 - VS 2019 C+ + x64/x86 build tools (Latest)
  - Click "Modify"
- Run `ridk install` with options `1,3` to set up [MSYS2](https://www.msys2.org) and development toolchain
- Add MSYS2's [`gcc`](https://gcc.gnu.org) at `C:\Ruby27-x64\msys64\mingw64\bin` to your `$PATH` [manually](https://www.java.com/en/download/help/path.html#:~:text=Mac%20OS%20X.-,Windows,-Windows%2010%20and) or with the following PowerShell command:

```
[Environment]::SetEnvironmentVariable('Path', $env:Path + ';C:\Ruby27-x64\msys64\mingw64\bin', [EnvironmentVariableTarget]::Machine)
```

- Open a new elevated prompt and run the following commands:

```
git clone --recursive https://github.com/nesbox/TIC-80 && cd .\TIC-80\build
cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SDLGPU=On -DBUILD_WITH_ALL=On ..
cmake --build . --parallel
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
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SDLGPU=On -DBUILD_WITH_ALL=On ..
$numCPUs = [Environment]::ProcessorCount
mingw32-make "-j$numCPUs"
```

You'll find `tic80.exe` in `TIC-80\build\bin`.

## Linux

### Ubuntu

#### Ubuntu 22.04 (Jammy Jellyfish)
Run the following commands from a terminal:

```
# Install the latest CMake from https://apt.kitware.com
test -f /usr/share/doc/kitware-archive-keyring/copyright ||
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main' | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null
sudo apt-get update
test -f /usr/share/doc/kitware-archive-keyring/copyright ||
sudo rm /usr/share/keyrings/kitware-archive-keyring.gpg
sudo apt-get install kitware-archive-keyring

sudo apt update && sudo apt -y install build-essential cmake git libpipewire-0.3-dev libwayland-dev libsdl2-dev ruby-dev libglvnd-dev libglu1-mesa-dev freeglut3-dev libcurl4-openssl-dev
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DBUILD_SDLGPU=On -DBUILD_WITH_ALL=On .. && cmake --build . --parallel
```

Install with [Install Instructions](#install-instructions)

#### Ubuntu 24.04 (Noble Numbat)
Run the following commands from a terminal:

```
sudo apt update && sudo apt -y install build-essential cmake git libpipewire-0.3-dev libwayland-dev libsdl2-dev ruby-dev libcurl4-openssl-dev libglvnd-dev libglu1-mesa-dev freeglut3-dev
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DBUILD_SDLGPU=On -DBUILD_WITH_ALL=On -DBUILD_STATIC=On .. && cmake --build . --parallel
```

Install with [Install Instructions](#install-instructions)

### Arch
run the following commands in the Terminal
```
sudo pacman -S cmake ruby mesa libglvnd glu
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DBUILD_WITH_ALL=On .. && cmake --build . --parallel
```

Install with [Install Instructions](#install-instructions)

### Fedora

#### Fedora 36
run the following commands in the Terminal
```
sudo dnf -y groupinstall "Development Tools" "Development Libraries"
sudo dnf -y install ruby rubygem-{tk{,-doc},rake,test-unit} cmake libglvnd-devel libglvnd-gles freeglut-devel clang libXext-devel SDL_sound pipewire-devel pipewire-jack-audio-connection-kit-devel pulseaudio-libs-devel
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DCMAKE_CXX_COMPILER=clang++ -DSDL_ALSA=On -DBUILD_WITH_ALL=On ..
```

Install with [Install Instructions](#install-instructions)

#### Fedora 40
Run the following commands from a terminal:
```
sudo dnf -y groupinstall "Development Tools" "Development Libraries"
sudo dnf -y install ruby-devel rubygem-rake cmake clang pipewire-devel SDL2-devel SDL2_sound-devel SDL2_gfx-devel wayland-devel libXext-devel pipewire-jack-audio-connection-kit-devel pipewire-jack-audio-connection-kit-devel pulseaudio-libs-devel rubygems-devel libdecor-devel libdrm-devel mesa-libgbm-devel esound-devel freeglut-devel
cmake -DBUILD_SDLGPU=On -DBUILD_WITH_ALL=On ..
cmake --build . --parallel
```

Install with [Install Instructions](#install-instructions)

### openSUSE

#### openSUSE Tumbleweed / Leap 16.0
Run the following commands from a terminal:
```
sudo zypper refresh
sudo zypper install --no-confirm --type pattern devel_basis
sudo zypper install --no-confirm cmake glu-devel libXext-devel pipewire-devel libcurl-devel
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DBUILD_SDLGPU=On -DBUILD_WITH_ALL=On .. && cmake --build . --parallel
```

Install with [Install Instructions](#install-instructions)

### Raspberry Pi

#### Raspberry Pi OS (64-Bit) (Bookworm)
Run the following commands from a terminal:

```
sudo apt update && sudo apt -y install cmake libpipewire-0.3-dev libwayland-dev libsdl2-dev ruby-dev libcurl4-openssl-dev
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DBUILD_SDLGPU=On -DBUILD_WITH_ALL=On .. && cmake --build . --parallel 2
```

Install with [Install Instructions](#install-instructions)

#### Raspberry Pi (Retropie)
First, add jessie-backports repo to your `/etc/apt/sources.list`

`deb [check-valid-until=no] http://archive.debian.org/debian jessie-backports main`

Then run the following commands in the Terminal

```
# required public keys
gpg --keyserver pgpkeys.mit.edu --recv-key  8B48AD6246925553
gpg -a --export 8B48AD6246925553 | sudo apt-key add -
gpg --keyserver pgpkeys.mit.edu --recv-key 7638D0442B90D010
gpg -a --export 7638D0442B90D010 | sudo apt-key add -

# upgrade system
sudo apt-get update
sudo apt-get dist-upgrade

# install software
sudo apt-get install git build-essential ruby-full libsdl2-dev zlib1g-dev
sudo apt-get install -t jessie-backports liblua5.3-dev
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DBUILD_WITH_ALL=On ..

# install software ubuntu 22.04.3 LTS
sudo apt-get install git build-essential ruby-full libsdl2-dev zlib1g-dev
sudo apt-get install liblua5.3-dev
sudo apt-get install libcurl4-openssl-dev
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DBUILD_WITH_ALL=On ..
```

Install with [Install Instructions](#install-instructions)

_Note:_ If you are using a normal Raspberry Pi image (not Retropie) you may not
have OpenGL drivers enabled. Run `sudo raspi-config`, then select 7
for "Advanced Options", followed by 6 for "GL Drivers", and enable "GL
(Fake KMS) Desktop Driver". After changing this setting, reboot.

## Mac
install `Command Line Tools for Xcode` and `brew` package manager

run the following commands in the Terminal
```
brew install git cmake
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DBUILD_WITH_ALL=On ..
make -j4
```

to create application icon for development version
```
mkdir -p ~/Applications/TIC80dev.app/Contents/{MacOS,Resources}
cp -f macosx/tic80.plist ~/Applications/TIC80dev.app/Contents/Info.plist
cp -f macosx/tic80.icns ~/Applications/TIC80dev.app/Contents/Resources
cat > ~/Applications/TIC80dev.app/Contents/MacOS/tic80 <<EOF
#!/bin/sh
exec /Users/nesbox/projects/TIC-80/build/bin/tic80 --skip --scale 2 >/dev/null
EOF
chmod +x ~/Applications/TIC80dev.app/Contents/MacOS/TIC80dev
```
Make sure to update the absolute path to the tic80 binary in the script, or
update the launch arguments.

## FreeBSD
run the following commands in the Terminal
```
sudo pkg install gcc git cmake ruby libglvnd libglu freeglut mesa-devel mesa-dri alsa-lib
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -DBUILD_WITH_ALL=On ..
make -j4
```

Mesa looks for swrast_dri.so from the wrong path, so also symlink it:

```
sudo ln -s /usr/local/lib/dri/swrast_dri.so /usr/local/lib/dri-devel/
```

# Install instructions

## Linux
- To make an executable file `./tic80` without installation run `make` and locate the output in `TIC-80/build/bin`.
- To install system-wide run `sudo make install`
- You can append `-j4` if you have a modern system, or `-j2` for a Raspberry Pi to speed up the process.

TIC-80 can now be run with `tic80` (if installed) or `./tic80` (with no installation).

## iOS / tvOS
You can find iOS/tvOS version here
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
* Andrei Rudenko — [GitHub @RudenkoArts](https://github.com/RudenkoArts)
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
