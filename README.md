[![Build Status](https://travis-ci.org/nesbox/TIC-80.svg?branch=master)](https://travis-ci.org/nesbox/TIC-80)

![TIC-80](https://tic.computer/img/logo64.png)
**TIC-80 TINY COMPUTER** - [https://tic.computer/](https://tic.computer/)

# About
TIC-80 is a **FREE** and **OPEN SOURCE** fantasy computer for making, playing and sharing tiny games.

With TIC-80 you get built-in tools for development: code, sprites, maps, sound editors and the command line, which is enough to create a mini retro game.

Games are packeged into a cartridge file, which can be easily distributed. TIC-80 works on all popular platforms. This means your cartridge can be played in any device.

To make a retro styled game, the whole process of creation and execution takes place under some technical limitations: 240x136 pixels display, 16 color palette, 256 8x8 color sprites, 4 channel sound and etc.

![TIC-80](https://user-images.githubusercontent.com/1101448/29687467-3ddc432e-8925-11e7-8156-5cec3700cc04.gif)

### Features
- Multiple progamming languages: Lua, Moonscript, Javascript and Wren
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
### with Visual Studio 2015
- install Visual Studio 2015
- install GIT
- run following commands in `cmd`
```
git clone --recursive https://github.com/nesbox/TIC-80
```
- open `TIC-80\build\windows\tic\tic.sln` and build
- enjoy :)

### with MinGW32
follow the instructions in the tutorial https://matheuslessarodrigues.github.io/tic80-build-tutorial/
made by [@matheuslessarodrigues](https://github.com/matheuslessarodrigues)

## Linux
run the following commands in the Terminal
```
sudo apt-get install git 
git clone --recursive https://github.com/nesbox/TIC-80
cd TIC-80
./build.sh
```

## iOS / tvOS
You can find iOS/tvOS version here https://github.com/CliffsDover/TIC-80
