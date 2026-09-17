#include "vqt_kernel.h"
#include "../vqtdata.h"

#ifndef TIC80_FFT_UNSUPPORTED

#define _USE_MATH_DEFINES
#include <math.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "kiss_fftr.h"

// Generate logarithmically spaced center frequencies for musical notes
// Keeps equal-tempered semitone spacing exact (2^(1/12)) without any scaling.
// Uses minFreq as the musical base and ignores maxFreq for placement.
void VQT_GenerateCenterFrequencies(float* frequencies, int numBins, float minFreq, float maxFreq)
{
    (void)maxFreq; // base is authoritative; maxFreq is not used to scale bins
    const double base = (double)minFreq;
    for (int i = 0; i < numBins; i++)
    {
        frequencies[i] = (float)(base * pow(2.0, (double)i / 12.0));
    }
}

// Calculate variable Q factor optimized for 8K FFT constraint
static float calculateVariableQ(float centerFreq)
{
    // Designed to maximize Q within 8K FFT window size limitation
    // Low-frequency windows are truncated to the available 8192 samples.
    if (centerFreq < 25.0f) return 7.4f;        // 20-25 Hz: Max possible with 8K
    else if (centerFreq < 30.0f) return 9.2f;   // 25-30 Hz: Good resolution
    else if (centerFreq < 40.0f) return 11.5f;  // 30-40 Hz: Better resolution
    else if (centerFreq < 50.0f) return 14.5f;  // 40-50 Hz: Near ideal
    else if (centerFreq < 65.0f) return 16.0f;  // 50-65 Hz: Almost full Q
    else if (centerFreq < 80.0f) return 17.0f;  // 65-80 Hz: Full standard Q
    else if (centerFreq < 160.0f) return 17.0f; // 80-160 Hz: Standard VQT
    else if (centerFreq < 320.0f) return 15.0f; // 160-320 Hz: Slightly wider
    else if (centerFreq < 640.0f) return 13.0f; // 320-640 Hz: Smoother
    else return 11.0f;                           // 640+ Hz: Very smooth
}

// Get adaptive sparsity threshold based on Q factor
static float getSparsityThreshold(float centerFreq)
{
    float Q = calculateVariableQ(centerFreq);
    // Higher Q needs lower threshold to preserve frequency selectivity
    if (Q > 30) return 0.005f;
    else if (Q > 20) return 0.01f;
    else return 0.02f;
}

// Generate Hamming window
static void generateHammingWindow(float* window, int length)
{
    const float a0 = 0.54f;
    const float a1 = 0.46f;

    for (int i = 0; i < length; i++)
    {
        window[i] = a0 - a1 * cos(2.0 * M_PI * i / (length - 1));
    }
}

// Generate Gaussian window
static void generateGaussianWindow(float* window, int length)
{
    const float sigma = 0.5f; // Standard deviation factor
    float center = (length - 1) / 2.0f;
    float normFactor = sigma * length / 2.0f;

    for (int i = 0; i < length; i++)
    {
        float x = (i - center) / normFactor;
        window[i] = exp(-0.5f * x * x);
    }
}

// Generate a single VQT kernel
static bool generateSingleKernel(
    VqtKernel* kernel,
    kiss_fftr_cfg fftCfg,
    int fftSize,
    float centerFreq,
    float minFreq,
    float sampleRate,
    VqtWindowType windowType,
    float sparsityThreshold)
{
    // Use variable Q optimized for 8K FFT
    float Q = calculateVariableQ(centerFreq);
    int windowLength = (int)(Q * sampleRate / centerFreq);

    // With 8K FFT and optimized variable Q:
    // 20Hz: Q=7.4 → 16,317 samples → truncated to 8,192 (still Q_eff ≈ 3.7)
    // 25Hz: Q=9.2 → 16,236 samples → truncated to 8,192 (Q_eff ≈ 4.6)
    // 30Hz: Q=11.5 → 16,870 samples → truncated to 8,192 (Q_eff ≈ 5.6)
    // 40Hz: Q=14.5 → 16,031 samples → truncated to 8,192 (Q_eff ≈ 7.4)
    // 50Hz: Q=16.0 → 14,112 samples → truncated to 8,192 (Q_eff ≈ 9.3)
    // 65Hz: Q=17.0 → 11,538 samples → truncated to 8,192 (Q_eff ≈ 12.1)
    // 80Hz+: All fit within 8K samples with designed Q!

    // Ensure it fits in FFT size
    if (windowLength > fftSize) {
        windowLength = fftSize;
        // Calculate effective Q after truncation
        float effectiveQ = windowLength * centerFreq / sampleRate;
        #ifdef VQT_DEBUG
        printf("Freq %.1f Hz: Q designed=%.1f, window=%d, Q effective=%.1f (truncated)\n",
               centerFreq, Q, windowLength, effectiveQ);
        #endif
    }

    // Ensure window length is reasonable
    if (windowLength < 32) windowLength = 32;  // Minimum window size

    // Allocate temporary arrays
    float* timeKernel = (float*)calloc(fftSize, sizeof(float));
    kiss_fft_cpx* freqKernel = (kiss_fft_cpx*)malloc((fftSize/2 + 1) * sizeof(kiss_fft_cpx));

    if (!timeKernel || !freqKernel)
    {
        free(timeKernel);
        free(freqKernel);
        return false;
    }

    // Generate window in the center of the kernel
    int windowStart = (fftSize - windowLength) / 2;

    // First generate the window
    float* tempWindow = (float*)malloc(windowLength * sizeof(float));
    if (!tempWindow)
    {
        free(timeKernel);
        free(freqKernel);
        return false;
    }

    switch (windowType)
    {
        case VQT_WINDOW_HAMMING:
            generateHammingWindow(tempWindow, windowLength);
            break;
        case VQT_WINDOW_GAUSSIAN:
            generateGaussianWindow(tempWindow, windowLength);
            break;
    }

    // Apply window and modulate with complex exponential
    // Following ESP32 reference: modulate based on full FFT size, not window size
    for (int i = 0; i < windowLength; i++)
    {
        int idx = windowStart + i;
        // Key insight: use idx (position in full FFT buffer) not i (position in window)
        // and center around N/2 where N is the full FFT size
        float phase = 2.0f * M_PI * (centerFreq / sampleRate) * (idx - fftSize/2);
        timeKernel[idx] = tempWindow[i] * cos(phase);
    }

    // Normalize by window length before FFT (like ESP32)
    for (int i = 0; i < fftSize; i++)
    {
        timeKernel[i] /= windowLength;
    }

    free(tempWindow);

    // Perform FFT
    kiss_fftr(fftCfg, timeKernel, freqKernel);

    // Count non-zero elements (above sparsity threshold)
    int nonZeroCount = 0;
    for (int i = 0; i <= fftSize/2; i++)
    {
        float magnitude = sqrt(freqKernel[i].r * freqKernel[i].r +
                              freqKernel[i].i * freqKernel[i].i);
        if (magnitude > sparsityThreshold)
        {
            nonZeroCount++;
        }
    }

    // Allocate sparse storage
    kernel->real = (float*)malloc(nonZeroCount * sizeof(float));
    kernel->imag = (float*)malloc(nonZeroCount * sizeof(float));
    kernel->indices = (int*)malloc(nonZeroCount * sizeof(int));
    kernel->length = nonZeroCount;

    if (!kernel->real || !kernel->imag || !kernel->indices)
    {
        free(kernel->real);
        free(kernel->imag);
        free(kernel->indices);
        memset(kernel, 0, sizeof *kernel);
        free(timeKernel);
        free(freqKernel);
        // tempWindow was already freed earlier
        return false;
    }

    // Store non-zero elements
    int sparseIndex = 0;
    int minIdx = fftSize, maxIdx = 0;  // Track range for debug
    for (int i = 0; i <= fftSize/2; i++)
    {
        float magnitude = sqrt(freqKernel[i].r * freqKernel[i].r +
                              freqKernel[i].i * freqKernel[i].i);
        if (magnitude > sparsityThreshold)
        {
            kernel->real[sparseIndex] = freqKernel[i].r;
            kernel->imag[sparseIndex] = freqKernel[i].i;
            kernel->indices[sparseIndex] = i;
            if (i < minIdx) minIdx = i;
            if (i > maxIdx) maxIdx = i;
            sparseIndex++;
        }
    }

    #ifdef VQT_DEBUG
    // Debug output for specific frequencies
    if (fabs(centerFreq - 110.0f) < 1.0f || fabs(centerFreq - 440.0f) < 1.0f)
    {
        printf("Kernel for %.1f Hz: FFT bins %d-%d (count: %d)\n",
               centerFreq, minIdx, maxIdx, nonZeroCount);
    }
    #endif

    free(timeKernel);
    free(freqKernel);
    return true;
}

// Generate VQT kernels for all bins
bool VQT_GenerateKernels(VqtKernel* kernels, const VqtKernelConfig* config)
{
    // Generate center frequencies
    float* centerFreqs = (float*)malloc(config->numBins * sizeof(float));
    if (!centerFreqs) return false;

    VQT_GenerateCenterFrequencies(centerFreqs, config->numBins,
                                  config->minFreq, config->maxFreq);

    // Create FFT configuration
    kiss_fftr_cfg fftCfg = kiss_fftr_alloc(config->fftSize, 0, NULL, NULL);
    if (!fftCfg)
    {
        free(centerFreqs);
        return false;
    }

    // Generate kernel for each VQT bin
    bool success = true;
    for (int i = 0; i < config->numBins; i++)
    {
        // Use adaptive sparsity threshold based on frequency
        float adaptiveThreshold = getSparsityThreshold(centerFreqs[i]);
        if (!generateSingleKernel(&kernels[i], fftCfg, config->fftSize,
                                 centerFreqs[i], config->minFreq,
                                 config->sampleRate, config->windowType,
                                 adaptiveThreshold))
        {
            // Clean up on failure
            for (int j = 0; j < i; j++)
            {
                free(kernels[j].real);
                free(kernels[j].imag);
                free(kernels[j].indices);
                memset(&kernels[j], 0, sizeof kernels[j]);
            }
            success = false;
            break;
        }
    }

    kiss_fft_free(fftCfg);
    free(centerFreqs);
    return success;
}

#else // TIC80_FFT_UNSUPPORTED

// Stub implementations when FFT is unsupported
void VQT_GenerateCenterFrequencies(float* frequencies, int numBins, float minFreq, float maxFreq) {}
bool VQT_GenerateKernels(VqtKernel* kernels, const VqtKernelConfig* config) { return false; }

#endif // TIC80_FFT_UNSUPPORTED
