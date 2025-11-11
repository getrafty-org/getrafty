#include <gtest/gtest.h>
#include <flux_capacitor.hpp>

using namespace getrafty::tutorial;

TEST(FluxCapacitorTest, JustWorks) {
  FluxCapacitor capacitor{};
  EXPECT_EQ(capacitor.computeTimeBarrierBreakSpeed(), 88);
}
