#pragma once

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>

enum LogLevel : int8_t {
    /*NOLINT*/ ERROR = 0,
    /*NOLINT*/ WARNING = 1,
    /*NOLINT*/ INFO = 2,
    /*NOLINT*/ DEBUG = 3,
    /*NOLINT*/ TRACE = 4,
};

namespace getrafty::wheels::logging {
    LogLevel &logLevel();

    class Logger {
    public:
        explicit Logger(const LogLevel level) : level_(level) {
        }

        ~Logger() {
            std::cout << getCurrentTime() << " [" << getLabel(level_) << "] " << stream_.str() << std::endl;
        }

        template<typename T>
        Logger &operator<<(const T &message) {
            stream_ << message;
            return *this;
        }

    private:
        LogLevel level_;
        std::ostringstream stream_;

        static std::string getLabel(LogLevel level) {
            switch (level) {
                case TRACE: {
                    return "TRACE";
                }
                case DEBUG: {
                    return "DEBUG";
                }
                case INFO: {
                    return "INFO";
                }
                case WARNING: { return "WARN"; }
                case ERROR: { return "ERROR"; }
                default: { return "UNKNOWN"; }
            }
        }

        static std::string getCurrentTime() {
            const auto& now = std::chrono::system_clock::now();
            const auto& time = std::chrono::system_clock::to_time_t(now);
            const auto& tm = *std::localtime(&time);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            return oss.str();
        }
    };

#define LOG(level) (getrafty::wheels::logging::Logger{level})
} // namespace getrafty::wheels::logging
