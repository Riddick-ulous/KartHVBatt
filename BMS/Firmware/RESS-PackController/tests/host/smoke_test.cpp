#include <gtest/gtest.h>

#include "packcontroller/app/build_profile.hpp"

namespace packcontroller {
namespace {

TEST(BuildSmoke, LinksCAndCxxLayersWithBoardBringupProfile) {
  EXPECT_EQ(build_profile(), BuildProfile::kBoardBringup);
  EXPECT_EQ(expected_tle_slave_count(), 1U);
  EXPECT_EQ(architecture_version(), 12U);
}

}  // namespace
}  // namespace packcontroller
