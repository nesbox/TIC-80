#pragma once

#include <stdbool.h>
#include "tic80_types.h"

// Initialize VQT processing
// Returns true on success, false on failure
bool VQT_Open(void);

// Process VQT from audio buffer (uses shared audio capture buffer)
void VQT_ProcessAudio(void);

// Close VQT processing and free resources
void VQT_Close(void);

// Apply VQT kernels to FFT output
void VQT_ApplyKernels(const float* fftReal, const float* fftImag);
