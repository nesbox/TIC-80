#include <assert.h>

#if (defined(DINGUX) || defined(__DJGPP__)) && !defined(static_assert)
#define static_assert _Static_assert
#endif
