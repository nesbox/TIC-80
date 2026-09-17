#include "vqtdata.h"

#ifndef TIC80_FFT_UNSUPPORTED

#include <string.h>
#include <stdlib.h>

// Global VQT data arrays
float vqtData[VQT_BINS];
float vqtSmoothingData[VQT_BINS];
float vqtNormalizedData[VQT_BINS];

// Peak tracking for auto-gain
float vqtPeakSmoothValue = 1.0f;

// Enable flag (tied to fftEnabled initially)
bool vqtEnabled = false;

// Array of kernels, one per VQT bin
VqtKernel vqtKernels[VQT_BINS];

// Whitened data arrays
float vqtWhiteData[VQT_BINS];
float vqtWhiteSmoothingData[VQT_BINS];
float vqtWhiteNormalizedData[VQT_BINS];
float vqtWhitePeakSmoothValue = 1.0f;


void VQT_Init(void)
{
    // Zero all data arrays
    memset(vqtData, 0, sizeof(vqtData));
    memset(vqtSmoothingData, 0, sizeof(vqtSmoothingData));
    memset(vqtNormalizedData, 0, sizeof(vqtNormalizedData));
    memset(vqtWhiteData, 0, sizeof(vqtWhiteData));
    memset(vqtWhiteSmoothingData, 0, sizeof(vqtWhiteSmoothingData));
    memset(vqtWhiteNormalizedData, 0, sizeof(vqtWhiteNormalizedData));

    // Initialize peak value
    vqtPeakSmoothValue = 1.0f;
    vqtWhitePeakSmoothValue = 1.0f;

    // Zero kernel pointers
    memset(vqtKernels, 0, sizeof(vqtKernels));
}

void VQT_Cleanup(void)
{
    // Free all allocated kernel memory
    for (int i = 0; i < VQT_BINS; i++)
    {
        if (vqtKernels[i].real)
        {
            free(vqtKernels[i].real);
            vqtKernels[i].real = NULL;
        }
        if (vqtKernels[i].imag)
        {
            free(vqtKernels[i].imag);
            vqtKernels[i].imag = NULL;
        }
        if (vqtKernels[i].indices)
        {
            free(vqtKernels[i].indices);
            vqtKernels[i].indices = NULL;
        }
        vqtKernels[i].length = 0;
    }
}

#else // TIC80_FFT_UNSUPPORTED

// When FFT is unsupported, provide empty implementations
float vqtData[VQT_BINS] = {0};
float vqtSmoothingData[VQT_BINS] = {0};
float vqtNormalizedData[VQT_BINS] = {0};
float vqtPeakSmoothValue = 1.0f;
bool vqtEnabled = false;
VqtKernel vqtKernels[VQT_BINS] = {{0}};

void VQT_Init(void) {}
void VQT_Cleanup(void) {}

#endif // TIC80_FFT_UNSUPPORTED
