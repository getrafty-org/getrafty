#include "logging.hpp"

namespace getrafty::wheels::logging {
    LogLevel &logLevel() {
        static LogLevel ll = TRACE;
        return ll;
    }
} // namespace getrafty::wheels::logging
