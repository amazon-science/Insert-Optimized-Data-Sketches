#pragma once

#define LIKELY(cond) (__builtin_expect(!!(cond), 1))
#define UNLIKELY(cond) (__builtin_expect(!!(cond), 0))

#define ALWAYS_INLINE __attribute__((always_inline, artificial)) inline

#ifdef OPTLEVEL_0
#define OPT_INLINE
#else
#define OPT_INLINE __attribute__((always_inline)) inline
#endif
