/**
 * Convenience classes that package different levels
 * of functionality of Circle applications.
 *
 * Derive the kernel class of the application from one of
 * the CStdlibApp* classes and implement at least the
 * Run () method. Extend the Initalize () and Cleanup ()
 * methods if necessary.
 */
#ifndef _stdlib_app_h
#define _stdlib_app_h

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/nulldevice.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include "customscreen.h"
#include <tic80.h>
#include <circle/serial.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/dwhcidevice.h>
#include <SDCard/emmc.h>
#include <fatfs/ff.h>
#include <vc4/vchiq/vchiqdevice.h>
#include <vc4/sound/vchiqsoundbasedevice.h>

// include <circle/fs/fat/fatfs.h>
#include <circle/input/console.h>
#include <circle/sched/scheduler.h>
#include <circle/net/netsubsystem.h>
#include <circle/startup.h>

#include <circle_glue.h>

#include <tic80.h>

/**
 * Basic Circle Stdlib application that supports GPIO access.
 */
class CStdlibApp
{
public:
        enum TShutdownMode
        {
                ShutdownNone,
                ShutdownHalt,
                ShutdownReboot
        };

        CStdlibApp (const char *kernel) :
                FromKernel (kernel)
        {
        }

        virtual ~CStdlibApp (void)
        {
        }

        virtual bool Initialize (void)
        {
                return mInterrupt.Initialize ();
        }

        virtual void Cleanup (void)
        {
        }

        virtual TShutdownMode Run (void) = 0;

        const char *GetKernelName(void) const
        {
                return FromKernel;
        }

protected:
	CMemorySystem	   mMemory;
        CActLED            mActLED;
        CKernelOptions     mOptions;
        CDeviceNameService mDeviceNameService;
        CNullDevice        mNullDevice;
        CExceptionHandler  mExceptionHandler;
        CInterruptSystem   mInterrupt;
private:
        char const *FromKernel;
};

/**
 * Stdlib application that adds screen support
 * to the basic CStdlibApp features.
 */
class CStdlibAppScreen : public CStdlibApp
{
public:
        CStdlibAppScreen(const char *kernel)
                : CStdlibApp (kernel),
                  mScreen (TIC80_WIDTH, TIC80_HEIGHT),
		mSerial(&mInterrupt),
                  mTimer (&mInterrupt),
                  mLogger (LogWarning /*mOptions.GetLogLevel ()*/, &mTimer)

        {
        }

        virtual bool Initialize (void)
        {
                if (!CStdlibApp::Initialize ())
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
		mSerial.RegisterMagicReceivedHandler ("REBOOT", reboot);

                CDevice *pTarget =
                        mDeviceNameService.GetDevice (mOptions.GetLogDevice (), false);
                if (pTarget == 0)
                {
                        pTarget = &mScreen;
                }

                if (!mLogger.Initialize (&mNullDevice)) //pTarget))
                // if (!mLogger.Initialize(pTarget))
                {
                        return false;
                }

                return mTimer.Initialize ();
        }
	CScreenDevice* getScreen()
	{
		return &mScreen;
	}

protected:
        CScreenDevice   mScreen;
        CSerialDevice   mSerial;
        CTimer          mTimer;
        CLogger         mLogger;
};

/**
 * Stdlib application that adds stdio support
 * to the CStdlibAppScreen functionality.
 */
class CStdlibAppStdio: public CStdlibAppScreen
{
private:
        char const *mpPartitionName;

public:
        // ransform to constexpr
        // constexpr char static DefaultPartition[] = "emmc1-1";
#define CSTDLIBAPP_DEFAULT_PARTITION "emmc1-1"

        CStdlibAppStdio (const char *kernel,
                         const char *pPartitionName = CSTDLIBAPP_DEFAULT_PARTITION)
                : CStdlibAppScreen (kernel),
                  mpPartitionName (pPartitionName),
                  mDWHCI (&mInterrupt, &mTimer),
                  mEMMC (&mInterrupt, &mTimer, &mActLED),
                  mConsole (&mScreen),
		  mVCHIQ (&mMemory, &mInterrupt),
		  mVCHIQSound (&mVCHIQ, 44100, 
			4000, // TODO verify 
			VCHIQSoundDestinationHDMI)
        {
        }

        virtual bool Initialize (void)
        {
                if (!CStdlibAppScreen::Initialize ())
                {
                        return false;
                }

                if (!mEMMC.Initialize ())
                {
                        return false;
                }

                CDevice * const pPartition =
                        mDeviceNameService.GetDevice (mpPartitionName, true);
                if (pPartition == 0)
                {
                        mLogger.Write (GetKernelName (), LogError,
                                       "Partition not found: %s", mpPartitionName);

                        return false;
                }

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

                mLogger.Write (GetKernelName (), LogNotice, "Compile time: " __DATE__ " " __TIME__);

                return true;
        }

        virtual void Cleanup (void)
        {
                //mFileSystem.UnMount ();

                CStdlibAppScreen::Cleanup ();
        }

protected:
        CDWHCIDevice    mDWHCI;
        CEMMCDevice     mEMMC;
        //CFATFileSystem  mFileSystem;
        CConsole        mConsole;
	FATFS		mFileSystem;
	CScheduler		mScheduler;
	CVCHIQDevice		mVCHIQ;
	CVCHIQSoundBaseDevice	mVCHIQSound;
};

#endif
