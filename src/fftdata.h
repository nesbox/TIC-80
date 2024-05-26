#pragma once
#include <stdbool.h>
#define FFT_SIZE 1024
extern float fPeakMinValue;
extern float fPeakSmoothing;
extern float fPeakSmoothValue;
extern float fAmplification;
extern float fftData[FFT_SIZE];
extern float fftSmoothingData[FFT_SIZE];
extern float fftNormalizedData[FFT_SIZE];
extern float fftNormalizedMaxData[FFT_SIZE];

extern bool fftEnabled;

typedef enum
{
    FFT_LOG_OFF = 0,
    FFT_LOG_FATAL,
    FFT_LOG_ERROR,
    FFT_LOG_WARNING,
    FFT_LOG_INFO,
    FFT_LOG_DEBUG,
    FFT_LOG_TRACE
} FFT_LogLevel;

extern FFT_LogLevel g_currentLogLevel;

void FFT_DebugLog(FFT_LogLevel level, const char* format, ...);
