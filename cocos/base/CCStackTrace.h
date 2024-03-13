#ifndef __CCSTACKTRACE_H__
#define __CCSTACKTRACE_H__

#include "platform/CCPlatformMacros.h"
#include <string>
#include <functional>

#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)

#include <unwind.h>

#endif


NS_CC_BEGIN
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
    const size_t StackTraceMaxDepth = 30;
#else
    // On platforms that doesn't allow to get stacktrace, stack trace can be empty.
    const size_t StackTraceMaxDepth = 0;
#endif // CC_TARGET_PLATFORM

    typedef struct StackTrace {
        size_t depth;
        void *elements[StackTraceMaxDepth];
    } StackTrace;

#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
    struct StackTraceUnwindState {
        void **current;
        void **end;
    };

    _Unwind_Reason_Code stacktrace_unwind_callback(struct _Unwind_Context *context, void *arg);

    // Call to stacktrace_capture() is force inlined to avoid showing it on stack traces.
    static inline __attribute__((always_inline)) StackTrace stacktrace_capture() {
        // NOTE: libunwind tends to SIGSEGV on some Android 12 devices. -- mz, 2024-03-13
        // The following devices are affected: Samsung Galaxy Tab S6, Samsung Galaxy Fold

        StackTrace stacktrace;

        StackTraceUnwindState state = {
                stacktrace.elements,
                stacktrace.elements + StackTraceMaxDepth
        };
        _Unwind_Backtrace(stacktrace_unwind_callback, &state);

        stacktrace.depth = state.current - stacktrace.elements;

        return stacktrace;
    }

    void stacktrace_dump(
            const StackTrace &stacktrace,
            std::function<void(
                    size_t idx,
                    const void *address,
                    const void *base_address,
                    const char *symbol,
                    const char *libpath)> fn
    );

    struct DemangledSymbolInfo {
        std::string className;
        std::string methodName;
    };

    DemangledSymbolInfo demangleSymbol(
            const void *address,
            const void *base_address,
            const char *symbol,
            const char *libpath,
            char *&demangledSymbolBuf
    );
#else
    static inline __attribute__((always_inline)) StackTrace stacktrace_capture() {
        return StackTrace {.depth = 0};
    }
#endif // CC_TARGET_PLATFORM

NS_CC_END

#endif //__CCSTACKTRACE_H__
