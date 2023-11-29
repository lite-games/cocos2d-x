#ifndef __CCRUNTIME_ERROR_H__
#define __CCRUNTIME_ERROR_H__

#include "base/CCStackTrace.h"
#include <stdexcept>

NS_CC_BEGIN
    class RuntimeError : public std::runtime_error {
    public:
        explicit RuntimeError(
                const std::string &what_arg,
                StackTrace stackTrace = stacktrace_capture()
        );

        explicit RuntimeError(
                const char *what_arg,
                StackTrace stackTrace = stacktrace_capture()
        );

        const StackTrace &getStackTrace() const { return stackTrace; }

    private:
        StackTrace stackTrace;
    };

NS_CC_END

#endif //__CCRUNTIME_ERROR_H__
