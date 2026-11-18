#pragma once

#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
  #include <xmmintrin.h>
  #define TRANSDUCER_DENORMAL_X86 1
#elif defined(__aarch64__)
  #define TRANSDUCER_DENORMAL_A64 1
#endif

namespace transducer::common {

// RAII: flush denormal results/inputs to zero for the calling thread (x86 SSE FTZ+DAZ, AArch64
// FZ) and restore the previous mode on destruction. Place at the top of every audio callback;
// hosts do not guarantee it. State variables additionally carry explicit flush guards, so DSP
// stays cheap even where this is a no-op.
class ScopedFlushDenormals {
public:
    ScopedFlushDenormals() noexcept {
#if defined(TRANSDUCER_DENORMAL_X86)
        saved_ = _mm_getcsr();
        _mm_setcsr(saved_ | 0x8040u); // FTZ (bit 15) | DAZ (bit 6)
#elif defined(TRANSDUCER_DENORMAL_A64)
        uint64_t fpcr;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        saved_ = fpcr;
        asm volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
#endif
    }

    ~ScopedFlushDenormals() {
#if defined(TRANSDUCER_DENORMAL_X86)
        _mm_setcsr(saved_);
#elif defined(TRANSDUCER_DENORMAL_A64)
        asm volatile("msr fpcr, %0" : : "r"(saved_));
#endif
    }

    ScopedFlushDenormals(const ScopedFlushDenormals&) = delete;
    ScopedFlushDenormals& operator=(const ScopedFlushDenormals&) = delete;

    static constexpr bool supported() noexcept {
#if defined(TRANSDUCER_DENORMAL_X86) || defined(TRANSDUCER_DENORMAL_A64)
        return true;
#else
        return false;
#endif
    }

private:
#if defined(TRANSDUCER_DENORMAL_X86)
    unsigned int saved_{0};
#elif defined(TRANSDUCER_DENORMAL_A64)
    uint64_t saved_{0};
#endif
};

} // namespace transducer::common
