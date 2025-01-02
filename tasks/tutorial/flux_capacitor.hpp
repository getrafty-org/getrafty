#pragma once

#include <cstdint>

namespace getrafty::tutorial {

using Speed = uint32_t;

// https://backtothefuture.fandom.com/wiki/Flux_capacitor
class FluxCapacitor {
public:
    Speed computeTimeBreakBarrierSpeed();
};

} // namespace getrafty::tutorial
