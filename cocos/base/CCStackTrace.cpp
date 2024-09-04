#include "CCStackTrace.h"

#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)

#include <unwind.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <sstream>
#include <regex>

#endif // CC_TARGET_PLATFORM


NS_CC_BEGIN

#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)

    _Unwind_Reason_Code stacktrace_unwind_callback(struct _Unwind_Context *context, void *arg) {
        auto *state = static_cast<StackTraceUnwindState *>(arg);
        uintptr_t pc = _Unwind_GetIP(context);
        if (pc) {
            if (state->current == state->end) {
                return _URC_END_OF_STACK;
            } else {
                *state->current++ = reinterpret_cast<void *>(pc);
            }
        }
        return _URC_NO_REASON;
    }

    void stacktrace_dump(
            const StackTrace &stacktrace,
            std::function<void(
                    size_t idx,
                    const void *address,
                    const void *base_address,
                    const char *symbol,
                    const char *libpath)> fn
    ) {
        for (size_t idx = 0; idx < stacktrace.depth; ++idx) {
            const void *address = stacktrace.elements[idx];
            const void *base_address = nullptr;
            const char *symbol = "";
            const char *libpath = "";

            Dl_info info;
            if (dladdr(address, &info) && info.dli_sname) {
                base_address = info.dli_fbase;
                symbol = info.dli_sname;
                libpath = info.dli_fname;
            }

            fn(idx, address, base_address, symbol, libpath);
        }
    }

    DemangledSymbolInfo demangleSymbol(
            const void *address,
            const void *base_address,
            const char *symbol,
            const char *libpath,
            size_t &symbolBufLength,
            char *&symbolBuf
    ) {
        int demanglingStatus = 0;
        size_t demangledSymbolLength = symbolBufLength;
        char *demangledSymbolBuf = abi::__cxa_demangle(
                symbol,
                symbolBuf,
                &demangledSymbolLength,
                &demanglingStatus
        );

        std::string classNameStr;
        std::string methodNameStr;
        switch (demanglingStatus) {
            case 0: // The demangling operation succeeded
            {
                std::string demangledSymbolStr = demangledSymbolBuf;

                static const auto demangledSymbolRegex = std::regex("(.*)::(.*)");
                std::smatch demangledSymbolMatch;

                if (std::regex_match(demangledSymbolStr, demangledSymbolMatch,
                                     demangledSymbolRegex)) {
                    auto classNameMatch = demangledSymbolMatch[1];
                    auto methodNameMatch = demangledSymbolMatch[2];
                    classNameStr = classNameMatch.str();
                    methodNameStr = methodNameMatch.str();

                    // Sample resulting stack trace element:
                    //   at RMVSettings.setVisible(bool) (address: 0x77e320f0f4) (libRummy.so)
                } else {
                    classNameStr = "Native";
                    methodNameStr = demangledSymbolStr.empty() ? "<unavailable>"
                                                               : demangledSymbolStr;

                    // Sample resulting stack trace element:
                    //   at Native.getText() (address: 0x78da8c5818) (libcrashground.so)
                }
                break;
            }
            case -2: // mangled_name is not a valid name under the C++ ABI mangling rules.
            {
                classNameStr = "Native";
                methodNameStr = std::string(symbol).empty() ? "<unavailable>" : symbol;

                // Sample resulting stack trace element:
                //  at Native.Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesEnd (address: 0x77e32bb010)(libRummy.so)
                break;
            }
            case -1: // A memory allocation failure occurred
            case -3: // One of the arguments is invalid.
            default: // Should not happen, but just in case.
            {
                // Fallback to reporting mangled name
                std::ostringstream oss;
                oss << (std::string(symbol).empty() ? "<unavailable>" : symbol);
                oss << " (demanglingStatus: " << demanglingStatus << ")";

                classNameStr = "Native";
                methodNameStr = oss.str();
                break;
            }
        }

        auto libPathStr = libpath ? std::string(libpath) : "";

        const auto PathSeparator = '/';
        auto lastSeparatorPos = libPathStr.find_last_of(PathSeparator);
        auto libNameStr = (lastSeparatorPos != std::string::npos)
                          ? libPathStr.substr(lastSeparatorPos + 1)
                          : libPathStr;

        // Relative address matches the address which "llvm-addr2line", "nm" and "objdump"
        // utilities give you, if you compiled position-independent code (-fPIC, -pie).
        // Android requires position-independent code since Android 5.0.
        auto relative_address = (void *) ((unsigned char *) address -
                                          (unsigned char *) base_address);

        std::ostringstream oss;
        oss << methodNameStr << " (address: " << relative_address;
        if (!libNameStr.empty()) oss << ", lib: " << libNameStr;
        oss << ")";
        methodNameStr = oss.str();

        // Update symbolBuf if __cxa_demangle() call has reallocated it.
        if (demangledSymbolBuf != nullptr
            && demangledSymbolBuf != symbolBuf) {
            symbolBufLength = demangledSymbolLength;
            symbolBuf = demangledSymbolBuf;
        }

        return {
                .className = classNameStr,
                .methodName = methodNameStr,
        };
    }

    jobjectArray createStackTrace(JNIEnv *env, const StackTrace &stackTrace) {
        env->PushLocalFrame(
            (jsize) (
                // class
                1
                // final array
                + 1
                // java objects created for each stack trace element
                + stackTrace.depth * 3
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

        auto demangledSymbolBufLength = (size_t) (sizeof(char) * 256);
        auto demangledSymbolBuf = (char *) malloc(demangledSymbolBufLength);
        stacktrace_dump(
            stackTrace,
            [=, &demangledSymbolBufLength, &demangledSymbolBuf]
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
                    demangledSymbolBufLength,
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

#endif

NS_CC_END