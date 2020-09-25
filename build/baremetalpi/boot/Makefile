#
# Download the firmware files to be required for boot (requires wget)
#
# These files must be copied along with the generated kernel.img
#     onto a SD(HC) card with FAT file system.
#

firmware:
	wget -q -O LICENCE.broadcom https://github.com/raspberrypi/firmware/blob/master/boot/LICENCE.broadcom?raw=true
	wget -q -O bootcode.bin https://github.com/raspberrypi/firmware/blob/master/boot/bootcode.bin?raw=true
	wget -q -O fixup.dat https://github.com/raspberrypi/firmware/blob/master/boot/fixup.dat?raw=true
	wget -q -O start.elf https://github.com/raspberrypi/firmware/blob/master/boot/start.elf?raw=true

all: firmware
