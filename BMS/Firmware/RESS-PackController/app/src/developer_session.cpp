#include "packcontroller/app/developer_session.hpp"

#include "pack_controller.h"

namespace packcontroller::app {
namespace {

constexpr std::uint32_t kConfirmationTimeMs = 100U;

std::uint32_t bool_bit(bool value, std::uint32_t bit) noexcept {
  return value ? (std::uint32_t{1U} << bit) : 0U;
}

}  // namespace

void SafetyInputConfirmation::update_level(Level& level, bool raw,
                                           std::uint32_t now_ms,
                                           bool unsafe_initial) noexcept {
  if (!level.initialized) {
    level.initialized = true;
    level.raw = raw;
    level.confirmed = unsafe_initial;
    level.stable_since_ms = now_ms;
    return;
  }
  if (raw != level.raw) {
    level.raw = raw;
    level.stable_since_ms = now_ms;
    return;
  }
  if ((now_ms - level.stable_since_ms) >= kConfirmationTimeMs) {
    level.confirmed = level.raw;
  }
}

void SafetyInputConfirmation::update(packcontroller_safety_inputs_t raw,
                                     std::uint32_t now_ms) noexcept {
  update_level(danger_clear_, raw.danger_voltage_clear_n, now_ms, false);
  update_level(por_valid_, raw.por_state_n, now_ms, false);
  update_level(sc_latched_, raw.sc_latched, now_ms, true);
  update_level(precharge_actual_, raw.precharge_actual, now_ms, true);
  update_level(air_p_actual_, raw.air_p_actual, now_ms, true);
  update_level(air_n_actual_, raw.air_n_actual, now_ms, true);
  update_level(dcdc_actual_, raw.dcdc_actual, now_ms, true);
}

DeveloperGates SafetyInputConfirmation::gates(bool scheduler_healthy,
                                              bool clock_healthy) const
    noexcept {
  return DeveloperGates{danger_clear_.confirmed, por_valid_.confirmed,
                        !sc_latched_.confirmed, scheduler_healthy,
                        clock_healthy};
}

std::uint32_t SafetyInputConfirmation::raw_bitmap() const noexcept {
  return bool_bit(precharge_actual_.raw, 1U) |
         bool_bit(air_p_actual_.raw, 3U) |
         bool_bit(air_n_actual_.raw, 5U) |
         bool_bit(dcdc_actual_.raw, 7U) |
         bool_bit(danger_clear_.raw, 8U) | bool_bit(por_valid_.raw, 9U) |
         bool_bit(sc_latched_.raw, 10U);
}

std::uint32_t SafetyInputConfirmation::confirmed_bitmap() const noexcept {
  return bool_bit(precharge_actual_.confirmed, 1U) |
         bool_bit(air_p_actual_.confirmed, 3U) |
         bool_bit(air_n_actual_.confirmed, 5U) |
         bool_bit(dcdc_actual_.confirmed, 7U) |
         bool_bit(danger_clear_.confirmed, 8U) |
         bool_bit(por_valid_.confirmed, 9U) |
         bool_bit(sc_latched_.confirmed, 10U);
}

packcontroller_switch_outputs_t SafetyInputConfirmation::confirmed_actuals()
    const noexcept {
  return packcontroller_switch_outputs_t{
      air_n_actual_.confirmed, precharge_actual_.confirmed,
      air_p_actual_.confirmed, dcdc_actual_.confirmed};
}

bool SafetyInputConfirmation::all_power_paths_open() const noexcept {
  return precharge_actual_.initialized && air_p_actual_.initialized &&
         air_n_actual_.initialized && dcdc_actual_.initialized &&
         !precharge_actual_.confirmed && !air_p_actual_.confirmed &&
         !air_n_actual_.confirmed && !dcdc_actual_.confirmed;
}

DeveloperResult DeveloperSession::handle(
    const services::ServiceRequest& request,
    const DeveloperContext& context) noexcept {
  DeveloperResult result{};
  if (!request.valid) {
    return result;
  }
  const auto command = request.message.service_command;
  const bool is_developer_command =
      (command ==
       PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_DEV_MODE_ENTER_CHOICE) ||
      (command ==
       PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_DEV_OUTPUT_SET_CHOICE) ||
      (command ==
       PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_DEV_MODE_EXIT_CHOICE);
  if (!is_developer_command) {
    return result;
  }

  result.handled = true;
  if (!profile_allows_developer_) {
    result.result = ServiceResult::kDeniedState;
    return result;
  }

  if (command ==
      PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_DEV_MODE_ENTER_CHOICE) {
    const std::uint32_t requested_raw = request.message.service_value0;
    if ((requested_raw < static_cast<std::uint32_t>(DeveloperMode::kOutputTest)) ||
        (requested_raw >
         static_cast<std::uint32_t>(DeveloperMode::kCommissioning))) {
      result.result = ServiceResult::kInvalidValue;
      return result;
    }
    if (!build_key_configured_ ||
        (request.message.service_value1 != build_key_)) {
      result.result = ServiceResult::kInvalidValue;
      return result;
    }
    const auto requested = static_cast<DeveloperMode>(requested_raw);
    const bool renewal = (mode_ == requested) && keepalive_seen_;
    if ((mode_ != DeveloperMode::kDisabled) && !renewal) {
      result.result = ServiceResult::kDeniedState;
      return result;
    }
    if (!renewal &&
        (!context.entry_state_allowed || !context.all_switch_requests_low)) {
      result.result = ServiceResult::kDeniedState;
      return result;
    }

    result.mode_changed = mode_ != requested;
    mode_ = requested;
    if (!renewal) {
      output_mask_ = 0U;
    }
    last_keepalive_ms_ = request.received_ms;
    keepalive_seen_ = true;
    commissioning_session_lost_ = false;
    result.result = ServiceResult::kOk;
    return result;
  }

  if (command ==
      PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_DEV_OUTPUT_SET_CHOICE) {
    if ((mode_ != DeveloperMode::kOutputTest) ||
        !session_fresh(request.received_ms)) {
      result.result = ServiceResult::kDeniedState;
      return result;
    }
    if ((request.message.service_value0 & ~static_cast<std::uint32_t>(
                                             kOutputMaskAllowed)) != 0U) {
      result.result = ServiceResult::kInvalidValue;
      return result;
    }
    output_mask_ = static_cast<std::uint8_t>(request.message.service_value0);
    result.result = ServiceResult::kOk;
    return result;
  }

  result.mode_changed = mode_ != DeveloperMode::kDisabled;
  if ((mode_ == DeveloperMode::kCommissioning) &&
      !context.all_switch_requests_low) {
    commissioning_session_lost_ = true;
  }
  mode_ = DeveloperMode::kDisabled;
  output_mask_ = 0U;
  keepalive_seen_ = false;
  result.result = ServiceResult::kOk;
  return result;
}

bool DeveloperSession::session_fresh(std::uint32_t now_ms) const noexcept {
  return keepalive_seen_ &&
         ((now_ms - last_keepalive_ms_) <= kKeepaliveTimeoutMs);
}

void DeveloperSession::update(std::uint32_t now_ms,
                              const DeveloperGates& gates) noexcept {
  if (mode_ == DeveloperMode::kDisabled) {
    output_mask_ = 0U;
    return;
  }
  if (!session_fresh(now_ms)) {
    if (mode_ == DeveloperMode::kCommissioning) {
      commissioning_session_lost_ = true;
    }
    mode_ = DeveloperMode::kDisabled;
    output_mask_ = 0U;
    keepalive_seen_ = false;
    return;
  }
  if ((mode_ == DeveloperMode::kOutputTest) && !gates.all_allow_output()) {
    output_mask_ = 0U;
  }
}

std::uint8_t DeveloperSession::output_mask(
    std::uint32_t now_ms, const DeveloperGates& gates) const noexcept {
  if ((mode_ != DeveloperMode::kOutputTest) || !session_fresh(now_ms) ||
      !gates.all_allow_output()) {
    return 0U;
  }
  return output_mask_;
}

packcontroller_switch_outputs_t OutputArbiter::resolve(
    DeveloperMode mode, std::uint8_t developer_mask,
    const DeveloperGates& gates, bool immediate_fault) const noexcept {
  packcontroller_switch_outputs_t outputs{};
  if (immediate_fault || (mode != DeveloperMode::kOutputTest) ||
      !gates.all_allow_output()) {
    return outputs;
  }
  outputs.air_n = (developer_mask & 0x01U) != 0U;
  outputs.precharge = (developer_mask & 0x02U) != 0U;
  outputs.air_p = (developer_mask & 0x04U) != 0U;
  outputs.dcdc = (developer_mask & 0x08U) != 0U;
  return outputs;
}

}  // namespace packcontroller::app
