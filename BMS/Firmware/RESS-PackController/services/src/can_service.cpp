#include "packcontroller/services/can_service.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace packcontroller::services {
namespace {

std::uint16_t saturating_add(std::uint16_t value,
                             std::uint16_t increment) noexcept {
  const auto maximum = std::numeric_limits<std::uint16_t>::max();
  if (increment > static_cast<std::uint16_t>(maximum - value)) {
    return maximum;
  }
  return static_cast<std::uint16_t>(value + increment);
}

bool valid_application_frame(const packcontroller_can_frame_t& frame,
                             std::uint8_t expected_length) noexcept {
  return frame.is_fd && frame.bit_rate_switch && !frame.is_extended &&
         (frame.length == expected_length);
}

void initialize_tx_frame(packcontroller_can_frame_t& frame,
                         packcontroller_can_bus_t bus,
                         std::uint32_t identifier,
                         std::uint8_t length) noexcept {
  frame = {};
  frame.identifier = identifier;
  frame.length = length;
  frame.bus = static_cast<std::uint8_t>(bus);
  frame.is_fd = true;
  frame.bit_rate_switch = true;
  frame.is_extended = false;
}

std::uint8_t next_alive(std::array<std::uint8_t, 2U>& counters,
                        std::size_t index) noexcept {
  const std::uint8_t current = counters[index];
  counters[index] = static_cast<std::uint8_t>((current + 1U) & 0x0FU);
  return current;
}

}  // namespace

CanBusState can_bus_state(
    const packcontroller_can_bus_diagnostics_t& diagnostics) noexcept {
  if (!diagnostics.started) {
    return CanBusState::kNotStarted;
  }
  if (diagnostics.bus_off) {
    return CanBusState::kBusOff;
  }
  if (diagnostics.error_passive) {
    return CanBusState::kErrorPassive;
  }
  return CanBusState::kErrorActive;
}

AliveResult AliveMonitor::observe(std::uint8_t alive,
                                  std::uint32_t now_ms) noexcept {
  alive = static_cast<std::uint8_t>(alive & 0x0FU);
  if (!initialized_) {
    initialized_ = true;
    last_alive_ = alive;
    last_progress_ms_ = now_ms;
    progress_count_ = 1U;
    return AliveResult::kFirst;
  }

  const auto delta = static_cast<std::uint8_t>((alive - last_alive_) & 0x0FU);
  if (delta == 0U) {
    discontinuities_ = saturating_add(discontinuities_, 1U);
    dropped_frames_ = saturating_add(dropped_frames_, 1U);
    return AliveResult::kDuplicate;
  }

  last_alive_ = alive;
  last_progress_ms_ = now_ms;
  ++progress_count_;
  if (delta == 1U) {
    return AliveResult::kAccepted;
  }

  discontinuities_ = saturating_add(discontinuities_, 1U);
  dropped_frames_ = saturating_add(
      dropped_frames_, static_cast<std::uint16_t>(delta - 1U));
  return AliveResult::kGap;
}

bool AliveMonitor::fresh(std::uint32_t now_ms) const noexcept {
  return initialized_ && ((now_ms - last_progress_ms_) <= kCanStaleTimeoutMs);
}

ReceiveResult CanService::receive(const packcontroller_can_frame_t& frame,
                                  std::uint32_t now_ms) noexcept {
  ReceiveResult result{};
  if ((frame.bus != static_cast<std::uint8_t>(PACKCONTROLLER_CAN_BUS_1)) &&
      (frame.bus != static_cast<std::uint8_t>(PACKCONTROLLER_CAN_BUS_2))) {
    return result;
  }
  const auto bus = static_cast<packcontroller_can_bus_t>(frame.bus);
  const std::size_t index = bus_index(bus);

  if (frame.identifier == PACK_CONTROLLER_VCU_BMS_CONTROL_FRAME_ID) {
    result.recognized = true;
    if (!valid_application_frame(
            frame, PACK_CONTROLLER_VCU_BMS_CONTROL_LENGTH)) {
      return result;
    }
    pack_controller_vcu_bms_control_t decoded{};
    if (pack_controller_vcu_bms_control_unpack(
            &decoded, frame.data, frame.length) != 0) {
      return result;
    }
    const AliveResult alive =
        control_alive_[index].observe(decoded.control_alive_counter, now_ms);
    result.discontinuity =
        (alive == AliveResult::kGap) || (alive == AliveResult::kDuplicate);
    if (alive == AliveResult::kDuplicate) {
      return result;
    }
    controls_[index].message = decoded;
    controls_[index].received_ms = now_ms;
    controls_[index].valid = true;
    result.accepted = true;
    return result;
  }

  if (frame.identifier == PACK_CONTROLLER_BMS_SERVICE_REQUEST_FRAME_ID) {
    result.recognized = true;
    if (!valid_application_frame(
            frame, PACK_CONTROLLER_BMS_SERVICE_REQUEST_LENGTH)) {
      return result;
    }
    pack_controller_bms_service_request_t decoded{};
    if (pack_controller_bms_service_request_unpack(
            &decoded, frame.data, frame.length) != 0) {
      return result;
    }
    const AliveResult alive = service_alive_[index].observe(
        decoded.service_request_alive_counter, now_ms);
    result.discontinuity =
        (alive == AliveResult::kGap) || (alive == AliveResult::kDuplicate);
    if (alive == AliveResult::kDuplicate) {
      return result;
    }
    result.accepted = true;
    result.service_available = true;
    result.service.message = decoded;
    result.service.bus = bus;
    result.service.received_ms = now_ms;
    result.service.valid = true;
  }
  return result;
}

void CanService::set_source(CanSource source) noexcept {
  if (source_ != source) {
    source_ = source;
    ++source_switch_count_;
  }
}

void CanService::update_source(std::uint32_t now_ms) noexcept {
  const bool main_fresh = control_alive_[0U].fresh(now_ms);
  const bool backup_fresh = control_alive_[1U].fresh(now_ms);

  if (source_ == CanSource::kNone) {
    if (main_fresh) {
      set_source(CanSource::kMain);
    } else if (backup_fresh) {
      set_source(CanSource::kBackup);
    }
    main_recovery_tracking_ = false;
    return;
  }

  if (source_ == CanSource::kMain) {
    if (!main_fresh) {
      set_source(backup_fresh ? CanSource::kBackup : CanSource::kNone);
    }
    main_recovery_tracking_ = false;
    return;
  }

  if (!main_fresh) {
    main_recovery_tracking_ = false;
    if (!backup_fresh) {
      set_source(CanSource::kNone);
    }
    return;
  }

  if (!main_recovery_tracking_) {
    main_recovery_tracking_ = true;
    main_recovery_since_ms_ = now_ms;
    main_recovery_start_progress_ = control_alive_[0U].progress_count();
  }
  if (((now_ms - main_recovery_since_ms_) >= kCanStaleTimeoutMs) &&
      (control_alive_[0U].progress_count() !=
       main_recovery_start_progress_)) {
    set_source(CanSource::kMain);
    main_recovery_tracking_ = false;
  }
}

const ControlSnapshot* CanService::authoritative_control(
    std::uint32_t now_ms) const noexcept {
  if (source_ == CanSource::kMain) {
    return control_alive_[0U].fresh(now_ms) ? &controls_[0U] : nullptr;
  }
  if (source_ == CanSource::kBackup) {
    return control_alive_[1U].fresh(now_ms) ? &controls_[1U] : nullptr;
  }
  return nullptr;
}

const AliveMonitor& CanService::control_alive(
    packcontroller_can_bus_t bus) const noexcept {
  return control_alive_[bus_index(bus)];
}

const AliveMonitor& CanService::service_alive(
    packcontroller_can_bus_t bus) const noexcept {
  return service_alive_[bus_index(bus)];
}

bool CanService::make_status_frame(packcontroller_can_bus_t bus,
                                   const StatusData& status,
                                   packcontroller_can_frame_t& frame) noexcept {
  pack_controller_bms_status_t message{};
  const std::size_t index = bus_index(bus);
  message.status_alive_counter = next_alive(status_alive_, index);
  message.status_crc_enabled = 0U;
  message.authoritative_can_bus = static_cast<std::uint8_t>(source_);
  message.critical_error_active = status.critical_error_active ? 1U : 0U;
  message.hv_state = status.hv_state;
  message.dcdc_state = status.dcdc_state;
  message.developer_mode = status.developer_mode;
  message.air_n_switch = status.air_n ? 1U : 0U;
  message.air_n_actual = status.air_n_actual ? 1U : 0U;
  message.precharge_switch = status.precharge ? 1U : 0U;
  message.precharge_actual = status.precharge_actual ? 1U : 0U;
  message.air_p_switch = status.air_p ? 1U : 0U;
  message.air_p_actual = status.air_p_actual ? 1U : 0U;
  message.dcdc_switch = status.dcdc ? 1U : 0U;
  message.dcdc_actual = status.dcdc_actual ? 1U : 0U;
  message.danger_voltage = status.danger_voltage ? 1U : 0U;
  message.por_state_n = status.por_state_n ? 1U : 0U;
  message.sc_latched = status.sc_latched ? 1U : 0U;
  message.primary_fault_id = status.primary_fault_id;
  message.fault_severity = status.fault_severity;
  message.runtime_seconds = status.runtime_seconds;
  message.status_crc8 = 0U;

  initialize_tx_frame(frame, bus, PACK_CONTROLLER_BMS_STATUS_FRAME_ID,
                      PACK_CONTROLLER_BMS_STATUS_LENGTH);
  return pack_controller_bms_status_pack(frame.data, &message,
                                         frame.length) == frame.length;
}

bool CanService::make_safety_frame(packcontroller_can_bus_t bus,
                                   const SafetyData& safety,
                                   packcontroller_can_frame_t& frame) noexcept {
  pack_controller_bms_safety_diag_t message{};
  const std::size_t index = bus_index(bus);
  message.safety_alive_counter = next_alive(safety_alive_, index);
  message.can1_state = safety.can1_state;
  message.can2_state = safety.can2_state;
  message.adc_quality = safety.adc_quality;
  message.scheduler_healthy = safety.scheduler_healthy ? 1U : 0U;
  message.watchdog_feed_enabled = safety.watchdog_feed_enabled ? 1U : 0U;
  message.digital_raw_bitmap = safety.digital_raw_bitmap;
  message.digital_confirmed_bitmap = safety.digital_confirmed_bitmap;
  message.fault_active_bitmap_lo = safety.fault_active_low;
  message.fault_active_bitmap_hi = safety.fault_active_high;
  message.fault_latched_bitmap_lo = safety.fault_latched_low;
  message.fault_latched_bitmap_hi = safety.fault_latched_high;
  message.scheduler_loop_last = safety.scheduler_loop_last_us;
  message.scheduler_loop_max = safety.scheduler_loop_max_us;
  message.can1_dropped_frames = safety.can1_dropped_frames;
  message.can2_dropped_frames = safety.can2_dropped_frames;
  message.can1_bus_off_count = safety.can1_bus_off_count;
  message.can2_bus_off_count = safety.can2_bus_off_count;
  message.adc_dma_error_count = safety.adc_dma_error_count;
  message.eeprom_error_count = safety.eeprom_error_count;
  message.imd_hardware_type = safety.imd_hardware_type;
  message.safety_crc8 = 0U;

  initialize_tx_frame(frame, bus, PACK_CONTROLLER_BMS_SAFETY_DIAG_FRAME_ID,
                      PACK_CONTROLLER_BMS_SAFETY_DIAG_LENGTH);
  return pack_controller_bms_safety_diag_pack(frame.data, &message,
                                              frame.length) == frame.length;
}

bool CanService::make_analog_frame(packcontroller_can_bus_t bus,
                                   const AnalogData& analog,
                                   packcontroller_can_frame_t& frame) noexcept {
  pack_controller_bms_analog_t message{};
  const std::size_t index = bus_index(bus);
  message.analog_alive_counter = next_alive(analog_alive_, index);
  message.analog_protocol_version =
      PACK_CONTROLLER_BMS_ANALOG_ANALOG_PROTOCOL_VERSION_ANALOG_PROTOCOL_V0_CHOICE;
  message.analog_frame_coherent = analog.coherent ? 1U : 0U;
  message.analog_quality = analog.overall_quality;
  message.analog_sample_counter =
      static_cast<std::uint16_t>(analog.sample_counter);
  message.analog_sample_timestamp = analog.timestamp_ms;

  message.raw_r_leak1 = analog.raw[0U];
  message.raw_v_vehi = analog.raw[1U];
  message.raw_tntc4 = analog.raw[2U];
  message.raw_vref_int = analog.raw[3U];
  message.raw_r_leak2 = analog.raw[4U];
  message.raw_tntc1 = analog.raw[5U];
  message.raw_tntc5 = analog.raw[6U];
  message.raw_v_batt = analog.raw[7U];
  message.raw_v_accu = analog.raw[8U];
  message.raw_v_dcdc = analog.raw[9U];
  message.raw_tntc3 = analog.raw[10U];
  message.raw_tntc2 = analog.raw[11U];

  message.r_leak1_sense_voltage =
      pack_controller_bms_analog_r_leak1_sense_voltage_encode(
          analog.physical[0U]);
  message.v_vehi_analog =
      pack_controller_bms_analog_v_vehi_analog_encode(analog.physical[1U]);
  message.tntc4_temperature =
      pack_controller_bms_analog_tntc4_temperature_encode(
          analog.physical[2U]);
  message.vdda =
      pack_controller_bms_analog_vdda_encode(analog.physical[3U]);
  message.r_leak2_sense_voltage =
      pack_controller_bms_analog_r_leak2_sense_voltage_encode(
          analog.physical[4U]);
  message.tntc1_temperature =
      pack_controller_bms_analog_tntc1_temperature_encode(
          analog.physical[5U]);
  message.tntc5_temperature =
      pack_controller_bms_analog_tntc5_temperature_encode(
          analog.physical[6U]);
  message.v_batt_analog =
      pack_controller_bms_analog_v_batt_analog_encode(analog.physical[7U]);
  message.v_accu_analog =
      pack_controller_bms_analog_v_accu_analog_encode(analog.physical[8U]);
  message.v_dcdc_analog =
      pack_controller_bms_analog_v_dcdc_analog_encode(analog.physical[9U]);
  message.tntc3_temperature =
      pack_controller_bms_analog_tntc3_temperature_encode(
          analog.physical[10U]);
  message.tntc2_temperature =
      pack_controller_bms_analog_tntc2_temperature_encode(
          analog.physical[11U]);

  message.r_leak1_quality = analog.quality[0U];
  message.v_vehi_quality = analog.quality[1U];
  message.tntc4_quality = analog.quality[2U];
  message.vref_int_quality = analog.quality[3U];
  message.r_leak2_quality = analog.quality[4U];
  message.tntc1_quality = analog.quality[5U];
  message.tntc5_quality = analog.quality[6U];
  message.v_batt_quality = analog.quality[7U];
  message.v_accu_quality = analog.quality[8U];
  message.v_dcdc_quality = analog.quality[9U];
  message.tntc3_quality = analog.quality[10U];
  message.tntc2_quality = analog.quality[11U];
  message.analog_dma_error_count = analog.dma_error_count;
  message.analog_dropped_block_count = analog.dropped_block_count;
  message.analog_crc8 = 0U;

  initialize_tx_frame(frame, bus, PACK_CONTROLLER_BMS_ANALOG_FRAME_ID,
                      PACK_CONTROLLER_BMS_ANALOG_LENGTH);
  return pack_controller_bms_analog_pack(frame.data, &message,
                                         frame.length) == frame.length;
}

bool CanService::make_service_response_frame(
    packcontroller_can_bus_t bus,
    const pack_controller_bms_service_response_t& response,
    packcontroller_can_frame_t& frame) noexcept {
  pack_controller_bms_service_response_t message = response;
  const std::size_t index = bus_index(bus);
  message.service_response_alive_counter =
      next_alive(service_response_alive_, index);
  message.service_response_crc8 = 0U;
  initialize_tx_frame(frame, bus,
                      PACK_CONTROLLER_BMS_SERVICE_RESPONSE_FRAME_ID,
                      PACK_CONTROLLER_BMS_SERVICE_RESPONSE_LENGTH);
  return pack_controller_bms_service_response_pack(frame.data, &message,
                                                   frame.length) == frame.length;
}

}  // namespace packcontroller::services
