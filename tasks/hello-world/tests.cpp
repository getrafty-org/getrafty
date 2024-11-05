#include <gtest/gtest.h>
#include <time_machine.hpp>

TEST(DeLoreanTimeMachine, readyToTravelInTime) {
  back_to_the_future::Delorean time_machine;

  EXPECT_EQ(88, time_machine.computeTimeTravelSpeed());
}
