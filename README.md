# TIC-80 issues tracker and wiki

[Discord chat](https://discord.gg/DkD73dP)

This is the official issues tracker of <https://tic.computer>. If you are experiencing a bug or would like to see a new feature [browse existing issues](https://github.com/nesbox/tic.computer/issues) or [create a new one](https://github.com/nesbox/tic.computer/issues/new).

Documentation is available in the [wiki](https://github.com/nesbox/tic.computer/wiki).

Thanks!

# Building instructions

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

## Linux
run the following commands in the Terminal
```
sudo apt-get install git build-essential libgtk-3-dev
git clone https://github.com/nesbox/TIC-80
cd TIC-80
make linux32 (or linux64/arm depending on your system)
```
