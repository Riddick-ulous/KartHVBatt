#include "packcontroller/app/build_profile.hpp"

#include "packcontroller/platform/build_contract.h"

namespace packcontroller {

BuildProfile build_profile() noexcept {
#if defined(PACKCONTROLLER_PROFILE_BOARD_BRINGUP)
  return BuildProfile::kBoardBringup;
#elif defined(PACKCONTROLLER_PROFILE_PACK_162S2P)
  return BuildProfile::kPack162s2p;
#else
#error "A PackController build profile must be selected"
#endif
}

std::uint8_t expected_tle_slave_count() noexcept {
#if defined(PACKCONTROLLER_PROFILE_BOARD_BRINGUP)
  return 1U;
#elif defined(PACKCONTROLLER_PROFILE_PACK_162S2P)
  return 18U;
#else
#error "A PackController build profile must be selected"
#endif
}

std::uint32_t architecture_version() noexcept {
  return packcontroller_architecture_version();
}

}  // namespace packcontroller
