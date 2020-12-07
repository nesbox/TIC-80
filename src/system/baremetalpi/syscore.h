#pragma once

#define HDMI_SOUND 1

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/nulldevice.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include "customscreen.h"
#include <tic80.h>
#include "utils.h"
#include <circle/serial.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/usbhcidevice.h>
#include <SDCard/emmc.h>
#include <fatfs/ff.h>

#ifdef HDMI_SOUND
  #include <vc4/vchiq/vchiqdevice.h>
  #include <vc4/sound/vchiqsoundbasedevice.h>
#else
  #include <circle/pwmsoundbasedevice.h>
#endif

#include <circle/input/mouse.h>
#include <circle/input/console.h>
#include <circle/sched/scheduler.h>
#include <circle/net/netsubsystem.h>
#include <circle/startup.h>

#include <circle_glue.h>


// mouse sensitivity
#define MOUSE_SENS 2
#define SAMPLE_RATE 44100
#define CHUNK_SIZE 4000

static        CActLED            mActLED;
static        CKernelOptions     mOptions;
static       CDeviceNameService mDeviceNameService;
static        CNullDevice        mNullDevice;
static        CExceptionHandler  mExceptionHandler;
static        CInterruptSystem   mInterrupt;
#ifdef EN_DEBUG
// show a larger screen, so the actual screen is on the top left
// and output is readable
static	CScreenDevice      mScreen(1280,720);
#else
static	CScreenDevice      mScreen(TIC80_WIDTH, TIC80_HEIGHT);
#endif
static        CSerialDevice      mSerial(&mInterrupt);
static        CTimer             mTimer(&mInterrupt);
static        CLogger		mLogger(LogWarning /*mOptions.GetLogLevel ()*/, &mTimer);
static        CUSBHCIDevice	mDWHCI (&mInterrupt, &mTimer);
static        CEMMCDevice     mEMMC(&mInterrupt, &mTimer, &mActLED);
static        CConsole        mConsole(&mScreen);
static	FATFS		mFileSystem;
static	CScheduler		mScheduler;
static CUSBKeyboardDevice *pKeyboard = NULL;
static CMouseDevice *pMouse= NULL;

static CSoundBaseDevice	*mSound;


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

        //if (!mLogger.Initialize (pTarget))
        if (!mLogger.Initialize (&mNullDevice))
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

#ifdef HDMI_SOUND
	// use HDMI sound, which need initialization
	CMemorySystem *mMemory = new CMemorySystem();

	CVCHIQDevice *mVCHIQ = new CVCHIQDevice(mMemory, &mInterrupt);
	if (!(mVCHIQ->Initialize ()))
        {
                        return false;
        }
	mSound = new CVCHIQSoundBaseDevice(mVCHIQ, SAMPLE_RATE, CHUNK_SIZE,  (TVCHIQSoundDestination) mOptions.GetSoundOption ()); //VCHIQSoundDestinationHDMI);
#else
	// jack audio
	mSound = new CPWMSoundBaseDevice (&mInterrupt, SAMPLE_RATE, CHUNK_SIZE);
#endif


        // Initialize newlib stdio with a reference to Circle's file system and console
	CGlueStdioInit (mConsole);

	if (f_mount (&mFileSystem, "SD:", 1) != FR_OK)
	{
		Die("Cannot mount drive");
	}

	pKeyboard = (CUSBKeyboardDevice *) mDeviceNameService.GetDevice ("ukbd1", FALSE);
	/*
	keep going in "surf mode"
	if (pKeyboard == 0)
	{
		return Die("Keyboard not found");
	}*/

	pMouse = (CMouseDevice *) mDeviceNameService.GetDevice ("mouse1", FALSE);
	if (pMouse == 0)
	{
		dbg("Mouse not found");
	}
	else
	{
		if (!pMouse->Setup (TIC80_WIDTH*MOUSE_SENS, TIC80_HEIGHT*MOUSE_SENS))
		{
			Die("Cannot setup mouse");
		}

	}


	CScreenDevice* screen = &mScreen;
	dbg("Screen is:\n");
	dbg("%d x %d pitch: %d\n", screen->GetWidth(), screen->GetHeight(), screen->GetPitch());

	CLogger::Get()->Write("TEST", LogError, "Test Logging!");
	dbg("data:\n");
	//dbg("Memory: %p\n", &mMemory);
	//dbg("Snd: %p\n", &mVCHIQSound);

	

	dbg("Creating tic80 instance..\n");


	return true;

}

