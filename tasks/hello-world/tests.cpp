#include <gtest/gtest.h>
#include <time_machine.h>

TEST(DeLoreanTimeMachine, readyToTravelInTime) {
  bttf::DeLoreanTimeMachine m;

  EXPECT_EQ(88, m.computeTimeTravelSpeed());
}
