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

CSpinLock keyspinlock;


	static u8 kl[10000];
	static u32 ki=0;
	void logchar(u8 c)
	{
		if(ki < 10000)
		{
		 kl[ki++]=c;
		}
	}
	void diechar()
	{
		kl[ki] =0;
		printf((const char*)kl);
		printf("\n");
		CTimer::SimpleMsDelay(60000);
	}


extern "C" {
static struct
{

	Studio* studio;

	struct
	{
		bool state[tic_keys_count];
	} keyboard;

	/*
	TODO restore
	struct
	{
		saudio_desc desc;
		float* samples;
	} audio;
	*/
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

static void* getUrlRequest(const char* url, s32* size)
{
	return NULL;
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

	.getUrlRequest = getUrlRequest,

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
	tic_mem* tic = platform.studio->tic;
	tic80_input* input = &tic->ram.input;

 u32 pitch = screen->GetPitch();
 u32* buf = screen->GetBuffer();
 for (int y = 0; y<136;y++)
 for (int x = 0; x<240;x++)
  {
	u32 p = ts[(y+4)*(8+240+8)+(x+8)];
	buf[pitch*y + x] = rgbaToBgra(p);
  }

 // single pixel mouse pointer
 //u32 midx =  pitch*(input->mouse.y)+input->mouse.x;
 //buf[midx]= 0xffffff;


//	memcpy(screen->GetBuffer(), tic->screen, 240*136*4);
}



} //extern C

void mouseEventHandler (TMouseEvent Event, unsigned nButtons, unsigned nPosX, unsigned nPosY)
{
	keyspinlock.Acquire();
	//dbg("mouse %x p: %d %d", nButtons, nPosX, nPosY);
	tic80_input* input = &platform.studio->tic->ram.input;
	if (nButtons & 0x01) input->mouse.left = true; else input->mouse.left = false;
	if (nButtons & 0x02) input->mouse.right = true; else input->mouse.right = false;
	if (nButtons & 0x04) input->mouse.middle = true; else input->mouse.middle = false;
	input->mouse.x = nPosX/MOUSE_SENS;
	input->mouse.y = nPosY/MOUSE_SENS;
	keyspinlock.Release();
}

void gamePadStatusHandler (unsigned nDeviceIndex, const TGamePadState *pState)
{
/*
	keyspinlock.Acquire();
	tic_mem* tic = platform.studio->tic;
	tic80_input* tic_input = &tic->ram.input;

	if(pState->buttons & 0x100)
	{
		tic_input->gamepads.first.a = true; 
	}

	keyspinlock.Release();

*/

/*
        printf("GP%u:BTN 0x%X", nDeviceIndex+1, pState->buttons);


        if (pState->naxes > 0)
        {
                printf(" A");

                for (int i = 0; i < pState->naxes; i++)
                {
                        printf(" %d", pState->axes[i].value);
                }
        }

        if (pState->nhats > 0)
        {
                printf(" H");

                for (int i = 0; i < pState->nhats; i++)
                {
                        printf(" %d", pState->hats[i]);
                }
        }

       printf("\n");
	// CLogger::Get ()->Write ("G", LogWarning, Msg);

*/
}

void KeyStatusHandlerRaw (unsigned char ucModifiers, const unsigned char RawKeys[6])
{
	keyspinlock.Acquire();
	tic_mem* tic = platform.studio->tic;

	tic80_input* tic_input = &tic->ram.input;

	tic_input->gamepads.first.data = 0;
	tic_input->keyboard.data = 0;

	u32 keynum = 0;

	//dbg("MODIF %02x ", ucModifiers);

	if(ucModifiers & 0x11) tic_input->keyboard.keys[keynum++]= tic_key_ctrl;
	if(ucModifiers & 0x22) tic_input->keyboard.keys[keynum++]= tic_key_shift;
	if(ucModifiers & 0x44) tic_input->keyboard.keys[keynum++]= tic_key_alt;

	for (unsigned i = 0; i < 6; i++)
	{
		if (RawKeys[i] != 0)
		{
			// keyboard
			if(keynum<TIC80_KEY_BUFFER){
				tic_keycode tkc = TicKeyboardCodes[RawKeys[i]];
				if(tkc != tic_key_unknown) tic_input->keyboard.keys[keynum++]= tkc;
			}
			// key to gamepad
			switch(RawKeys[i])
			{
				case 0x1d: tic_input->gamepads.first.a = true; break;
				case 0x1b: tic_input->gamepads.first.b = true; break;
				case 0x52: tic_input->gamepads.first.up = true; break;
				case 0x51: tic_input->gamepads.first.down = true; break;
				case 0x50: tic_input->gamepads.first.left = true; break;
				case 0x4F: tic_input->gamepads.first.right = true; break;
	/*
				case 0x3a: if (keynum<TIC80_KEY_BUFFER) tic_input->keyboard.keys[keynum++]= tic_key_f1;
				case 0x3b: if (keynum<TIC80_KEY_BUFFER) tic_input->keyboard.keys[keynum++]= tic_key_f2;
				case 0x3c: if (keynum<TIC80_KEY_BUFFER) tic_input->keyboard.keys[keynum++]= tic_key_f3;
				case 0x3d: if (keynum<TIC80_KEY_BUFFER) tic_input->keyboard.keys[keynum++]= tic_key_f4;
				case 0x29: if (keynum<TIC80_KEY_BUFFER) tic_input->keyboard.keys[keynum++]= tic_key_escape;
*/
			}
//			if (RawKeys[i] == 0x13) diechar();
	//		dbg(" %02x ", RawKeys[i]);

		}
	}
//	dbg("\n");
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


	platform.studio = studioInit(0, NULL, 44100, "tic80/", &systemInterface);
	if( !platform.studio)
	{
		Die("Could not init studio");
	}
	dbg("Studio init ok..\n");

	if (pKeyboard){
		pKeyboard->RegisterKeyStatusHandlerRaw (KeyStatusHandlerRaw);
	}
	//initGamepads(mDeviceNameService, gamePadStatusHandler);
	if (pMouse) {
		pMouse->RegisterEventHandler (mouseEventHandler);
	}

	tic_mem* tic = platform.studio->tic;
	tic80_input* tic_input = &tic->ram.input;


	// sound system
	mSound->AllocateQueue(1000);
	mSound->SetWriteFormat(SoundFormatSigned16);

	if (! (mSound -> Start()) )
	{
		Die("Could not start sound.");
	}

	// MAIN LOOP
	while(true)
	{
		keyspinlock.Acquire();
		platform.studio->tick();
		keyspinlock.Release();

		mSound->Write(tic->samples.buffer, tic->samples.size);

		mScreen.vsync();

		screenCopy(&mScreen, platform.studio->tic->screen);

		mScheduler.Yield(); // for sound
	}

	return ShutdownHalt;
}
