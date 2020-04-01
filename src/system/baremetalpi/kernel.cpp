//
// kernel.cpp
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "kernel.h"
#include "customscreen.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <tic80.h>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/input/mouse.h>
#include "utils.h"
#include <limits.h>
#include <studio.h>
#include "keycodes.h"
#include "gamepads.h"
#include "syscore.h"

static const char* KN = "TIC80"; // kernel name

// currently pressed keys and modifiers
static unsigned char keyboardRawKeys[6];
static char keyboardModifiers;

// mouse status
static unsigned mousex;
static unsigned mousey;
static unsigned mousexOld;
static unsigned mouseyOld;
static unsigned mousebuttons;
static unsigned mousebuttonsOld;
static unsigned mouseTime = 600; // starts not visible

// gamepad status
static struct TGamePadState gamepad;

static CSpinLock keyspinlock;

extern "C" {
static struct
{

	Studio* studio;

	struct
	{
		bool state[tic_keys_count];
	} keyboard;

	char* clipboard;

} platform;

static void setClipboardText(const char* text)
{
	if(platform.clipboard)
	{
		free(platform.clipboard);
		platform.clipboard = NULL;
	}

	platform.clipboard = strdup(text);
}

static bool hasClipboardText()
{
	return platform.clipboard != NULL;
}

static char* getClipboardText()
{
	return platform.clipboard ? strdup(platform.clipboard) : NULL;
}

static void freeClipboardText(const char* text)
{
	free((void*)text);
}

static u64 getPerformanceCounter()
{
	return CTimer::Get()->GetTicks();
}

static u64 getPerformanceFrequency()
{
	return HZ;
}

static void* httpGetSync(const char* url, s32* size)
{
	return NULL;
}

static void httpGet(const char* url, HttpGetCallback callback, void* calldata)
{
	
}

static void agoFullscreen()
{
}

static void showMessageBox(const char* title, const char* message)
{
}

static void setWindowTitle(const char* title)
{
}

static void openSystemPath(const char* path)
{

}

static void preseed()
{
#if defined(__TIC_MACOSX__)
	srandom(time(NULL));
	random();
#else
	srand(time(NULL));
	rand();
#endif
}

static void pollEvent()
{

}

static void updateConfig()
{

}


static System systemInterface = 
{
	.setClipboardText = setClipboardText,
	.hasClipboardText = hasClipboardText,
	.getClipboardText = getClipboardText,
	.freeClipboardText = freeClipboardText,

	.getPerformanceCounter = getPerformanceCounter,
	.getPerformanceFrequency = getPerformanceFrequency,

	.httpGetSync = httpGetSync,
	.httpGet = httpGet,

	.fileDialogLoad = NULL, //file_dialog_load,
	.fileDialogSave = NULL, //file_dialog_save,

	.goFullscreen = agoFullscreen,
	.showMessageBox = showMessageBox,
	.setWindowTitle = setWindowTitle,

	.openSystemPath = openSystemPath,
	.preseed = preseed,
	.poll = pollEvent,
	.updateConfig = updateConfig,
};

u32 rgbaToBgra(u32 u){
	u8 r = u & 0xFF;
	u8 g = (u >> 8) & 0xff;
	u8 b = (u >> 16) & 0xff;
	return (b & 0xFF)      
					| ((g & 0xFF) << 8)
					| ((r & 0xFF)   << 16)
					| (0xFF << 24);
}

void screenCopy(CScreenDevice* screen, u32* ts)
{
	u32 pitch = screen->GetPitch();
	u32* buf = screen->GetBuffer();
	for (int y = 0; y<136;y++)
	for (int x = 0; x<240;x++)
	{
		u32 p = ts[(y+4)*(8+240+8)+(x+8)];
		buf[pitch*y + x] = rgbaToBgra(p);
	}

	// single pixel mouse pointer, disappear after 10 seconds unmoved
	if (mouseTime<600)
	{
		u32 midx =  pitch*(mousey)+mousex;
		buf[midx]= 0xffffff;
	}

	// memcpy(screen->GetBuffer(), tic->screen, 240*136*4); would have been too good
}



} //extern C

void mouseEventHandler (TMouseEvent Event, unsigned nButtons, unsigned nPosX, unsigned nPosY)
{
	keyspinlock.Acquire();
	mousex = nPosX/MOUSE_SENS;
	mousey = nPosY/MOUSE_SENS;
	mousebuttons = nButtons;
	keyspinlock.Release();
}

void gamePadStatusHandler (unsigned nDeviceIndex, const TGamePadState *pState)
{

	keyspinlock.Acquire();
	// Just copy buttons and axes. 
	gamepad.buttons = pState -> buttons;
	gamepad.naxes = pState -> naxes;
	for (int i = 0; i< pState->naxes; i++)
	{
		gamepad.axes[i].value = pState -> axes[i].value;
	}
	keyspinlock.Release();
}


void inputToTic()
{
	tic_mem* tic = platform.studio->tic;

	tic80_input* tic_input = &tic->ram.input;

	// mouse
	if (mousebuttons & 0x01) tic_input->mouse.left = true; else tic_input->mouse.left = false;
	if (mousebuttons & 0x02) tic_input->mouse.right = true; else tic_input->mouse.right = false;
	if (mousebuttons & 0x04) tic_input->mouse.middle = true; else tic_input->mouse.middle = false;
	tic_input->mouse.x = mousex;
	tic_input->mouse.y = mousey;

	if( (mousex == mousexOld) && (mousey == mouseyOld) && (mousebuttons == mousebuttonsOld))
	{
		mouseTime++;
	}
	else
	{
		mouseTime = 0;
	}

	mousexOld = mousex;
	mouseyOld = mousey;
	mousebuttonsOld = mousebuttons;

	// keyboard
	tic_input->gamepads.first.data = 0;
	tic_input->keyboard.data = 0;

	u32 keynum = 0;

	//dbg("MODIF %02x ", keyboardModifiers);

	if(keyboardModifiers & 0x11) tic_input->keyboard.keys[keynum++]= tic_key_ctrl;
	if(keyboardModifiers & 0x22) tic_input->keyboard.keys[keynum++]= tic_key_shift;
	if(keyboardModifiers & 0x44) tic_input->keyboard.keys[keynum++]= tic_key_alt;

	for (unsigned i = 0; i < 6; i++)
	{
		if (keyboardRawKeys[i] != 0)
		{
			// keyboard
			if(keynum<TIC80_KEY_BUFFER){
				tic_keycode tkc = TicKeyboardCodes[keyboardRawKeys[i]];
				if(tkc != tic_key_unknown) tic_input->keyboard.keys[keynum++]= tkc;
			}
			// key to gamepad
			switch(keyboardRawKeys[i])
			{
				case 0x1d: tic_input->gamepads.first.a = true; break;
				case 0x1b: tic_input->gamepads.first.b = true; break;
				case 0x52: tic_input->gamepads.first.up = true; break;
				case 0x51: tic_input->gamepads.first.down = true; break;
				case 0x50: tic_input->gamepads.first.left = true; break;
				case 0x4F: tic_input->gamepads.first.right = true; break;
			}
			//dbg(" %02x ", keyboardRawKeys[i]);

		}
	}
	//dbg("\n");


	// gamepads

	if (gamepad.buttons & 0x100) tic_input->gamepads.first.a = true;
	if (gamepad.buttons & 0x200) tic_input->gamepads.first.b = true;
	if (gamepad.buttons & 0x400) tic_input->gamepads.first.x = true;
	if (gamepad.buttons & 0x800) tic_input->gamepads.first.y = true;
	// map ESC to a gamepad button to exit the game
	if (gamepad.buttons & 0x1000) tic_input->keyboard.keys[keynum++]= tic_key_escape;

	// TODO use min and max instead of hardcoded range
	if (gamepad.naxes > 0)
	{
		if (gamepad.axes[0].value < 50) tic_input->gamepads.first.left = true;
		if (gamepad.axes[0].value > 200) tic_input->gamepads.first.right = true;

	}
	if (gamepad.naxes > 1)
	{
		if (gamepad.axes[1].value < 50) tic_input->gamepads.first.up = true;
		if (gamepad.axes[1].value > 200) tic_input->gamepads.first.down = true;
	}
/*
        dbg("G:BTN 0x%X", gamepad.buttons);

        if (gamepad.naxes > 0)
        {
                dbg(" A");

                for (int i = 0; i < gamepad.naxes; i++)
                {
                        dbg(" %d", gamepad.axes[i].value);
                }
        }

        if (gamepad.nhats > 0)
        {
                dbg(" H");

                for (int i = 0; i < gamepad.nhats; i++)
                {
                        dbg(" %d", gamepad.hats[i]);
                }
        }

       dbg("\n");
*/

}
void KeyStatusHandlerRaw (unsigned char ucModifiers, const unsigned char RawKeys[6])
{
	// this gets called with whatever key is currently pressed in RawKeys
	// (plus modifiers).
	keyspinlock.Acquire();
	keyboardModifiers = ucModifiers;
	memcpy(keyboardRawKeys, RawKeys, sizeof(unsigned char)*6);
	keyspinlock.Release();
}

void testmkdir(const char* name)
{
	dbg("creating %s : ", name);
	FRESULT res = f_mkdir(name);
	if(res == FR_OK)
	{
		dbg("ok\n");
	}
	else
	{
		dbg("KO %d\n", res);
	}
}

void teststat(const char* name)
{
	dbg("K:%s", name);
	FILINFO filinfo;
	FRESULT res = f_stat (name, &filinfo);
	dbg("stat: %d ", res);
	dbg("size: %ld ", filinfo.fsize);
	dbg("attr: %02x\n", filinfo.fattrib);
}

TShutdownMode Run(void)
{
	initializeCore();
	mLogger.Write (KN, LogNotice, "TIC80 Port");

	//teststat("circle.txt");
	//teststat("no.txt");
	//teststat("carts");

	//testmkdir("/slash");
	//CTimer::SimpleMsDelay(5000);

	// ok testmkdir("primo");
	// ko testmkdir("secondo/bis");
	// ok testmkdir(".terzo");
	// ko testmkdir(".quarto/");
	// ko testmkdir("quinto/");
	// ok testmkdir("sesto bis");


	dbg("Calling studio init instance..\n");

	if (pKeyboard)
	{
		dbg("With keyboard\n");
		platform.studio = studioInit(0, NULL, 44100, "tic80/", &systemInterface);
	}
	else
	{
		//  if no keyboard, start in surf mode!
		char  arg0[] = "xxkernel";
		char  arg1[] = "-surf";
		char* argv[] = { &arg0[0], &arg1[0], NULL };
		int argc = 2;
		dbg("Without keyboard\n");
		platform.studio = studioInit(argc, argv, 44100, "tic80/", &systemInterface);
	}

	if( !platform.studio)
	{
		Die("Could not init studio");
	}

	// gotoSurf();

	dbg("Studio init ok..\n");

	if (pKeyboard){
		pKeyboard->RegisterKeyStatusHandlerRaw (KeyStatusHandlerRaw);
	}

	initGamepads(mDeviceNameService, gamePadStatusHandler);

	if (pMouse) {
		pMouse->RegisterEventHandler (mouseEventHandler);
	}

	tic_mem* tic = platform.studio->tic;
	tic80_input* tic_input = &tic->ram.input;
	tic_input->keyboard.data = 0;


	// sound system
	mSound->AllocateQueue(1000);
	mSound->SetWriteFormat(SoundFormatSigned16);

	if (!mSound->Start())
	{
		Die("Could not start sound.");
	}

	// MAIN LOOP
	while(true)
	{
		keyspinlock.Acquire();
		inputToTic();
		keyspinlock.Release();

		platform.studio->tick();

		mSound->Write(tic->samples.buffer, tic->samples.size);

		mScreen.vsync();

		screenCopy(&mScreen, platform.studio->tic->screen);

		mScheduler.Yield(); // for sound
	}

	return ShutdownHalt;
}
