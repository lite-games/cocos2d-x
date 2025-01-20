#include "catch_and_rethrow_as_platform_exception.h"

#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)

#include <string>
#include <vector>

#endif // CC_TARGET_PLATFORM

NS_CC_BEGIN
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)

    static jobjectArray concatenateArrays(
            JNIEnv *env,
            jclass elementClass,
            const std::vector<jobjectArray> &arrays
    ) {
        jsize finalArrayLength = 0;
        for (auto const array: arrays) {
            finalArrayLength += env->GetArrayLength(array);
        }

        env->PushLocalFrame(1 + finalArrayLength);

        auto finalArray = env->NewObjectArray(
                finalArrayLength,
                elementClass,
                nullptr
        );

        jsize finalArrayIdx = 0;
        for (auto const array: arrays) {
            auto arrayLength = env->GetArrayLength(array);

            for (jsize arrayElementIdx = 0; arrayElementIdx < arrayLength; ++arrayElementIdx) {
                auto element = env->GetObjectArrayElement(array, arrayElementIdx);
                env->SetObjectArrayElement(
                        finalArray,
                        finalArrayIdx++,
                        element
                );
            }
        }

        return (jobjectArray) env->PopLocalFrame((jobject) finalArray);
    }

    static jthrowable createJavaException(
            JNIEnv *env,
            const char *msg,
            jthrowable jThrowable,
            const StackTrace &stackTrace,
            jthrowable cause,
            bool keepJavaStackTrace
    ) {
        env->PushLocalFrame(
                // classes
                2
                // java objects
                + 5
        );

        auto RuntimeExceptionClass = env->FindClass("java/lang/RuntimeException");
        auto RuntimeExceptionClass_Init = env->GetMethodID(
                RuntimeExceptionClass,
                "<init>",
                "(Ljava/lang/String;Ljava/lang/Throwable;)V"
        );
        auto RuntimeExceptionClass_getStackTraceMethod = env->GetMethodID(
                RuntimeExceptionClass,
                "getStackTrace",
                "()[Ljava/lang/StackTraceElement;"
        );
        auto RuntimeExceptionClass_setStackTraceMethod = env->GetMethodID(
                RuntimeExceptionClass,
                "setStackTrace",
                "([Ljava/lang/StackTraceElement;)V"
        );
        auto StackTraceElementClass = env->FindClass("java/lang/StackTraceElement");

        auto nativeStackTrace = createStackTrace(env, stackTrace);

        jthrowable javaException;
        if (jThrowable) {
            // Java exception coming from JNI should always be the main cause.
            assert(cause == nullptr);

            javaException = jThrowable;

            // append native stacktrace to java stacktrace
            auto javaStackTrace = (jobjectArray) env->CallObjectMethod(
                javaException,
                RuntimeExceptionClass_getStackTraceMethod
            );

            auto finalStackTrace = concatenateArrays(
                env,
                StackTraceElementClass,
                {javaStackTrace, nativeStackTrace}
            );

            env->CallVoidMethod(
                javaException,
                RuntimeExceptionClass_setStackTraceMethod,
                finalStackTrace
            );
        } else {
            auto exceptionMessage = env->NewStringUTF(msg);
            javaException = (jthrowable) env->NewObject(
                RuntimeExceptionClass,
                RuntimeExceptionClass_Init,
                exceptionMessage,
                cause
            );

            if (keepJavaStackTrace) {
                // append java stack trace to native stacktrace
                auto javaStackTrace = (jobjectArray) env->CallObjectMethod(
                    javaException,
                    RuntimeExceptionClass_getStackTraceMethod
                );

                auto finalStackTrace = concatenateArrays(
                    env,
                    StackTraceElementClass,
                    {nativeStackTrace, javaStackTrace}
                );

                env->CallVoidMethod(
                    javaException,
                    RuntimeExceptionClass_setStackTraceMethod,
                    finalStackTrace
                );
            } else {
                // report only native stacktrace
                env->CallVoidMethod(
                    javaException,
                    RuntimeExceptionClass_setStackTraceMethod,
                    nativeStackTrace
                );
            }
        }

        return (jthrowable) env->PopLocalFrame(javaException);
    }

    static jthrowable createJavaException(JNIEnv *env, const std::exception_ptr &exPtr) {
        struct ExceptionInfo {
            std::string msg;
            #if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
            jthrowable jThrowable;
            #endif
            StackTrace stackTrace;
        };

        std::exception_ptr exceptionPtr = exPtr;
        std::vector<ExceptionInfo> exceptionStack;
        while (exceptionPtr) {
            try {
                std::rethrow_exception(exceptionPtr);
            } catch (const RuntimeError &ex) {
                exceptionStack.push_back(
                    ExceptionInfo{
                        .msg = ex.what(),
                        #if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
                        .jThrowable = ex.getThrowable(),
                        #else
                        .jThrowable = nullptr,
                        #endif
                        .stackTrace = ex.getStackTrace(),
                    }
                );

                try {
                    std::rethrow_if_nested(ex);
                    exceptionPtr = nullptr;
                } catch (...) {
                    exceptionPtr = std::current_exception();
                }
            } catch (const std::exception &ex) {
                exceptionStack.push_back(
                    ExceptionInfo{
                        .msg = ex.what(),
                        .jThrowable = nullptr,
                        .stackTrace = StackTrace{.depth = 0},
                    }
                );

                try {
                    std::rethrow_if_nested(ex);
                    exceptionPtr = nullptr;
                } catch (...) {
                    exceptionPtr = std::current_exception();
                }
            } catch (const std::string &ex) {
                exceptionStack.push_back(
                    ExceptionInfo{
                        .msg = ex,
                        .jThrowable = nullptr,
                        .stackTrace = StackTrace{.depth = 0},
                    }
                );

                try {
                    std::rethrow_if_nested(std::current_exception());
                    exceptionPtr = nullptr;
                } catch (...) {
                    exceptionPtr = std::current_exception();
                }
            } catch (const char *ex) {
                exceptionStack.push_back(
                    ExceptionInfo{
                        .msg = ex,
                        .jThrowable = nullptr,
                        .stackTrace = StackTrace{.depth = 0},
                    }
                );

                try {
                    std::rethrow_if_nested(std::current_exception());
                    exceptionPtr = nullptr;
                } catch (...) {
                    exceptionPtr = std::current_exception();
                }
            } catch (...) {
                exceptionStack.push_back(
                    ExceptionInfo{
                        .msg = "Unknown exception",
                        .jThrowable = nullptr,
                        .stackTrace = StackTrace{.depth = 0},
                    }
                );

                try {
                    std::rethrow_if_nested(std::current_exception());
                    exceptionPtr = nullptr;
                } catch (...) {
                    exceptionPtr = std::current_exception();
                }
            }
        }

        env->PushLocalFrame((jint) exceptionStack.size());
        jthrowable javaException = nullptr;
        for (size_t idx = exceptionStack.size(); idx > 0; --idx) {
            size_t depth = idx - 1;
            const auto &exceptionInfo = exceptionStack[depth];

            javaException = createJavaException(
                env,
                exceptionInfo.msg.c_str(),
                exceptionInfo.jThrowable,
                exceptionInfo.stackTrace,
                javaException,
                depth == 0
            );
        }
        return (jthrowable) env->PopLocalFrame(javaException);
    }

    void rethrowAsJavaException(JNIEnv *env, const std::exception_ptr &exPtr) {
        auto javaException = createJavaException(env, exPtr);
        env->Throw(javaException);
        env->DeleteLocalRef(javaException);
    }

#endif // CC_TARGET_PLATFORM
NS_CC_END