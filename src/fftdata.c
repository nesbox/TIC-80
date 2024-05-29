#ifndef TIC80_FFT_UNSUPPORTED
#include "fftdata.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

float fPeakMinValue = 0.01f;
float fPeakSmoothing = 0.995f;
float fPeakSmoothValue = 0.0f;
float fAmplification = 1.0f;
float fftData[FFT_SIZE] = {0};
float fftSmoothingData[FFT_SIZE] = {0};
float fftNormalizedData[FFT_SIZE] = {0};
float fftNormalizedMaxData[FFT_SIZE] = {0};

bool fftEnabled = false;

#define FFT_DEBUG

FFT_LogLevel g_currentLogLevel = FFT_LOG_DEBUG;

void FFT_DebugLog(FFT_LogLevel level, const char* format, ...)
{
#ifdef FFT_DEBUG
    if (level <= g_currentLogLevel)
    {
        if (level == FFT_LOG_TRACE)
        {
            time_t now = time(NULL);
            struct tm* tm_now = localtime(&now);
            char time_str[20]; // ISO 8601 format requires 19 characters + null terminator
            strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", tm_now);

            printf("%s ", time_str);
        }
        va_list args;
        va_start(args, format);
        switch (level)
        {
            case FFT_LOG_TRACE:
                printf("[FFT TRACE]: ");
                break;
            case FFT_LOG_DEBUG:
                printf("[FFT DEBUG]: ");
                break;
            case FFT_LOG_INFO:
                printf("[FFT INFO]: ");
                break;
            case FFT_LOG_WARNING:
                printf("[FFT WARNING]: ");
                break;
            case FFT_LOG_ERROR:
                printf("[FFT ERROR]: ");
                break;
            case FFT_LOG_FATAL:
                printf("[FFT FATAL]: ");
                break;
            case FFT_LOG_OFF:
                return;
        }
        vprintf(format, args);
        va_end(args);
    }
#endif
}
#endif
