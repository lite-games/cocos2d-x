#include "catch_and_rethrow_as_platform_exception.h"

#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)

#include <string>
#include <vector>

#endif // CC_TARGET_PLATFORM

NS_CC_BEGIN
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)

    static jobjectArray createStackTrace(JNIEnv *env, const StackTrace &stackTrace) {
        env->PushLocalFrame(
                (jsize) (
                        // class
                        1
                        // final array
                        + 1
                        // java objects created for each stack trace element
                        + stackTrace.depth * 4
                )
        );

        auto StackTraceElementClass = env->FindClass("java/lang/StackTraceElement");
        auto StackTraceElementClass_Init = env->GetMethodID(
                StackTraceElementClass,
                "<init>",
                "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V"
        );

        auto stacktraceArray = env->NewObjectArray(
                (jsize) stackTrace.depth,
                StackTraceElementClass,
                nullptr
        );

        auto demangledSymbolBuf = (char *) malloc(sizeof(char) * 256);
        stacktrace_dump(
                stackTrace,
                [=, &demangledSymbolBuf]
                        (size_t idx,
                         const void *address,
                         const void *base_address,
                         const char *symbol,
                         const char *libpath) {
                    auto info = demangleSymbol(
                            address,
                            base_address,
                            symbol,
                            libpath,
                            demangledSymbolBuf
                    );

                    auto declaringClass = env->NewStringUTF(info.className.c_str());
                    auto methodName = env->NewStringUTF(info.methodName.c_str());
                    // NOTE: Reporting libpath as filename resulted in bad information on Crashlytics crash report. -- mz, 2023-11-30
                    // I've tried reporting a filename from libpath, but Crashlytics treated such stacktraces
                    //   as coming from some third party library.
                    // This resulted in imprecise crash reports.
                    //   Crashlytics was able to choose the following stacktrace element:
                    //     Native.<unavailable> (address: 0x78e055abc8)
                    //   as the source of the problem instead of higher placed and more detailed:
                    //     Native.getTextOnSideThread() (address: 0x78e0559d50) (libapp.so)
                    //   just because it had a library filename attached to it.
                    auto stackTraceElement = env->NewObject(
                            StackTraceElementClass,
                            StackTraceElementClass_Init,
                            declaringClass,
                            methodName,
                            nullptr, // unknown filename
                            -1 // -1 means: no line number
                    );

                    env->SetObjectArrayElement(
                            stacktraceArray,
                            (jsize) idx,
                            stackTraceElement
                    );
                }
        );
        free(demangledSymbolBuf);

        return (jobjectArray) env->PopLocalFrame((jobject) stacktraceArray);
    }

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

        auto exceptionMessage = env->NewStringUTF(msg);
        auto javaException = (jthrowable) env->NewObject(
                RuntimeExceptionClass,
                RuntimeExceptionClass_Init,
                exceptionMessage,
                cause
        );

        if (keepJavaStackTrace) {
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
            env->CallVoidMethod(
                    javaException,
                    RuntimeExceptionClass_setStackTraceMethod,
                    nativeStackTrace
            );
        }

        return (jthrowable) env->PopLocalFrame(javaException);
    }

    static jthrowable createJavaException(JNIEnv *env, const std::exception_ptr &exPtr) {
        struct ExceptionInfo {
            std::string msg;
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