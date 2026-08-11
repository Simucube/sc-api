/**
 * @file
 * @brief Small helpers that hide compiler differences.
 */

#ifndef SC_API_COMPATIBILITY_H_
#define SC_API_COMPATIBILITY_H_

#ifdef _MSC_VER
#include <immintrin.h>
#endif

namespace sc_api::compatibility {

/** Hint instruction that helps with spinlock performance */
inline void spinlockPauseInstr() {
#ifdef _MSC_VER
    _mm_pause();
#else
    asm volatile("rep; nop" ::: "memory");
#endif
}

}  // namespace sc_api::compatibility

#endif  // SC_API_COMPATIBILITY_H_
