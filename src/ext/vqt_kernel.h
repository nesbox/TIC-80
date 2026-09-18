#pragma once

#include <stdbool.h>
#include "../vqtdata.h"

// Window types for VQT kernels
typedef enum {
    VQT_WINDOW_HAMMING = 0,
    VQT_WINDOW_GAUSSIAN
} VqtWindowType;

// VQT kernel configuration
typedef struct {
    int fftSize;              // FFT size (4096)
    int numBins;              // Number of VQT bins (120)
    float minFreq;            // Minimum frequency (20 Hz)
    float maxFreq;            // Maximum frequency (20480 Hz)
    float sampleRate;         // Sample rate (44100 Hz)
    VqtWindowType windowType; // Window function type
    float sparsityThreshold;  // Threshold for sparse matrix (0.01)
} VqtKernelConfig;

// Generate VQT kernels for all bins
// Returns true on success, false on failure
bool VQT_GenerateKernels(VqtKernel* kernels, const VqtKernelConfig* config);

// Generate center frequencies for VQT bins
void VQT_GenerateCenterFrequencies(float* frequencies, int numBins, float minFreq, float maxFreq);
