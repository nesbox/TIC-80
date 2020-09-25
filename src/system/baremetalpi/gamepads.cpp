#include "gamepads.h"
#include "utils.h"

void initGamepads(CDeviceNameService m_DeviceNameService, TGamePadStatusHandler  handler)
{
	dbg("Searching gamepads..\n");
	for (unsigned nDevice = 1; nDevice<=4; nDevice++) // max 4 gamepads
	{
		CString DeviceName;
		DeviceName.Format ("upad%u", nDevice);

		CUSBGamePadDevice *pGamePad =
			(CUSBGamePadDevice *) m_DeviceNameService.GetDevice (DeviceName, FALSE);

		if (pGamePad == 0)
		{
			// no more gamepads
			break;
		}

		const TGamePadState *pState = pGamePad->GetInitialState ();

		assert (pState != 0);

		dbg("Prop %d\n", pGamePad->GetProperties());

		dbg("Gamepad %u: %d Button(s) %d Hat(s)\n", nDevice, pState->nbuttons, pState->nhats);

		for (int i = 0; i < pState->naxes; i++)
		{
			dbg("Gamepad %u: Axis %d: Minimum %d Maximum %d\n", nDevice, i+1, pState->axes[i].minimum, pState->axes[i].maximum);
		}

		pGamePad->RegisterStatusHandler (handler);

	}
	dbg("Finished searching gamepads\n");

}
