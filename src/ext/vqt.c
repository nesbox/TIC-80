#include "api.h"
#include "vqt.h"
#include "vqt_kernel.h"
#include "../vqtdata.h"
#include "../fftdata.h"

#ifndef TIC80_FFT_UNSUPPORTED

#include "fft.h"
#include "kiss_fftr.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <string.h>
#include <stdbool.h>
#include <stdio.h>


// FFT configuration for VQT
static kiss_fftr_cfg vqtFftCfg = NULL;
static float* vqtAudioBuffer = NULL;
static kiss_fft_cpx* vqtFftOutput = NULL;

// Initialize VQT processing
bool VQT_Open(void)
{
    VQT_Close();
    VQT_Init();

    // Allocate FFT buffers
    vqtAudioBuffer = (float*)malloc(VQT_FFT_SIZE * sizeof(float));
    vqtFftOutput = (kiss_fft_cpx*)malloc((VQT_FFT_SIZE/2 + 1) * sizeof(kiss_fft_cpx));

    if (!vqtAudioBuffer || !vqtFftOutput)
    {
        VQT_Close();
        return false;
    }

    // Create FFT configuration
    vqtFftCfg = kiss_fftr_alloc(VQT_FFT_SIZE, 0, NULL, NULL);
    if (!vqtFftCfg)
    {
        VQT_Close();
        return false;
    }

    // Configure VQT kernels
    VqtKernelConfig config = {
        .fftSize = VQT_FFT_SIZE,
        .numBins = VQT_BINS,
        .minFreq = VQT_MIN_FREQ,
        .maxFreq = VQT_MAX_FREQ,
        .sampleRate = 44100.0f,  // Standard TIC-80 sample rate
        .windowType = VQT_WINDOW_HAMMING,
        .sparsityThreshold = VQT_SPARSITY_THRESHOLD
    };

    // Generate kernels
    if (!VQT_GenerateKernels(vqtKernels, &config))
    {
        VQT_Close();
        return false;
    }

    vqtEnabled = true;
    return true;
}

// Apply VQT kernels to FFT output
void VQT_ApplyKernels(const float* fftReal, const float* fftImag)
{
    // Clear VQT output
    memset(vqtData, 0, sizeof(vqtData));

    // Apply each kernel to compute VQT bins
    for (int bin = 0; bin < VQT_BINS; bin++)
    {
        VqtKernel* kernel = &vqtKernels[bin];

        // Check if kernel is valid
        if (!kernel->real || !kernel->imag || !kernel->indices || kernel->length == 0)
        {
            vqtData[bin] = 0.0f;
            continue;
        }

        float real = 0.0f;
        float imag = 0.0f;

        // Sparse matrix multiplication
        for (int k = 0; k < kernel->length; k++)
        {
            int idx = kernel->indices[k];
            // Ensure index is within bounds
            if (idx < 0 || idx >= VQT_FFT_SIZE/2 + 1)
                continue;

            // Complex multiplication: (a + bi) * (c + di) = (ac - bd) + (ad + bc)i
            real += fftReal[idx] * kernel->real[k] - fftImag[idx] * kernel->imag[k];
            imag += fftReal[idx] * kernel->imag[k] + fftImag[idx] * kernel->real[k];
        }

        // Calculate magnitude with gain boost
        vqtData[bin] = sqrt(real * real + imag * imag) * 2.0f;  // Match FFT gain factor

        // Check for NaN or Inf
        if (!isfinite(vqtData[bin]))
            vqtData[bin] = 0.0f;
    }
}

// Process VQT from audio data
void VQT_ProcessAudio(void)
{
    if (!vqtFftCfg || !vqtEnabled) return;

    // Check if kernels are initialized
    bool kernelsValid = false;
    for (int i = 0; i < VQT_BINS; i++)
    {
        if (vqtKernels[i].real && vqtKernels[i].length > 0)
        {
            kernelsValid = true;
            break;
        }
    }

    if (!kernelsValid)
    {
        // Kernels not initialized, set all output to zero
        memset(vqtData, 0, sizeof(vqtData));
        memset(vqtSmoothingData, 0, sizeof(vqtSmoothingData));
        memset(vqtNormalizedData, 0, sizeof(vqtNormalizedData));
        return;
    }

    FFT_CopyAudio(vqtAudioBuffer, VQT_FFT_SIZE);
    kiss_fftr(vqtFftCfg, vqtAudioBuffer, vqtFftOutput);

    // Extract real and imaginary components for kernel application
    float fftReal[VQT_FFT_SIZE/2 + 1];
    float fftImag[VQT_FFT_SIZE/2 + 1];

    for (int i = 0; i <= VQT_FFT_SIZE/2; i++)
    {
        fftReal[i] = vqtFftOutput[i].r;
        fftImag[i] = vqtFftOutput[i].i;
    }

    VQT_ApplyKernels(fftReal, fftImag);

    // Spectral whitening: produce whitened copy into vqtWhiteData (raw vqtData remains unmodified)
#if VQT_SPECTRAL_WHITENING_ENABLED
    {
        const float eps = VQT_WHITENING_EPS;
        int width = VQT_WHITENING_WIDTH_BINS;
        if (width < 1) width = 1;
        int half = width / 2;
        float alpha = VQT_WHITENING_STRENGTH;
        if (alpha < 0.0f) alpha = 0.0f; else if (alpha > 1.0f) alpha = 1.0f;

        float logM[VQT_BINS];
        float env[VQT_BINS];

        for (int i = 0; i < VQT_BINS; i++)
        {
            float m = vqtData[i];
            if (!isfinite(m) || m < 0.0f) m = 0.0f;
            logM[i] = logf(m + eps);
        }

        for (int i = 0; i < VQT_BINS; i++)
        {
            int start = i - half;
            int end = i + half;
            if (start < 0) start = 0;
            if (end >= VQT_BINS) end = VQT_BINS - 1;
            float sum = 0.0f;
            int count = 0;
            for (int j = start; j <= end; j++) { sum += logM[j]; count++; }
            env[i] = count > 0 ? sum / (float)count : logM[i];
        }

        for (int i = 0; i < VQT_BINS; i++)
        {
            float m = vqtData[i];
            if (!isfinite(m) || m < 0.0f) m = 0.0f;
            if (m == 0.0f)
            {
                vqtWhiteData[i] = 0.0f;
                continue;
            }
            float wLog = logM[i] - env[i];
            float wAmp = expf(wLog) - 1.0f;
            if (!isfinite(wAmp) || wAmp < 0.0f) wAmp = 0.0f;
            float mp = (1.0f - alpha) * m + alpha * wAmp;
            if (!isfinite(mp) || mp < 0.0f) mp = 0.0f;
            vqtWhiteData[i] = mp;
        }
    }
#else
    // Whitening disabled: mirror raw into whitened buffers for A/B APIs
    for (int i = 0; i < VQT_BINS; i++) vqtWhiteData[i] = vqtData[i];
#endif

    // Apply smoothing to raw data
    for (int i = 0; i < VQT_BINS; i++)
    {
        vqtSmoothingData[i] = vqtSmoothingData[i] * VQT_SMOOTHING_FACTOR +
                              vqtData[i] * (1.0f - VQT_SMOOTHING_FACTOR);
    }

    // Apply smoothing to whitened data
    for (int i = 0; i < VQT_BINS; i++)
    {
        vqtWhiteSmoothingData[i] = vqtWhiteSmoothingData[i] * VQT_SMOOTHING_FACTOR +
                                   vqtWhiteData[i] * (1.0f - VQT_SMOOTHING_FACTOR);
    }

    // Find peak for normalization
    float currentPeak = 0.0f;
    for (int i = 0; i < VQT_BINS; i++)
    {
        if (vqtSmoothingData[i] > currentPeak)
            currentPeak = vqtSmoothingData[i];
    }

    // Initialize peak value if needed
    if (vqtPeakSmoothValue <= 0.0f)
        vqtPeakSmoothValue = 0.1f;

    // Smooth peak value
    if (currentPeak > vqtPeakSmoothValue)
        vqtPeakSmoothValue = currentPeak;
    else
        vqtPeakSmoothValue = vqtPeakSmoothValue * 0.99f + currentPeak * 0.01f;

    // Ensure peak value doesn't go too low
    if (vqtPeakSmoothValue < 0.0001f)
        vqtPeakSmoothValue = 0.0001f;

    // Normalize raw data
    float normalizer = 1.0f / vqtPeakSmoothValue;
    for (int i = 0; i < VQT_BINS; i++)
    {
        vqtNormalizedData[i] = vqtSmoothingData[i] * normalizer;
        if (vqtNormalizedData[i] > 1.0f)
            vqtNormalizedData[i] = 1.0f;

        // Final NaN check
        if (!isfinite(vqtNormalizedData[i]))
            vqtNormalizedData[i] = 0.0f;
    }

    // Peak for whitened normalization
    float currentWhitePeak = 0.0f;
    for (int i = 0; i < VQT_BINS; i++)
    {
        if (vqtWhiteSmoothingData[i] > currentWhitePeak)
            currentWhitePeak = vqtWhiteSmoothingData[i];
    }
    if (vqtWhitePeakSmoothValue <= 0.0f)
        vqtWhitePeakSmoothValue = 0.1f;
    if (currentWhitePeak > vqtWhitePeakSmoothValue)
        vqtWhitePeakSmoothValue = currentWhitePeak;
    else
        vqtWhitePeakSmoothValue = vqtWhitePeakSmoothValue * 0.99f + currentWhitePeak * 0.01f;
    if (vqtWhitePeakSmoothValue < 0.0001f)
        vqtWhitePeakSmoothValue = 0.0001f;

    // Normalize whitened data
    float normalizerW = 1.0f / vqtWhitePeakSmoothValue;
    for (int i = 0; i < VQT_BINS; i++)
    {
        vqtWhiteNormalizedData[i] = vqtWhiteSmoothingData[i] * normalizerW;
        if (vqtWhiteNormalizedData[i] > 1.0f)
            vqtWhiteNormalizedData[i] = 1.0f;
        if (!isfinite(vqtWhiteNormalizedData[i]))
            vqtWhiteNormalizedData[i] = 0.0f;
    }
}


// Close VQT processing and free resources
void VQT_Close(void)
{
    vqtEnabled = false;
    if (vqtFftCfg)
    {
        kiss_fft_free(vqtFftCfg);
        vqtFftCfg = NULL;
    }

    if (vqtAudioBuffer)
    {
        free(vqtAudioBuffer);
        vqtAudioBuffer = NULL;
    }

    if (vqtFftOutput)
    {
        free(vqtFftOutput);
        vqtFftOutput = NULL;
    }

    // Clean up kernels
    VQT_Cleanup();
}

// API functions for VQT
double tic_api_vqt(tic_mem* memory, s32 bin)
{
    // Validate bin range
    if (!fftEnabled || !vqtEnabled || bin < 0 || bin >= VQT_BINS)
        return 0.0;

    // Return normalized VQT data
    return vqtData[bin] / vqtPeakSmoothValue;
}

double tic_api_vqts(tic_mem* memory, s32 bin)
{
    // Validate bin range
    if (!fftEnabled || !vqtEnabled || bin < 0 || bin >= VQT_BINS)
        return 0.0;

    // Return smoothed normalized VQT data
    return vqtNormalizedData[bin];
}

// Raw (non-normalized) VQT access functions
double tic_api_vqtr(tic_mem* memory, s32 bin)
{
    // Validate bin range
    if (!fftEnabled || !vqtEnabled || bin < 0 || bin >= VQT_BINS)
        return 0.0;

    // Return raw VQT data (non-normalized)
    return vqtData[bin];
}

double tic_api_vqtrs(tic_mem* memory, s32 bin)
{
    // Validate bin range
    if (!fftEnabled || !vqtEnabled || bin < 0 || bin >= VQT_BINS)
        return 0.0;

    // Return raw smoothed VQT data (non-normalized)
    return vqtSmoothingData[bin];
}

// Whitened VQT API
double tic_api_vqtw(tic_mem* memory, s32 bin)
{
    if (!fftEnabled || !vqtEnabled || bin < 0 || bin >= VQT_BINS) return 0.0;
    return vqtWhiteData[bin] / vqtWhitePeakSmoothValue;
}

double tic_api_vqtsw(tic_mem* memory, s32 bin)
{
    if (!fftEnabled || !vqtEnabled || bin < 0 || bin >= VQT_BINS) return 0.0;
    return vqtWhiteNormalizedData[bin];
}

double tic_api_vqtrw(tic_mem* memory, s32 bin)
{
    if (!fftEnabled || !vqtEnabled || bin < 0 || bin >= VQT_BINS) return 0.0;
    return vqtWhiteData[bin];
}

double tic_api_vqtrsw(tic_mem* memory, s32 bin)
{
    if (!fftEnabled || !vqtEnabled || bin < 0 || bin >= VQT_BINS) return 0.0;
    return vqtWhiteSmoothingData[bin];
}

#else // TIC80_FFT_UNSUPPORTED

// Stub implementations when FFT is unsupported
bool VQT_Open(void) { return false; }
void VQT_ProcessAudio(void) {}
void VQT_Close(void) {}
void VQT_ApplyKernels(const float* fftReal, const float* fftImag) {}

// API stubs when FFT is unsupported
double tic_api_vqt(tic_mem* memory, s32 bin) { return 0.0; }
double tic_api_vqts(tic_mem* memory, s32 bin) { return 0.0; }
double tic_api_vqtr(tic_mem* memory, s32 bin) { return 0.0; }
double tic_api_vqtrs(tic_mem* memory, s32 bin) { return 0.0; }

double tic_api_vqtw(tic_mem* memory, s32 bin) { return 0.0; }
double tic_api_vqtsw(tic_mem* memory, s32 bin) { return 0.0; }
double tic_api_vqtrw(tic_mem* memory, s32 bin) { return 0.0; }
double tic_api_vqtrsw(tic_mem* memory, s32 bin) { return 0.0; }

#endif // TIC80_FFT_UNSUPPORTED
