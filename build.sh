#!/usr/bin/env bash

##########################################################################
# build script for TIC-80
##########################################################################
echo "For a normal build, use the linux target."
echo "-lto targets (Link Time Optimization) have smaller output size."
echo "wasm, emscripten and mingw targets require their compilers to be installed."
PS3="Select build target: "
options=("dependencies" "linux" "linux32-lto" "linux64-lto" "emscripten" "wasm" "mingw" "chip-lto" "exit")
select opt in "${options[@]}"
do
  case $opt in
    "install packages")
      sudo apt-get install build-essential mercurial make cmake autoconf automake libtool libasound2-dev libpulse-dev libaudio-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev libxxf86vm-dev libxss-dev libgl1-mesa-dev libesd0-dev libdbus-1-dev libudev-dev libgles1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev libibus-1.0-dev fcitx-libs-dev libsamplerate0-dev libgtk-3-dev zlib1g-dev libwayland-dev libxkbcommon-dev wayland-protocols libsndio-dev -y
      ;;
    "linux")
      make linux
      ;;
    "linux32-lto")
      make linux32-lto
      ;;
    "linux64-lto")
      make linux64-lto
      ;;
    "emscripten")
      make emscripten
      ;;
    "wasm")
      make wasm
      ;;
    "mingw")
      make mingw
      ;;
    "chip-lto")
      make chip-lto
      ;;
    "exit")
      break
      ;;
    *) echo Invalid target;;
  esac
done
