#include <assert.h>

#if (defined(DINGUX) || defined(__DJGPP__)) && !defined(static_assert)
#define static_assert _Static_assert
#endif

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 5 
#define static_assert__(x,line) static char stat_ass ## line[(x) ? +1 : -1]
#define static_assert_(x,line) static_assert__(x,line)
#define static_assert(x,y) static_assert_(x, __LINE__)
#endif
