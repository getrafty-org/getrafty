#pragma once
#include <chrono>

namespace getrafty::wheels::misc {
    class CpuTimer {
    public:
        explicit CpuTimer(): start_ts_(std::clock()) {};

        [[nodiscard]] auto spent() const {
            const size_t clocks = std::clock() - start_ts_;
            return std::chrono::microseconds((clocks * 1'000'000) / CLOCKS_PER_SEC);
        }

    private:
        std::clock_t start_ts_{};
    };
} // getrafty::wheels::misc
