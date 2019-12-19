#pragma once

#include <circle/usb/usbgamepad.h>
#include <circle/devicenameservice.h>
#include <stdio.h>

void initGamepads(CDeviceNameService m_DeviceNameService, TGamePadStatusHandler  handler);
