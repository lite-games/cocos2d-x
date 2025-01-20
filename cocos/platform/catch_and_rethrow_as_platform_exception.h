#ifndef __CCCATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION__
#define __CCCATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION__

#include "CCPlatformConfig.h"

#include "base/CCRuntimeError.h"

#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)

#include <jni.h>

#endif

NS_CC_BEGIN
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)

    void rethrowAsJavaException(JNIEnv *env, const std::exception_ptr &exPtr);

    // Using macros instead of catch_and_rethrow() function to avoid showing it on stacktraces.
    // For now, let's catch only RuntimeError as it has a stacktrace attached.
    //   Let other native exceptions pass and create a crashdump as earlier.
    //   Which will result in Crashlytics showing
    //     a decent stacktrace under SIGABRT crash report
    //     although lacking an error message.
#define CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_BEGIN try {
#define CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_END(javaEnv) \
    } catch (cocos2d::RuntimeError &ex) { \
        cocos2d::rethrowAsJavaException(javaEnv, std::current_exception()); \
    }
#define CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_END_RET(javaEnv, ret) \
    } catch (cocos2d::RuntimeError &ex) { \
        cocos2d::rethrowAsJavaException(javaEnv, std::current_exception()); \
        return (ret); \
    }

#else

    #define CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_BEGIN
#define CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_END(javaEnv)
#define CC_CATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION_END_RET(javaEnv, ret)

#endif // CC_TARGET_PLATFORM
NS_CC_END

#endif //__CCCATCH_AND_RETHROW_AS_PLATFORM_EXCEPTION__
