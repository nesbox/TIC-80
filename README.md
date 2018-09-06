[![Build Status](https://travis-ci.org/nesbox/TIC-80.svg?branch=master)](https://travis-ci.org/nesbox/TIC-80)
[![Build status](https://ci.appveyor.com/api/projects/status/1pflw77cjd8mqggb/branch/master?svg=true)](https://ci.appveyor.com/project/nesbox/tic-80)

![TIC-80](https://tic.computer/img/logo64.png)
**TIC-80 TINY COMPUTER** - [https://tic.computer/](https://tic.computer/)

# About
TIC-80 is a **FREE** and **OPEN SOURCE** fantasy computer for making, playing and sharing tiny games.

With TIC-80 you get built-in tools for development: code, sprites, maps, sound editors and the command line, which is enough to create a mini retro game.

Games are packeged into a cartridge file, which can be easily distributed. TIC-80 works on all popular platforms. This means your cartridge can be played in any device.

To make a retro styled game, the whole process of creation and execution takes place under some technical limitations: 240x136 pixels display, 16 color palette, 256 8x8 color sprites, 4 channel sound and etc.

![TIC-80](https://user-images.githubusercontent.com/1101448/29687467-3ddc432e-8925-11e7-8156-5cec3700cc04.gif)

### Features
- Multiple progamming languages: [Lua](https://www.lua.org),
  [Moonscript](https://moonscript.org),
  [Javascript](https://developer.mozilla.org/en-US/docs/Web/JavaScript),
  [Wren](http://wren.io/), and [Fennel](https://fennel-lang.org).
- Games can have mouse and keyboard as input
- Games can have up to 4 controllers as input
- Builtin editors: for code, sprites, world maps, sound effects and music
- An aditional memory bank: load different assets from your cartridge while your game is executing

# Binaries Downloads
You can download compiled versions for the major operating systems directly from our [releases page](https://github.com/nesbox/TIC-80/releases).

# Pro Version
To help supporting the TIC-80 development, we have a [PRO Version](https://nesbox.itch.io/tic).
This version has a few aditional features and can only be download on [our Itch.io page](https://nesbox.itch.io/tic).

For users who can't spend the money, we made it easy to build the pro version from the source code.

### Pro features

- Save/load cartridges in text format, and create your game in any editor you want, also useful for version control systems.
- Even more memory banks: instead of having only 1 memory bank you have 8.
- Export your game only without editors, and then publish it to app stores (WIP).

# Community
You can play and share games, tools and music at [tic.computer](https://tic.computer/play).

The community also hangs and discuss on [Discord chat](https://discord.gg/DkD73dP).

# Contributing
You are can contribute by issuing a bug or requesting a new feature on our [issues page](https://github.com/nesbox/tic.computer/issues).
Keep in mind when engaging on a discussion to follow our [Code of Conduct](https://github.com/nesbox/TIC-80/blob/master/CODE_OF_CONDUCT.md).

You can also contribute by reviewing or improving our [wiki](https://github.com/nesbox/tic.computer/wiki).
The [wiki](https://github.com/nesbox/tic.computer/wiki) holds TIC-80 documentation, code snippets and game development tutorials.

# Build instructions

## Windows
### with Visual Studio 2017
- install `Visual Studio 2017`
- install `git`
- run following commands in `cmd`
```
git clone --recursive https://github.com/nesbox/TIC-80
cmake -G "Visual Studio 15 2017 Win64"
```
- open `TIC-80.sln` and build
- enjoy :)

### with MinGW
- install `mingw-w64` (http://mingw-w64.org) and add `.../mingw/bin` path to the *System Variables Path*
- install `git`
- install `cmake` (https://cmake.org)
- run following commands in `terminal`
```
git clone --recursive https://github.com/nesbox/TIC-80
cd TIC-80
cmake -G "MinGW Makefiles"
mingw32-make -j4
```

## Linux (Ubuntu 14.04)
run the following commands in the Terminal
```
sudo apt-get install git cmake libgtk-3-dev libgles1-mesa-dev libglu-dev -y
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80
cmake . && make -j4
```

to install the latest CMake:
```
wget "https://cmake.org/files/v3.12/cmake-3.12.0-Linux-x86_64.sh"
sudo sh cmake-3.12.0-Linux-x86_64.sh --skip-license --prefix=/usr
```

## Mac
install `Command Line Tools for Xcode` and `brew` package manager

run the following commands in the Terminal
```
brew install git cmake
git clone --recursive https://github.com/nesbox/TIC-80
cd TIC-80
cmake . && make -j4
```

## iOS / tvOS
You can find iOS/tvOS version here 
- 0.60.3: https://github.com/brunophilipe/TIC-80
- 0.45.0: https://github.com/CliffsDover/TIC-80
