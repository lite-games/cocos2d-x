#include "CCRuntimeError.h"

NS_CC_BEGIN
    RuntimeError::RuntimeError(const std::string &what_arg, StackTrace stackTrace)
            : std::runtime_error(what_arg), stackTrace(stackTrace) {
    }

    RuntimeError::RuntimeError(const char *what_arg, StackTrace stackTrace)
            : std::runtime_error(what_arg), stackTrace(stackTrace) {
    }
NS_CC_END