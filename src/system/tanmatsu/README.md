# TIC-80 for Tanmatsu

TIC-80 running natively on the [Tanmatsu](https://nicolaielectronics.nl/tanmatsu/),
an ESP32-P4 palmtop with a QWERTY keyboard. The full studio is included:
console, code editor, sprite, map, SFX and music editors, and the surf browser.
[CHANGELOG.md](CHANGELOG.md) lists what changed in each release.

## Hardware it uses

| Part | Driven through |
|------|----------------|
| 480x800 MIPI DSI panel (ST7701) | PPA scale-rotate-mirror straight into the panel framebuffer |
| Keyboard (TCA8418) | `bsp_input_get_queue()`, scancode and navigation events |
| ES8156 audio codec | `bsp_audio_*` plus a direct I2S write, 44100 Hz 16 bit stereo |
| SD card and internal FAT | ESP-IDF VFS, mounted at `/sd` and `/int` |
| WiFi (ESP32-C6 over ESP-Hosted) | `wifi-manager`, reusing the networks the launcher stored |
| USB HID on the USB-C port | `badgeteam/usb-input` for the host and the devices, `badgeteam/hid-host` for the reports |

TIC-80 renders 256x144 pixels including the border. The panel is portrait and
the BSP reports a default rotation of 270 degrees, so the picture is turned a
quarter turn on the way into the framebuffer and ends up as 432x768 at 24,16,
with the rest painted black once at startup. All four rotations are handled:
one TIC-80 row or column always becomes one panel row, which keeps the scaling
a memcpy.

Both the scaling and the rotation are done by the **PPA**, the ESP32-P4's pixel
processing accelerator, reading TIC-80's buffer and writing the panel's own
framebuffer in one DMA pass. The CPU touches no pixels at all. TIC-80 is asked
for `BGRA8888` for exactly this reason: in memory that is B,G,R,A, which is
what the PPA calls ARGB8888, so its buffer is fed to the hardware untouched.

There are two fallbacks behind that, used if the PPA client cannot be created
or the panel framebuffer cannot be reached: the same rotation and scaling on
the CPU straight into the panel framebuffer, and failing that a scratch buffer
pushed out with `bsp_display_blit()`. Two things about that call are worth
knowing, because its header documents neither: it takes **end coordinates**,
not a width and a height, and it waits on a flush semaphore that a rejected
blit never releases, so a single bad call costs a full second on the next one.

## Building

Needs ESP-IDF v6.0.2 and the ESP32-P4 toolchain.

```sh
cd src/system/tanmatsu/project
make prepare        # only once: clones ESP-IDF into ./esp-idf
make build
make badgelink      # only once: fetches the badgelink tool
make install        # uploads into AppFS over USB
make run
```

If ESP-IDF is already installed elsewhere, point at it instead of running
`make prepare`:

```sh
export IDF_PATH=/path/to/esp-idf
export IDF_TOOLS_PATH=$HOME/.espressif
export IDF_SOURCE="$IDF_PATH/export.sh"
```

The `.IDF_PATH` and `.IDF_TOOLS_PATH` files the badge.team Makefile also looks
for do not work: it appends its own path to whatever they contain, whether or
not they exist, and the result is two paths in one variable.

The build produces `build/tanmatsu/tic80.bin`, around 1.8 MB, which goes into
the 8 MB AppFS partition next to the other apps.

## Controls

The keyboard works as you would expect in the editors. Since the Tanmatsu has
no pointing device and half of TIC-80 is mouse driven, the **Fn** key turns the
arrow cluster into a pointer:

| Keys | What happens |
|------|--------------|
| Fn + arrows | Move the pointer; it accelerates while held |
| Fn + left space | Left click |
| Fn + right space | Right click |
| Fn + shift + up/down | Scroll wheel |
| Fn + middle space | Latch pointer mode, so Fn need not be held |
| Fn + Esc | Leave TIC-80 and go back to the launcher |
| Volume up/down | Codec volume, separate from TIC-80's own volume setting |

Gamepad input follows TIC-80's usual keyboard mapping: arrows for the d-pad,
`Z` `X` `A` `S` for A, B, X and Y.

### USB

A keyboard, a mouse or a gamepad plugged into the USB-C port joins in, on top
of the built-in keyboard rather than instead of it. Powering the port means
turning the boost converter on, which costs battery whether or not anything is
plugged in; it goes off again on the way back to the launcher.

A mouse moves the pointer whether or not **Fn** is held, so the editors work
the way they do on a desktop. A gamepad plays: the stick, the hat switch and a
d-pad hiding in the buttons all steer, and the first four buttons the report
descriptor numbers become A, B, X and Y.

The USB host and the devices go through
[badgeteam/usb-input](https://github.com/badgeteam/esp32-component-usb-input).
It starts the USB host library and the HID class driver, and it decides for
each interface if it is a keyboard, a mouse or a gamepad. Devices do not agree
on a report layout, so nothing guesses by report length.
[badgeteam/hid-host](https://github.com/badgeteam/esp32-component-hid-host)
reads each report descriptor and finds the keys, the axes, the hat switch and
the buttons. A device that it has never seen also works. A keyboard that
reports more than six keys at a time works too.

`usb_hid.c` receives the events from usb-input. Keyboard and gamepad states go
into a queue that the main loop empties once a frame, so only one task writes
the fields that TIC-80 reads. A mouse can report a thousand times a second, so
its movement is added up as it arrives and the main loop reads the total.

## Where files live

Carts and configuration go in `/sd/tic80` when an SD card is mounted, and in
`/int/tic80` on internal flash when there is none. Both are FAT, so a card can
be filled with `.tic` files from a desktop.

## Speed

60 fps, measured on hardware with the profiler in `tic80_tanmatsu.c` (set
`TIC80_TANMATSU_PROFILE` to 1):

| Stage | Per frame |
|-------|-----------|
| `studio_tick` plus `studio_sound` | 10.9 ms |
| present (PPA) | 4.6 ms |
| audio write | 1.1 ms, which is the loop waiting on the 60 Hz codec |
| input | 0.03 ms |

Carts that do per-pixel work in Lua are a different story: `fire.tic` spends
43 ms a frame inside `studio_tick` and runs at 20 fps. That is the Lua VM and
TIC-80's own drawing, not the display path, which stays at its 4.6 ms.
Handing out internal RAM first to try to move TIC-80's VRAM out of PSRAM was
measured and made it worse, 47.3 ms, so it was reverted.

It started at 32 fps. Two thirds of the gain came from the display path: the
first version scaled and rotated on the CPU into its own buffer and then called
`bsp_display_blit()`, which on a DPI panel *copies* that buffer into the
driver's framebuffer. That was 663 KiB written, 663 KiB read and 663 KiB
written again for every frame. Handing the whole job to the PPA took present
from 14.2 ms to 4.6 ms, and `studio_tick` got faster too, from 16.0 ms to
10.9 ms, because the CPU stopped pushing all those pixels through the cache.

The rest came from `sdkconfigs/tanmatsu`: dynamic frequency scaling off, and
the L2 cache raised from 128 KiB to 256 KiB. Note that 400 MHz is not
available; this board is `Chip rev: v1.0`, and pre-v3 P4 silicon rejects it
with "invalid CPU frequency value" from `esp_clk_init`.

## Installing

The app goes into AppFS with badgelink, which leaves the launcher firmware
alone. The badge has to be in USB device mode first: launcher home screen,
purple diamond, the icon top right turns from a bug into a USB symbol.

```sh
cd badgelink/tools
./badgelink.sh appfs upload com.annejan.tic80 "TIC-80" 1 path/to/tic80.bin
./badgelink.sh fs mkdir /int/apps/com.annejan.tic80
./badgelink.sh fs upload /int/apps/com.annejan.tic80/metadata.json ../../../appfs/metadata.json
./badgelink.sh fs upload /int/apps/com.annejan.tic80/icon32.png    ../../../appfs/icon32.png
```

`com.annejan.tic80` is the slug TIC-80 is published under in the Tanmatsu app
repository, and the one carts name as their interpreter (see Carts as apps).
Installing under another slug adds a second copy, and AppFS rarely has room
for two.

The launcher takes the name, description and icon from that metadata; without
it the app still runs but shows the default icon. `appfs/icon32.png` is
TIC-80's own artwork, resized from `build/linux/tic80.png`.

## Carts as apps

A cart can have its own entry in the launcher's Apps menu, the same way
Graceloader runs its apps: a repository app of type `script` that names
TIC-80 as its interpreter and the cart as its executable.

```json
"application": [{
  "targets": ["tanmatsu", "konsool"],
  "revision": 1,
  "type": "script",
  "interpreter": "com.annejan.tic80",
  "executable": "game.tic",
  "assets": [{"source_file": "game.tic", "target_file": "game.tic"}]
}]
```

The launcher installs the cart to `<sd or int>/apps/<slug>/game.tic`, and when
the app is opened it boots TIC-80 with that path as the AppFS boot argument.
TIC-80 loads the cart and plays it after the splash; Esc and CLOSE GAME drop
to the console with the cart loaded. When TIC-80 is missing, the launcher
offers to install it first.

Verified on hardware on 2026-10-21 with `tetris.tic` as a script app in
`/int/apps`: opening it from Apps plays the cart, CLOSE GAME reaches the
console, and a plain launch of TIC-80 still opens the studio.

A plain launch of TIC-80 passes the app's own directory as the argument, so
only a path to a regular file counts as a cart. The argument holds at most 127
characters.

To try a cart without the launcher, badgelink passes the argument directly:

```sh
./badgelink.sh start com.annejan.tic80 /int/apps/<slug>/game.tic
```

## Verified on hardware

Run on a Tanmatsu on 2026-08-20, installed into AppFS with badgelink. Working:
the studio comes up, the console takes typed input, `demo` writes the bundled
carts to the SD card, `load` and `run` work, music and sound effects play, the
gamepad mapping responds, the emulated pointer moves with Fn+arrows, and Fn+Esc
returns to the launcher.

## Network

The port supplies its own `net.c`, the way the 3DS and Switch ports do. Requests
are queued by `tic_net_get` and performed by a worker task with
`esp_http_client`; `tic_net_end` hands the results to their callbacks on the
main task, which is where the studio expects to be called back.

WiFi is the ESP32-C6 reached over ESP-Hosted, and the networks are the ones the
launcher already stored in NVS, so nothing is asked of the user. The radio is
brought up on first use rather than at startup: it resets the coprocessor and
takes about 35 seconds from cold to having an address.

Two things about that are worth knowing, because both cost an evening:

- **Bring the radio up in the launcher's order.** Power it into application
  mode, call `wifi_remote_initialize()`, and only then
  `wifi_connection_init_stack()`. Calling the connection manager cold leaves its
  event group uncreated and the first connect attempt asserts inside FreeRTOS.
- **The radio and the SD card share one SDMMC controller.** ESP-Hosted claims it
  during early boot, so the card has to be mounted without initialising the
  controller again, or `esp_vfs_fat_sdmmc_mount` fails with "no available sd
  host controller" and the card silently disappears. `bsp_storage_mount()`
  (badge-bsp 1.6 and later) knows this, which is why `storage.c` leaves the
  card to the BSP.

Confirmed on hardware: the radio comes up, joins a stored network and gets a
lease, surf lists the site and downloaded carts run, and TIC-80 keeps running at
60 fps throughout, with failures staying failures rather than hangs.

Which site it talks to follows the build, as it does upstream: a build whose
checkout sits exactly on a `vX.Y.Z` tag is a release and talks to tic80.com,
and anything else is a dev snapshot and talks to dev.tic80.com (`TIC_HOST` in
`src/studio/system.h`, set from `cmake/version.cmake`). Both hosts serve the
same `*.tic80.com` certificate, so the pinned root in `certs/` covers either.

## What is not here

- **Lua only.** The other TIC-80 languages are left out to keep the binary and
  the memory footprint down. MoonScript and Fennel run on the same Lua VM and
  would be cheap to add.
- **No FFT.** There is no audio input path, so the `fft()` API is compiled out
  the same way it is on the other embedded ports.
- **No CRT shader.** There is no GPU path; the software renderer draws
  straight into the framebuffer.
- **The network build is 1.7 MB**, against 1.0 MB without it, almost all of it
  TLS and the WiFi stack. That matters on a device whose AppFS partition is
  8 MB and usually has other things in it.

## How the port is put together

| File | Job |
|------|-----|
| `tic80_tanmatsu.c` | Entry point, main loop, and the `tic_sys_*` hooks the studio needs |
| `display.c` | Framebuffer allocation, RGBA8888 to RGB565, scaling, blitting |
| `audio.c` | I2S output; the blocking write is what paces the loop at 60 Hz |
| `keymap.c` | Scancodes to `tic_key`, typed text, and the emulated pointer |
| `storage.c` | Mounts internal flash, has the BSP mount the SD card, picks the cart directory |
| `CMakeLists.txt` | ESP-IDF component; source lists come from `cmake/tanmatsu.cmake` |
| `linker.lf` | Puts TIC-80's 812 KB of static buffers in PSRAM |
| `project/` | The ESP-IDF project that builds the component into an app |

TIC-80's own CMake is not used: ESP-IDF drives its own build, so
`cmake/tanmatsu.cmake` holds the source lists and both builds read the same
file.
