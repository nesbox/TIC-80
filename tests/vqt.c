/* Offline spectral and lifecycle regression test (no capture device required).
 * cc -Isrc -Iinclude tests/vqt.c src/ext/fft.c src/fftdata.c src/vqtdata.c
 *    src/ext/vqt.c src/ext/vqt_kernel.c src/ext/kiss_fft.c src/ext/kiss_fftr.c
 *    -lm -o vqt-test
 * Add -DTIC80_FFT_UNSUPPORTED to test all eight API stubs.
 */
#include "api.h"
#include "fftdata.h"
#include "vqtdata.h"
#include "ext/fft.h"
#include "ext/vqt.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static double (*const apis[])(tic_mem*, s32) = {
    tic_api_vqt, tic_api_vqts, tic_api_vqtr, tic_api_vqtrs,
    tic_api_vqtw, tic_api_vqtsw, tic_api_vqtrw, tic_api_vqtrsw,
};

#ifndef TIC80_FFT_UNSUPPORTED
#include "ext/miniaudio.h"
#include "ext/kiss_fftr.h"
extern void OnReceiveFrames(ma_device*, void*, const void*, ma_uint32);
extern kiss_fftr_cfg fftcfg;

static void tone(int bin, float amplitude)
{
    float stereo[VQT_FFT_SIZE * 2];
    double freq = VQT_MIN_FREQ * pow(2.0, bin / 12.0);
    for (int i = 0; i < VQT_FFT_SIZE; i++)
        stereo[i*2] = stereo[i*2+1] = amplitude * sin(2.0 * 3.14159265358979323846 * freq * i / 44100.0);
    OnReceiveFrames(NULL, NULL, stereo, VQT_FFT_SIZE);
    VQT_ProcessAudio();
}

static void check_values(void)
{
    for (unsigned a = 0; a < sizeof apis / sizeof *apis; a++)
    {
        assert(apis[a](NULL, -1) == 0);
        assert(apis[a](NULL, VQT_BINS) == 0);
        for (int bin = 0; bin < VQT_BINS; bin++)
        {
            double value = apis[a](NULL, bin);
            assert(isfinite(value) && value >= 0);
        }
    }
}
#endif

int main(void)
{
    for (unsigned a = 0; a < sizeof apis / sizeof *apis; a++)
        assert(apis[a](NULL, 54) == 0);
#ifndef TIC80_FFT_UNSUPPORTED
    fftEnabled = true;
    assert(VQT_Open());
    // Fresh silence must produce zero across all variants, including whitening.
    OnReceiveFrames(NULL, NULL, NULL, VQT_FFT_SIZE);
    VQT_ProcessAudio();
    for (unsigned a = 0; a < sizeof apis / sizeof *apis; a++)
        for (int b = 0; b < VQT_BINS; b++) assert(apis[a](NULL, b) == 0);

    const int bins[] = {30, 42, 54, 66, 78, 90};
    for (unsigned b = 0; b < sizeof bins / sizeof *bins; b++)
    {
        assert(VQT_Open());
        tone(bins[b], 0.25f);
        int peak = 0;
        for (int i = 1; i < VQT_BINS; i++)
            if (vqtData[i] > vqtData[peak]) peak = i;
        printf("Tone bin %d: peak bin %d\n", bins[b], peak);
        assert(peak == bins[b]);
        double raw = tic_api_vqtr(NULL, peak);
        assert(raw > 0);
        assert(fabs(tic_api_vqtrs(NULL,peak) / raw - 0.7) < 1e-5);
        tone(bins[b], 0.5f);
        assert(fabs(tic_api_vqtr(NULL,peak) / raw - 2.0) < 1e-4);
        check_values();
    }
    // Oversized stereo callbacks retain their most recent frames, and the 2K
    // FFT reads the tail of the expanded 8K capture buffer.
    float stereo[(AUDIO_BUFFER_SIZE + 16) * 2];
    for (int i = 0; i < AUDIO_BUFFER_SIZE + 16; i++)
        stereo[i*2] = stereo[i*2+1] = i;
    OnReceiveFrames(NULL,NULL,stereo,AUDIO_BUFFER_SIZE + 16);
    float tail[FFT_SIZE * 2];
    FFT_CopyAudio(tail, FFT_SIZE * 2);
    assert(tail[0] == AUDIO_BUFFER_SIZE + 16 - FFT_SIZE * 2);
    assert(tail[FFT_SIZE * 2 - 1] == AUDIO_BUFFER_SIZE + 15);
    fftcfg = kiss_fftr_alloc(FFT_SIZE*2, 0, NULL, NULL);
    assert(fftcfg);
    fAmplification = 1;
    FFT_GetFFT(fftData);
    double expected = 0;
    for (int i=0; i<FFT_SIZE*2; i++) expected += tail[i] * 2;
    assert(fabs(fftData[0] / expected - 1.0) < 1e-5);
    kiss_fft_free(fftcfg);
    VQT_Close();
    VQT_Close();
    for (unsigned a = 0; a < sizeof apis / sizeof *apis; a++)
        assert(apis[a](NULL, 54) == 0);
    assert(VQT_Open());
    fftEnabled = false;
    for (unsigned a = 0; a < sizeof apis / sizeof *apis; a++)
        assert(apis[a](NULL, 54) == 0);
    VQT_Close();
#endif
    puts("VQT tones, gain, smoothing, whitening, capture windows and lifecycle checks passed");
    return 0;
}
