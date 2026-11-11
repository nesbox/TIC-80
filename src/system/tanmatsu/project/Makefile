# Device parameters
DEVICE ?= tanmatsu
# The AppFS slug TIC-80 is published under. Script apps name it as their
# interpreter, and installing under any other slug adds a second copy that
# AppFS has no room for.
SLUG ?= com.annejan.tic80

# Build parameters
IDF_VERSION ?= v6.0.2
BUILD ?= build/$(DEVICE)
FAT ?= 0
SDKCONFIG_DEFAULTS ?= sdkconfigs/general;sdkconfigs/$(DEVICE)
SDKCONFIG ?= sdkconfig_$(DEVICE)

# SDK
IDF_PATH ?= $(shell cat .IDF_PATH 2>/dev/null || test -d `pwd`/esp-idf && echo `pwd`/esp-idf || echo '$(HOME)/.espressif/$(IDF_VERSION)/esp-idf')
IDF_TOOLS_PATH ?= $(shell cat .IDF_TOOLS_PATH 2>/dev/null || test -d `pwd`/esp-idf-tools && echo `pwd`/esp-idf-tools || echo '$(HOME)/.espressif/tools')
IDF_SOURCE ?= $(shell cat .IDF_PATH 2>/dev/null && echo '$(IDF_PATH)/export.sh' || test -d `pwd`/esp-idf && echo '$(IDF_PATH)/export.sh' || echo '$(HOME)/.espressif/tools/activate_idf_$(IDF_VERSION).sh')
IDF_EXPORT_QUIET ?= 1
IDF_GITHUB_ASSETS ?= dl.espressif.com/github_assets
IDF_INSTALL_PATH ?= $(shell echo `pwd`/esp-idf)
IDF_INSTALL_TOOLS_PATH ?= $(shell echo `pwd`/esp-idf-tools)

MAKEFLAGS += --silent
SHELL := /usr/bin/env bash


####

# Set IDF_TARGET based on device name

ifeq ($(DEVICE), tanmatsu)
IDF_TARGET ?= esp32p4
else ifeq ($(DEVICE), mch2022)
IDF_TARGET ?= esp32
else ifeq ($(DEVICE), kami)
IDF_TARGET ?= esp32
else ifeq ($(DEVICE), hackerhotel-2024)
IDF_TARGET ?= esp32c6
else ifeq ($(DEVICE), hackaday2025)
IDF_TARGET ?= esp32s3
else ifeq ($(DEVICE), heltecv3)
IDF_TARGET ?= esp32s3
else ifeq ($(DEVICE), esp32-p4-function-ev-board)
IDF_TARGET ?= esp32p4
else ifeq ($(DEVICE), esp32-s31-korvo-1)
IDF_TARGET ?= esp32s31
else
$(warning "Unknown device, defaulting to ESP32")
IDF_TARGET ?= esp32
endif

IDF_PARAMS := -B $(BUILD) -DDEVICE=$(DEVICE) -DSDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" -DSDKCONFIG=$(SDKCONFIG) -DIDF_TARGET=$(IDF_TARGET) -DFAT=$(FAT)

#####

export IDF_TOOLS_PATH
export IDF_GITHUB_ASSETS

# General targets

.PHONY: all
all: build

# Badgelink
.PHONY: badgelink
badgelink:
	rm -rf badgelink
	git clone https://github.com/badgeteam/esp32-component-badgelink.git badgelink
	cd badgelink/tools; ./install.sh

.PHONY: install
install: build
install:
	cd badgelink/tools; ./badgelink.sh appfs upload $(SLUG) "TIC-80" 0 ../../$(BUILD)/tic80.bin

.PHONY: run
run:
	cd badgelink/tools; ./badgelink.sh start $(SLUG)

# Preparation

.PHONY: prepare
prepare: sdk

.PHONY: submodules
submodules: 
	if [ ! -f .submodules_update_done ]; then \
		echo "Updating submodules"; \
		git submodule update --init --recursive; \
		touch .submodules_update_done; \
	fi

.PHONY: sdk
sdk:
	if test -d "$(IDF_INSTALL_PATH)"; then echo -e "ESP-IDF target folder exists!\r\nPlease remove the folder or un-set the environment variable."; exit 1; fi
	if test -d "$(IDF_INSTALL_TOOLS_PATH)"; then echo -e "ESP-IDF tools target folder exists!\r\nPlease remove the folder or un-set the environment variable."; exit 1; fi
	git clone --recursive --branch "$(IDF_VERSION)" https://github.com/espressif/esp-idf.git "$(IDF_INSTALL_PATH)" --depth=1 --shallow-submodules
	cd "$(IDF_INSTALL_PATH)"; git submodule update --init --recursive
	cd "$(IDF_INSTALL_PATH)"; bash install.sh all

.PHONY: eim-sdk
eim-sdk:
	eim install --do-not-track true -i $(IDF_VERSION)

.PHONY: check-sdk
check-sdk:
	@if test -d $(IDF_PATH); then \
		printf '%s\n' "ESP-IDF $(IDF_VERSION) found!"; \
	else \
		printf 'ESP-IDF SDK not found. %s\n' "Please install ESP-IDF $(IDF_VERSION), either by running 'make prepare' (installs to this folder) or 'make eim-sdk' (installs using Espressif installation manager) or if manually installed set the IDF_PATH and IDF_TOOLS_PATH environment variables or create files .IDF_PATH and .IDF_TOOLS_PATH in this folder containing the paths." >&2; \
		exit 1; \
	fi

.PHONY: print-sdk
print-sdk:
	echo "ESP-IDF path:               $(IDF_PATH)"
	echo "ESP-IDF tools:              $(IDF_TOOLS_PATH)"
	echo "ESP-IDF source command:     $(IDF_SOURCE)"
	echo "ESP-IDF installation path:  $(IDF_INSTALL_PATH)"
	echo "ESP-IDF installation tools: $(IDF_INSTALL_TOOLS_PATH)"

.PHONY: reinstallsdk
reinstallsdk:
	cd "$(IDF_PATH)"; bash install.sh all

.PHONY: removesdk
removesdk:
	rm -rf "$(IDF_PATH)"
	rm -rf "$(IDF_TOOLS_PATH)"

.PHONY: refreshsdk
refreshsdk: removesdk sdk

.PHONY: menuconfig
menuconfig:
	source "$(IDF_SOURCE)" && idf.py menuconfig -DDEVICE=$(DEVICE) -DSDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" -DSDKCONFIG=$(SDKCONFIG) -DIDF_TARGET=$(IDF_TARGET)
	
# Cleaning

.PHONY: clean
clean:
	rm -rf $(BUILD)
	rm -f .submodules_update_done
	rm -rf managed_components
	rm -f sdkconfig_*

.PHONY: fullclean
fullclean: clean
	rm -rf build
	rm -f sdkconfig_*
	rm -f sdkconfig
	rm -f sdkconfig.old
	rm -f sdkconfig.ci
	rm -f sdkconfig.defaults

# Check if build environment is set up correctly
.PHONY: checkbuildenv
checkbuildenv:
	if [ -z "$(IDF_PATH)" ]; then echo "IDF_PATH is not set!"; exit 1; fi
	if [ -z "$(IDF_TOOLS_PATH)" ]; then echo "IDF_TOOLS_PATH is not set!"; exit 1; fi

# Building

.PHONY: build
build: check-sdk icons checkbuildenv
	source "$(IDF_SOURCE)" >/dev/null && idf.py $(IDF_PARAMS) build

.PHONY: reconfigure
reconfigure: check-sdk checkbuildenv
	source "$(IDF_SOURCE)" >/dev/null && idf.py $(IDF_PARAMS) reconfigure

# Hardware
#
# No flash, monitor or erase targets: the USB serial port a Tanmatsu shows is
# the ESP32-C6 radio coprocessor, not the P4 that runs TIC-80, and flashing
# would replace the launcher. The app goes in through badgelink (install, run).
# Debugging the P4 takes an external FTDI JTAG adapter (openocdftdi, gdb).

.PHONY: prepappfs
prepappfs:
	source "$(IDF_SOURCE)" && \
	python3 managed_components/badgeteam__appfs/tools/appfs_generate.py \
	8192000 \
	appfs.bin

.PHONY: openocdftdi
openocdftdi:
	source "$(IDF_SOURCE)" && idf.py $(IDF_PARAMS) openocd --openocd-commands "-f board/esp32p4-ftdi.cfg"

.PHONY: gdb
gdb:
	source "$(IDF_SOURCE)" && idf.py $(IDF_PARAMS) gdb

.PHONY: gdbgui
gdbgui:
	source "$(IDF_SOURCE)" && idf.py $(IDF_PARAMS) gdbgui

.PHONY: gdbtui
gdbtui:
	source "$(IDF_SOURCE)" && idf.py $(IDF_PARAMS) gdbtui

# Tools

.PHONY: size
size:
	source "$(IDF_SOURCE)" && idf.py $(IDF_PARAMS) size

.PHONY: size-components
size-components:
	source "$(IDF_SOURCE)" && idf.py $(IDF_PARAMS) size-components

.PHONY: size-files
size-files:
	source "$(IDF_SOURCE)" && idf.py $(IDF_PARAMS) size-files

.PHONY: efuse
efuse:
	$(IDF_PATH)/components/efuse/efuse_table_gen.py --idf_target esp32p4 $(IDF_PATH)/components/efuse/esp32p4/esp_efuse_table.csv main/esp_efuse_custom_table.csv

# Formatting

.PHONY: format
format:
	find main/ -iname '*.h' -o -iname '*.c' -o -iname '*.cpp' | xargs clang-format -i

# Re-compile protobuf files
# If you are an end user, you do not need to run this;
# the output files are already there in the repository.

.PHONY: compile-protobuf
compile-protobuf:
	protoc --pyi_out=tools --python_out=tools badgelink.proto
	python3 main/badgelink/nanopb/generator/nanopb_generator.py -D main/badgelink -f badgelink.options badgelink.proto

# Take all svg files from main/static/icons and put them in main/fat/icons as png using tools/connvert.sh
ICONS_SRC := $(wildcard main/static/icons/*.svg)
ICONS_DST := $(patsubst main/static/icons/%.svg,main/fat/icons/%.png,$(ICONS_SRC))

.PHONY: icons
icons: $(ICONS_DST)

main/fat/icons/%.png: main/static/icons/%.svg
	mkdir -p main/fat/icons
	tools/convert.sh $< $@
	
# Build all targets
.PHONY: buildall
buildall:
	$(MAKE) build DEVICE=tanmatsu
	$(MAKE) build DEVICE=esp32-p4-function-ev-board
	$(MAKE) build DEVICE=mch2022
	$(MAKE) build DEVICE=hackaday2025
	$(MAKE) build DEVICE=hackerhotel-2024
	$(MAKE) build DEVICE=heltecv3
	$(MAKE) build DEVICE=kami

# Vscode
.PHONY: vscode
vscode:
	rm -rf .vscode
	cp -r .vscode.template .vscode
