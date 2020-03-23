
# Raspberry PI Baremetal build

The following explains how to build TIC-80 for the Raspberry PI boards in baremetal mode, that is, without an operating system (the board boots directly in TIC-80)

# Requirements

You need:

- A Linux machine
- gcc ARM toolchain. You can get it [here](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads). Successfully tested with version: gcc-arm-none-eabi-8-2018-q4-major
- A standard building environment (make, cmake, gcc, wget, etc)

# Building instructions

Following are all the building steps for building the system.

First, set the path to include the arm toolkit (fix the command with your path):

```
PATH=/home/you/gcc-arm-none-eabi-8-2018-q4-major/bin/:$PATH
```

Get a fresh copy of TIC-80 repository:

```
git clone --recursive https://github.com/nesbox/TIC-80
cd TIC-80
```

Clone circle/circle-stdlib:

```
cd vendor
git clone --recursive https://github.com/smuehlst/circle-stdlib.git
```

Now you have to edit file in circle-stdlib/libs/circle/include/circle/sysconfig.h and uncomment the following define to enable it:
```
#define USE_USB_SOF_INTR
```

Now build circle-stdlib:

```
cd circle-stdlib
./configure -r 3
make
```

Make some addon that are not compiled automatically:

```
cd libs/circle/addon/vc4/sound/
make
cd ../vchiq
make
cd ../../linux
make
cd ../fatfs
make
cd ../../../../../..
```

Build `tic80lib` for arm with baremetal customizations:

```
cd build
cmake -DCMAKE_BUILD_TYPE=MinSizeRel -DBAREMETALPI=1 ..
make tic80lib
```

Build the kernel:

```
cd baremetalpi
make
```

This generates the final `kernel8-32.img` file. Copy it into your SD card root.

Now you have to prepare some bootup files that need to be copied to the SD card root together with your kernel8-32.img. This only need to be done once:

```
cd boot
make
```

Now copy the following files to your SD card

- `config.txt`
- `LICENCE.broadcom`
- `bootcode.bin`
- `fixup.dat`
- `start.elf`

You need to create a `tic80` folder into your SD card. Your carts go in there.

# Thanks

This project is built on two awesome projects, [circle](https://github.com/rsta2/circle) and [circle-stdlib](https://github.com/smuehlst/circle-stdlib). Without them, this version of TIC-80 would not exists.

