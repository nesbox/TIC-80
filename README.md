[![Build Status](https://github.com/nesbox/TIC-80/workflows/Build/badge.svg)](https://github.com/nesbox/TIC-80/actions?query=workflow%3ABuild)

![TIC-80](https://tic.computer/img/logo64.png)
**TIC-80 TINY COMPUTER** - [https://tic80.com](https://tic80.com)

# About
TIC-80 is a **FREE** and **OPEN SOURCE** fantasy computer for making, playing and sharing tiny games.

With TIC-80 you get built-in tools for development: code, sprites, maps, sound editors and the command line, which is enough to create a mini retro game.

Games are packaged into a cartridge file, which can be easily distributed. TIC-80 works on all popular platforms. This means your cartridge can be played in any device.

To make a retro styled game, the whole process of creation and execution takes place under some technical limitations: 240x136 pixel display, 16 color palette, 256 8x8 color sprites, 4 channel sound, etc.

![TIC-80](https://user-images.githubusercontent.com/1101448/92492270-d6bcbc80-f1fb-11ea-9d2d-468ad015ace2.gif)

### Features
- Multiple programming languages: [Lua](https://www.lua.org),
  [Moonscript](https://moonscript.org),
  [Javascript](https://developer.mozilla.org/en-US/docs/Web/JavaScript),
  [Wren](http://wren.io/), [Fennel](https://fennel-lang.org), and
  [Squirrel](http://www.squirrel-lang.org).
- Games can have mouse and keyboard as input
- Games can have up to 4 controllers as input (with up to 8 buttons, each)
- Built-in editors: for code, sprites, world maps, sound effects and music
- An aditional memory bank: load different assets from your cartridge while your game is executing

# Binary Downloads
You can download compiled versions for the major operating systems directly from our [releases page](https://github.com/nesbox/TIC-80/releases).

# Pro Version
To help support TIC-80 development, we have a [PRO Version](https://nesbox.itch.io/tic80).
This version has a few additional features and binaries can only be downloaded on [our Itch.io page](https://nesbox.itch.io/tic80).

For users who can't spend the money, we made it easy to build the pro version from the source code. (`cmake .. -DBUILD_PRO=On`)

### Pro features

- Save/load cartridges in text format, and create your game in any editor you want, also useful for version control systems.
- Even more memory banks: instead of having only 1 memory bank you have 8.
- Export your game without editors, and then publish it to app stores (WIP).

# Community
You can play and share games, tools and music at [https://tic80.com/play](https://tic80.com/play).

The community also hangs out and discusses on [Telegram](https://t.me/tic80) or [Discord](https://discord.gg/DkD73dP).

# Contributing
You can contribute by issuing a bug or requesting a new feature on our [issues page](https://github.com/nesbox/tic.computer/issues).
Keep in mind when engaging on a discussion to follow our [Code of Conduct](https://github.com/nesbox/TIC-80/blob/master/CODE_OF_CONDUCT.md).

You can also contribute by reviewing or improving our [wiki](https://github.com/nesbox/TIC-80/wiki).
The [wiki](https://github.com/nesbox/TIC-80/wiki) holds TIC-80 documentation, code snippets and game development tutorials.

# Build instructions

## Windows
### with Visual Studio 2017
- install `Visual Studio 2017`
- install `git`
- run following commands in `cmd`
```
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -G "Visual Studio 15 2017 Win64" ..
```
- open `TIC-80.sln` and build
- enjoy :)

### with MinGW
- install `mingw-w64` (http://mingw-w64.org) and add `.../mingw/bin` path to the *System Variables Path*
- install `git`
- install `cmake` (https://cmake.org)
- run following commands in `terminal`
```
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake -G "MinGW Makefiles" ..
mingw32-make -j4
```

## Linux 
### Ubuntu 14.04
run the following commands in the Terminal
```
sudo apt-get install git cmake libgtk-3-dev libgles1-mesa-dev libglu-dev -y
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake ..
make -j4
```

to install the latest CMake:
```
wget "https://cmake.org/files/v3.12/cmake-3.12.0-Linux-x86_64.sh"
sudo sh cmake-3.12.0-Linux-x86_64.sh --skip-license --prefix=/usr
```

### Ubuntu 18.04

run the following commands in the Terminal
```
sudo apt-get install g++ git cmake libgtk-3-dev libglvnd-dev libglu1-mesa-dev freeglut3-dev libasound2-dev -y
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake ..
make -j4
```

### Raspberry Pi (Retropie)

First, add jessie-backports repo to your `/etc/apt/sources.list`

`deb http://ftp.debian.org/debian jessie-backports main`  

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
sudo apt-get install git build-essential libgtk-3-dev libsdl2-dev zlib1g-dev
sudo apt-get install -t jessie-backports liblua5.3-dev
git clone --recursive https://github.com/nesbox/TIC-80 && cd TIC-80/build
cmake ..
make -j4
```

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
cmake ..
make -j4
```

## iOS / tvOS
You can find iOS/tvOS version here 
- 0.60.3: https://github.com/brunophilipe/TIC-80
- 0.45.0: https://github.com/CliffsDover/TIC-80

## Credits

* Filippo Rivato - [Twitter @HomineLudens](https://twitter.com/HomineLudens)  
* Fred Bednarski - [Twitter @FredBednarski](https://twitter.com/FredBednarski)  
* Al Rado - [Twitter @alrado2](https://twitter.com/alrado2)  
* Trevor Martin - [Twitter @trelemar](https://twitter.com/trelemar)  
* MonstersGoBoom - [Twitter @MonstersGoBoom](https://twitter.com/MonstersGo)  
* Matheus Lessa - [Twitter @matheuslrod](https://twitter.com/matheuslrod)  
* CliffsDover - [Twitter @DancingBottle](https://twitter.com/DancingBottle)  
* Frantisek Jahoda - [GitHub @jahodfra](https://github.com/jahodfra)  
* Guilherme Medeiros - [GitHub @frenetic](https://github.com/frenetic)  
* Andrei Rudenko - [GitHub @RudenkoArts](https://github.com/RudenkoArts)  
* Phil Hagelberg - [@technomancy](https://technomancy.us/colophon)  
* Rob Loach - [Twitter @RobLoach](https://twitter.com/RobLoach) [GitHub @RobLoach](https://github.com/RobLoach)
* Wade Brainerd - [GitHub @wadetb](https://github.com/wadetb)
* Paul Robinson - [GitHub @paul59](https://github.com/paul59)
* Stefan Devai - [GitHub @stefandevai](https://github.com/stefandevai) [Blog stefandevai.me](https://stefandevai.me)
* Damien de Lemeny - [GitHub @ddelemeny](https://github.com/ddelemeny)
* Adrian Siekierka - [GitHub @asiekierka](https://github.com/asiekierka) [Website](https://asie.pl/)
* Jay Em (Sweetie16 palette) - [Twitter @GrafxKid](https://twitter.com/GrafxKid)  
* msx80 - [Twitter @msx80](https://twitter.com/msx80) [Github msx80](https://github.com/msx80)
* Josh Goebel - [Twitter @dreamer3](https://twitter.com/dreamer3) [Github joshgoebel](https://github.com/joshgoebel)
