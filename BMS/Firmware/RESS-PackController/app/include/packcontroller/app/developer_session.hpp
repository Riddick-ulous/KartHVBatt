#ifndef PACKCONTROLLER_APP_DEVELOPER_SESSION_HPP
#define PACKCONTROLLER_APP_DEVELOPER_SESSION_HPP

#include <cstdint>

#include "packcontroller/platform/io.h"
#include "packcontroller/services/can_service.hpp"

namespace packcontroller::app {

// ASCII "BRT" interpreted as the numeric ServiceValue1 value 0x00425254.
inline constexpr std::uint32_t kDeveloperBuildKey = 0x00425254U;

enum class DeveloperMode : std::uint8_t {
  kDisabled = 0U,
  kOutputTest = 1U,
  kCommissioning = 2U,
};

enum class ServiceResult : std::uint8_t {
  kOk = 0U,
  kBusy = 1U,
  kDeniedState = 2U,
  kInvalidTarget = 3U,
  kInvalidValue = 4U,
  kCrcError = 5U,
  kNvmError = 6U,
  kSequenceError = 7U,
};

struct DeveloperGates final {
  bool danger_voltage_clear{false};
  bool por_valid{false};
  bool sc_not_latched{false};
  bool scheduler_healthy{false};
  bool clock_healthy{false};

  [[nodiscard]] bool all_allow_output() const noexcept {
    return danger_voltage_clear && por_valid && sc_not_latched &&
           scheduler_healthy && clock_healthy;
  }
};

struct DeveloperContext final {
  bool entry_state_allowed{false};
  bool all_switch_requests_low{true};
};

struct DeveloperResult final {
  ServiceResult result{ServiceResult::kInvalidTarget};
  bool handled{false};
  bool mode_changed{false};
};

class SafetyInputConfirmation final {
 public:
  void update(packcontroller_safety_inputs_t raw,
              std::uint32_t now_ms) noexcept;
  [[nodiscard]] DeveloperGates gates(bool scheduler_healthy,
                                     bool clock_healthy) const noexcept;
  [[nodiscard]] std::uint32_t raw_bitmap() const noexcept;
  [[nodiscard]] std::uint32_t confirmed_bitmap() const noexcept;
  [[nodiscard]] packcontroller_switch_outputs_t confirmed_actuals()
      const noexcept;
  [[nodiscard]] bool all_power_paths_open() const noexcept;

 private:
  struct Level final {
    bool raw{false};
    bool confirmed{false};
    bool initialized{false};
    std::uint32_t stable_since_ms{0U};
  };

  static void update_level(Level& level, bool raw, std::uint32_t now_ms,
                           bool unsafe_initial) noexcept;

  Level danger_clear_{};
  Level por_valid_{};
  Level sc_latched_{};
  Level precharge_actual_{};
  Level air_p_actual_{};
  Level air_n_actual_{};
  Level dcdc_actual_{};
};

class DeveloperSession final {
 public:
  DeveloperSession(bool profile_allows_developer, std::uint32_t build_key,
                   bool build_key_configured = true) noexcept
      : profile_allows_developer_(profile_allows_developer),
        build_key_(build_key),
        build_key_configured_(build_key_configured) {}

  DeveloperResult handle(
      const services::ServiceRequest& request,
      const DeveloperContext& context) noexcept;
  void update(std::uint32_t now_ms, const DeveloperGates& gates) noexcept;

  [[nodiscard]] DeveloperMode mode() const noexcept { return mode_; }
  [[nodiscard]] bool session_fresh(std::uint32_t now_ms) const noexcept;
  [[nodiscard]] std::uint8_t output_mask(std::uint32_t now_ms,
                                         const DeveloperGates& gates) const
      noexcept;
  [[nodiscard]] bool commissioning_session_lost() const noexcept {
    return commissioning_session_lost_;
  }

 private:
  static constexpr std::uint32_t kKeepaliveTimeoutMs = 500U;
  static constexpr std::uint8_t kOutputMaskAllowed = 0x0FU;

  bool profile_allows_developer_{false};
  std::uint32_t build_key_{0U};
  bool build_key_configured_{false};
  DeveloperMode mode_{DeveloperMode::kDisabled};
  std::uint8_t output_mask_{0U};
  std::uint32_t last_keepalive_ms_{0U};
  bool keepalive_seen_{false};
  bool commissioning_session_lost_{false};
};

class OutputArbiter final {
 public:
  [[nodiscard]] packcontroller_switch_outputs_t resolve(
      DeveloperMode mode, std::uint8_t developer_mask,
      const DeveloperGates& gates, bool immediate_fault) const noexcept;
};

}  // namespace packcontroller::app

#endif
