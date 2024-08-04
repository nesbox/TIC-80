# Package

version       = "0.1.0"
author        = "archargelod"
description   = "tic80 wasm template for Nim language"
license       = "MIT"
srcDir        = "src"


# Dependencies

requires "nim >= 2.0.0"

# Tasks
import std/strformat

let pwd = getCurrentDir()

task wasmbuild, "Build wasm binary (debug)":
  exec("nim c -o:cart.wasm src/cart")

task wasmrelease, "Build wasm binary (release)":
  exec("nim c -d:release -o:cart.wasm src/cart")

task buildcart, "Build optimized wasm binary and import it to tic80 cart":
  rmFile("cart.tic")
  exec("nim c -d:release -o:cart.wasm src/cart")
  exec(&"tic80 --cli --fs=\"{pwd}\" --cmd=\"load src/cart.tic & import binary cart.wasm & save cart.tic & exit\"")

task runcart, "Build optimized wasm binary and run it with tic80":
  rmFile("cart.tic")
  exec("nim c -d:release -o:cart.wasm src/cart")
  exec(&"tic80 --skip --fs=\"{pwd}\" --cmd=\"load src/cart.tic & import binary cart.wasm & save cart.tic & run\"")

task debugcart, "Build wasm binary in debug mode and run it with tic80":
  rmFile("cart.tic")
  exec("nim c -o:cart.wasm src/cart")
  exec(&"tic80 --skip --fs=\"{pwd}\" --cmd=\"load src/cart.tic & import binary cart.wasm & save cart.tic & run\"")
