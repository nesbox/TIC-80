#pragma once

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/nulldevice.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include "customscreen.h"
#include "size.h"
#include "utils.h"
#include <circle/serial.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/dwhcidevice.h>
#include <SDCard/emmc.h>
#include <fatfs/ff.h>
#include <vc4/vchiq/vchiqdevice.h>
#include <vc4/sound/vchiqsoundbasedevice.h>
#include <circle/input/mouse.h>
#include <circle/input/console.h>
#include <circle/sched/scheduler.h>
#include <circle/net/netsubsystem.h>
#include <circle/startup.h>

#include <circle_glue.h>

#define MOUSE_SENS 2

static	CMemorySystem	   mMemory;
static        CActLED            mActLED;
static        CKernelOptions     mOptions;
static       CDeviceNameService mDeviceNameService;
static        CNullDevice        mNullDevice;
static        CExceptionHandler  mExceptionHandler;
static        CInterruptSystem   mInterrupt;
static	CScreenDevice      mScreen(SCREEN_WIDTH, SCREEN_HEIGHT);
static        CSerialDevice      mSerial(&mInterrupt);
static        CTimer             mTimer(&mInterrupt);
static        CLogger		mLogger(LogWarning /*mOptions.GetLogLevel ()*/, &mTimer);
static        CDWHCIDevice	mDWHCI (&mInterrupt, &mTimer);
static        CEMMCDevice     mEMMC(&mInterrupt, &mTimer, &mActLED);
static        CConsole        mConsole(&mScreen);
static	FATFS		mFileSystem;
static	CScheduler		mScheduler;
static	CVCHIQDevice		mVCHIQ(&mMemory, &mInterrupt);
static	CVCHIQSoundBaseDevice	mVCHIQSound(&mVCHIQ, 44100, 4000 /* TODO verify */,  (TVCHIQSoundDestination) mOptions.GetSoundOption ()); //VCHIQSoundDestinationHDMI);

static CUSBKeyboardDevice *pKeyboard = NULL;
static CMouseDevice *pMouse= NULL;

boolean Die(const char *msg)
{
	dbg("FATAL\n");
	dbg(msg);
	dbg("\n");
	CTimer::SimpleMsDelay(100000);
	return false;
}


boolean initializeCore()
{
	if(!mInterrupt.Initialize ())
	{
		return false;
	}
	if (!mScreen.Initialize ())
        {
        	return false;
        }

	if (!mSerial.Initialize (921600))
        {
		return false;
	}
	mSerial.RegisterMagicReceivedHandler ("REBOOT", reboot); // for hot deploy

        CDevice *pTarget = mDeviceNameService.GetDevice (mOptions.GetLogDevice (), false);
	if (pTarget == 0)
        {
        	pTarget = &mScreen;
	}

        if (!mLogger.Initialize (&mNullDevice)) //pTarget))      // if (!mLogger.Initialize(pTarget))
        {
        	return false;
	}

	if(!mTimer.Initialize ())
	{
		return false;
	}

 	if (!mEMMC.Initialize ())
	{
                return false;
	}

            /*    CDevice * const pPartition =
                        mDeviceNameService.GetDevice (CSTDLIBAPP_DEFAULT_PARTITION, true);
                if (pPartition == 0)
                {
                        mLogger.Write (GetKernelName (), LogError,
                                       "Partition not found: %s", mpPartitionName);

                        return false;
                }*/

                /*if (!mFileSystem.Mount (pPartition))
                {
                        mLogger.Write (GetKernelName (), LogError,
                                         "Cannot mount partition: %s", mpPartitionName);
                        return false;
                }*/

	if (!mDWHCI.Initialize ())
	{
		return false;
        }

	if (!mConsole.Initialize ())
	{
        	return false;
	}

	if (!mVCHIQ.Initialize ())
        {
                        return false;
        }


        // Initialize newlib stdio with a reference to Circle's file system and console
	CGlueStdioInit (mConsole);

	if (f_mount (&mFileSystem, "SD:", 1) != FR_OK)
	{
		Die("Cannot mount drive");
	}

	pKeyboard = (CUSBKeyboardDevice *) mDeviceNameService.GetDevice ("ukbd1", FALSE);
	if (pKeyboard == 0)
	{
		return Die("Keyboard not found");
	}

	pMouse = (CMouseDevice *) mDeviceNameService.GetDevice ("mouse1", FALSE);
	if (pMouse == 0)
	{
		dbg("Mouse not found");
	}
	else
	{
		if (!pMouse->Setup (SCREEN_WIDTH*MOUSE_SENS, SCREEN_HEIGHT*MOUSE_SENS))
		{
			Die("Cannot setup mouse");
		}

	}


	CScreenDevice* screen = &mScreen;
	dbg("Screen is:\n");
	dbg("%d x %d pitch: %d\n", screen->GetWidth(), screen->GetHeight(), screen->GetPitch());

	CLogger::Get()->Write("TEST", LogError, "Test Logging!");
	dbg("data:\n");
	dbg("Memory: %p\n", &mMemory);
	dbg("Snd: %p\n", &mVCHIQSound);

	

	dbg("Creating tic80 instance..\n");


	return true;

}

