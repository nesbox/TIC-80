#pragma once

#include_next <math.h>

#define log2(x) (log (x) / M_LOG2_E)
#define log2f(x) (logf (x) / (float) M_LOG2_E)
#define M_LOG2_E        0.693147180559945309417
