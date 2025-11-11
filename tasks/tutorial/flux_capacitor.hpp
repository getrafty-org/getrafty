#pragma once

#include <cstdint>

namespace getrafty::tutorial {

using Speed = uint32_t;

// https://en.wikipedia.org/wiki/DeLorean_time_machine#Equipment
class FluxCapacitor {
public:
    Speed computeTimeBarrierBreakSpeed();
};

} // namespace getrafty::tutorial
