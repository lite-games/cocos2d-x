#ifndef __CCRUNTIME_ERROR_H__
#define __CCRUNTIME_ERROR_H__

#include "platform/CCPlatformConfig.h"
#include "base/CCStackTrace.h"
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
#include "platform/android/jni/JniHelper.h"
#include <jni.h>
#endif
#include <stdexcept>

NS_CC_BEGIN
class RuntimeError : public std::runtime_error
{
public:
    explicit RuntimeError(
        const std::string& what_arg,
        StackTrace stackTrace = stacktrace_capture()
    ) :
        std::runtime_error(what_arg),
        stackTrace(stackTrace) { }

    #if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
    explicit RuntimeError(
        jthrowable jThrowable,
        StackTrace stackTrace = stacktrace_capture()
    ) :
        std::runtime_error("Unhandled java error"),
        jThrowable((jthrowable) JniHelper::getEnv()->NewGlobalRef(jThrowable)),
        stackTrace(stackTrace) { }
    #endif

    explicit RuntimeError(
        const char* what_arg,
        StackTrace stackTrace = stacktrace_capture()
    ) :
        std::runtime_error(what_arg),
        stackTrace(stackTrace) { }

    ~RuntimeError() override
    {
        #if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
        if (jThrowable) {
            JniHelper::getEnv()->DeleteGlobalRef(jThrowable);
            jThrowable = nullptr;
        }
        #endif
    }

    #if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
    jthrowable getThrowable() const { return jThrowable; }
    #endif

    const StackTrace& getStackTrace() const { return stackTrace; }

private:
    #if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
    jthrowable jThrowable = nullptr;
    #endif
    StackTrace stackTrace;
};

NS_CC_END

#endif //__CCRUNTIME_ERROR_H__
