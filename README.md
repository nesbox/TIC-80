# TIC-80 issues tracker and wiki

[Discord chat](https://discord.gg/DkD73dP)

This is the official issues tracker of <https://tic.computer>. If you are experiencing a bug or would like to see a new feature [browse existing issues](https://github.com/nesbox/tic.computer/issues) or [create a new one](https://github.com/nesbox/tic.computer/issues/new).

Documentation is available in the [wiki](https://github.com/nesbox/tic.computer/wiki).

Thanks!

# Build instructions

## Windows
### with Visual Studio 2015
- install Visual Studio 2015
- install GIT
- run following commands in `cmd`
```
mkdir tic
cd tic
git clone https://github.com/nesbox/3rd-party
git clone https://github.com/nesbox/TIC-80
```
- open `TIC-80\build\windows\tic\tic.sln` and build
- enjoy :)

### with MinGW32
follow the instructions in the tutorial https://matheuslessarodrigues.github.io/tic80-build-tutorial/
made by [@matheuslessarodrigues](https://github.com/matheuslessarodrigues)

## Using CMAKE 
### Linux
run the following commands in the Terminal
```
sudo apt-get install git cmake build-essential libgtk-3-dev emscripten
git clone https://github.com/nesbox/TIC-80
cd TIC-80
mkdir build_
cd build_
cmake -DEMDIR=/usr/bin ..
make 

- get coffee.
```

### with Visual Studio 2017CE (or others)

* clone https://github.com/nesbox/TIC-80
* start cmake & choose your generator.
* point towards the source 
* configure and watch the errors come by
* set EMDIR to your emscripten folder which has emcc emmake etc. in it.
* configure again and hopefully no real errors should now occur
* start VS, load the project and hack away

## iOS / tvOS
You can find iOS/tvOS version here https://github.com/CliffsDover/TIC-80

