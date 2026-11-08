# Changelog: TIC-80 for Tanmatsu

The changes to the Tanmatsu port, one entry for each release in the
[badge.team app repository](https://github.com/Nicolai-Electronics/app-repository)
(`com.annejan.tic80`). The revision is the number the launcher updates on, and
the version is the one the app shows. For the changes to TIC-80 itself, see the
[upstream releases](https://github.com/nesbox/TIC-80/releases).

## Unreleased

- Built on upstream TIC-80 main, which is now on the 1.5 line. Upstream made
  the drawing API and the vbank blit faster.

## Revision 8: 1.4.3365-dev (2026-11-05)

- badge-bsp 2.0.0. The app works on the boards with the new display panel,
  and `bsp_version` in the metadata is 2.
- Built on upstream TIC-80 main after 1.4.1. It has the new menu bar instead
  of the toolbar, and the new code editor. A build that is not on a release
  tag is a development snapshot, so the version ends in `-dev`.
- Carts start from the launcher's Apps menu. A repository app can name
  `com.annejan.tic80` as its interpreter and a `.tic` cart as its executable.
- The BSP mounts the SD card. The port has no SD card driver of its own now.
- USB keyboards, mice and gamepads go through the shared
  [badgeteam/usb-input](https://github.com/badgeteam/esp32-component-usb-input)
  component. Its hid-host 0.4.0 reads keyboards that report more than six
  keys at a time.
- The default-route workaround is removed. The WiFi station is already the
  default route. The earlier failures came from requests sent before DHCP gave
  an address.

## Revision 7: 1.3.3 (2026-10-04)

- Built on upstream TIC-80 1.3.3. The toolbar and the ROM handling are now
  separate modules, as they are upstream.
- Surf fixes from upstream: the covers no longer block the browser, and
  CLOSE GAME on a cart goes back to the browser.
- The VQT analysis is part of the core.

## Revision 6: 1.3.2 (2026-08-31)

- A fast USB mouse no longer loses movement. Mouse movement is added up on
  the HID driver task, and the main loop reads the total once a frame. Before,
  each report used a queue entry, so the queue filled and dropped movement and
  keyboard events.

## Revision 5: 1.3.2 (2026-08-26)

- USB host: a keyboard, a mouse or a gamepad in the USB-C port works together
  with the built-in keyboard.
- The mouse moves the pointer without Fn. Fn and the arrow keys still work.
- Gamepads steer with the stick, the hat switch or the d-pad buttons. The
  first four buttons in the report descriptor become A, B, X and Y.
- Each device is identified from its report descriptor through
  [badgeteam/hid-host](https://github.com/badgeteam/esp32-component-hid-host),
  so devices the port does not know also work.
- The USB port gets power only while TIC-80 runs.

## Revision 4: 1.3.1 (2026-08-22)

- Networking works. Surf lists tic80.com, and carts download and run.
- The WiFi interface is made the default route. Without it, all traffic past
  the gateway failed with "Host is unreachable".
- The port includes the GTS Root R4 certificate for tic80.com. The certificate
  store was empty before, so every TLS handshake failed. The binary is 72 KiB
  smaller than when it carried a full certificate bundle.
- Retries for a TLS handshake that the server drops, and for a WiFi join that
  fails on the first attempt.

## Revision 3: 1.3.0 (2026-08-22)

- First networking: surf can use the WiFi networks that the launcher has
  stored. In this revision, no request reached tic80.com (fixed in
  revision 4).
- The SD card mounts when the radio is in use. The card and the ESP32-C6
  radio use the same SDMMC controller.

## Revision 2: 1.2.1 (2026-08-22)

- Lua uses 32-bit numbers. The ESP32-P4 has a single precision FPU, so the
  heaviest scene of a per-pixel demo went from 5 to 16 fps. Precision is
  about 7 digits instead of 15.
- The PPA scales and rotates the picture into the panel's framebuffer, so the
  CPU does not copy pixels. One frame on screen went from 14.2 ms to 4.6 ms.
- Two framebuffers, so the picture does not tear.
- Sound goes through a buffer that a task on the second core empties, so a
  slow frame does not cause a gap in the sound.

## Revision 1: 1.2.0 (2026-08-20)

- First release: the full studio (console, code editor, and the sprite, map,
  sound and music editors) on the ESP32-P4, through badge-bsp directly.
- Fn turns the arrow keys into a pointer. Fn with the left space key clicks,
  Fn with the right space key right-clicks, and Fn with Esc goes back to the
  launcher.
- Carts and settings are in `/sd/tic80`, or in `/int/tic80` when there is no
  SD card.
