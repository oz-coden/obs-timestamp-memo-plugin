#pragma once
// OBS 31.1.1 selects this MSVC intrinsic in util_uint64.h, but clang's
// intrin.h omits its declaration. Supply only the declaration for analysis;
// production builds continue to use the real MSVC header.
#if defined(__clang__) && defined(_MSC_VER) && defined(_M_X64)
extern "C" unsigned long long _udiv128(unsigned long long, unsigned long long, unsigned long long,
				       unsigned long long *);
#endif
