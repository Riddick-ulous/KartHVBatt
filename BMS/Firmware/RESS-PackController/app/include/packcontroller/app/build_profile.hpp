#ifndef PACKCONTROLLER_APP_BUILD_PROFILE_HPP
#define PACKCONTROLLER_APP_BUILD_PROFILE_HPP

#include <cstdint>

namespace packcontroller {

enum class BuildProfile : std::uint8_t {
  kBoardBringup = 0U,
  kPack162s2p = 1U,
};

BuildProfile build_profile() noexcept;
std::uint8_t expected_tle_slave_count() noexcept;
std::uint32_t architecture_version() noexcept;

}  // namespace packcontroller

#endif
