#pragma once
#define FFT_SIZE 2048

typedef int                 BOOL;
  //////////////////////////////////////////////////////////////////////////

  typedef struct FFT_Settings
  {
    BOOL bUseRecordingDevice;
    void* pDeviceID;
  } FFT_Settings;

  typedef void (*FFT_ENUMERATE_FUNC)(const BOOL bIsCaptureDevice, const char* szDeviceName, void* pDeviceID, void* pUserContext);

  extern float fAmplification;

  void FFT_EnumerateDevices(FFT_ENUMERATE_FUNC pEnumerationFunction, void* pUserContext);

  BOOL FFT_Create();
  BOOL FFT_Destroy();
  BOOL FFT_Open(FFT_Settings* pSettings);
  BOOL FFT_GetFFT(float* _samples);
  void FFT_Close();

  //////////////////////////////////////////////////////////////////////////

