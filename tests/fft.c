/* Offline regression test: no capture device is opened.
 * cc -Isrc -Iinclude tests/fft.c src/ext/fft.c src/fftdata.c
 *    src/ext/kiss_fft.c src/ext/kiss_fftr.c -lm -o fft-test
 * Add -DTIC80_FFT_UNSUPPORTED to exercise the no-capture API stubs.
 */
#include "api.h"
#include "fftdata.h"
#include "ext/fft.h"
#include "ext/kiss_fftr.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

#ifndef TIC80_FFT_UNSUPPORTED
extern float sampleBuf[];
extern kiss_fftr_cfg fftcfg;

#ifndef AUDIO_BUFFER_SIZE
#define AUDIO_BUFFER_SIZE (FFT_SIZE * 2)
#endif

static void tone(float amplitude)
{
    for (int i = 0; i < FFT_SIZE * 2; i++)
        sampleBuf[AUDIO_BUFFER_SIZE - FFT_SIZE * 2 + i] = amplitude * sinf(2.0f * 3.14159265358979323846f * 32 * i / (FFT_SIZE * 2));
    FFT_GetFFT(fftData);
}

static void near(double actual, double expected)
{
    assert(fabs(actual - expected) < 0.001 * fmax(1.0, fabs(expected)));
}
#endif

int main(void)
{
    assert(tic_api_fftr(NULL, 32, -1) == 0);
    assert(tic_api_fftrs(NULL, 32, -1) == 0);
#ifndef TIC80_FFT_UNSUPPORTED
    g_currentLogLevel = FFT_LOG_OFF;
    fftcfg = kiss_fftr_alloc(FFT_SIZE * 2, 0, NULL, NULL);
    assert(fftcfg);
    fftEnabled = true;
    fAmplification = 7.0f;
    tone(0.25f);
    double raw = tic_api_fftr(NULL, 32, -1);
    near(raw, 512.0);
    near(tic_api_fft(NULL, 32, -1), raw * 7.0);
    near(tic_api_fftrs(NULL, 32, -1), raw * 0.4);
    tone(0.5f);
    near(tic_api_fftr(NULL, 32, -1), raw * 2.0);
    near(tic_api_fftrs(NULL, 32, -1), raw * (0.4 * 0.6 + 2.0 * 0.4));
    for (int i = 0; i < FFT_SIZE; i++)
    {
        fftRawData[i] = fftData[i] = i + 1;
        fftRawSmoothingData[i] = fftSmoothingData[i] = (i + 1) * 0.5f;
    }
    near(tic_api_fftr(NULL, 1, 3), 9);
    near(tic_api_fftrs(NULL, 1, 3), 4.5);
    const int ranges[][2] = {{-2,-1},{1024,-1},{-5,3},{1022,1100},{5,3},{1025,5},{2,-2},{-3,-2}};
    for (unsigned i = 0; i < sizeof ranges / sizeof *ranges; i++)
    {
        int start = ranges[i][0], end = ranges[i][1];
        near(tic_api_fftr(NULL,start,end), tic_api_fft(NULL,start,end));
        near(tic_api_fftrs(NULL,start,end), tic_api_ffts(NULL,start,end));
    }
    fftEnabled = false;
    assert(tic_api_fftr(NULL, 0, 1023) == 0);
    assert(tic_api_fftrs(NULL, 0, 1023) == 0);
    kiss_fft_free(fftcfg);
#endif
    puts("FFT raw magnitude, smoothing, range and disabled API checks passed");
    return 0;
}
