#pragma once
#include <stdbool.h>
#include "tic80_config.h"

#define VQT_BINS 120
#define VQT_FFT_SIZE 8192   // 8192 samples at 44100 Hz (~186 ms window)

// VQT frequency range
// Use a musical base note for exact semitone alignment across bins.
// D#0/Eb0 ≈ 19.445 Hz; over 10 octaves (120 semitones) this reaches ≈18.8 kHz.
// This keeps every bin on a real note and fits within the audible band.
#define VQT_MIN_FREQ 19.445f   // D#0 / Eb0 base (A4=440)
#define VQT_MAX_FREQ 20480.0f  // Upper reference (not used to scale bins)

// Smoothing parameters
#define VQT_SMOOTHING_FACTOR 0.3f  // Reduced from 0.7f for more responsive display
#define VQT_SPARSITY_THRESHOLD 0.01f

// Spectral whitening configuration
#ifndef VQT_SPECTRAL_WHITENING_ENABLED
#define VQT_SPECTRAL_WHITENING_ENABLED 1
#endif

#ifndef VQT_WHITENING_WIDTH_BINS
#define VQT_WHITENING_WIDTH_BINS 21   // odd window width for envelope smoothing
#endif

#ifndef VQT_WHITENING_STRENGTH
#define VQT_WHITENING_STRENGTH 0.95f   // 0..1 mix toward whitened spectrum
#endif

#ifndef VQT_WHITENING_EPS
#define VQT_WHITENING_EPS 1e-6f       // floor to stabilize log domain
#endif

// Raw VQT magnitude data
extern float vqtData[VQT_BINS];

// Smoothed VQT data for visual stability
extern float vqtSmoothingData[VQT_BINS];

// Normalized VQT data (0-1 range)
extern float vqtNormalizedData[VQT_BINS];

// Peak tracking for auto-gain
extern float vqtPeakSmoothValue;

// Enable flag (tied to fftEnabled initially)
extern bool vqtEnabled;

// Whitened VQT data (dual outputs)
extern float vqtWhiteData[VQT_BINS];
extern float vqtWhiteSmoothingData[VQT_BINS];
extern float vqtWhiteNormalizedData[VQT_BINS];
extern float vqtWhitePeakSmoothValue;

// Sparse kernel storage structures
typedef struct {
    float* real;      // Real parts of kernel (sparse)
    float* imag;      // Imaginary parts of kernel (sparse)
    int* indices;     // Non-zero FFT bin indices
    int length;       // Number of non-zero elements
} VqtKernel;

// Array of kernels, one per VQT bin
extern VqtKernel vqtKernels[VQT_BINS];

// Initialize VQT data structures
void VQT_Init(void);

// Free VQT kernel memory
void VQT_Cleanup(void);
