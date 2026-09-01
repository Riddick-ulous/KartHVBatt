#include "packcontroller/app/nvm.hpp"

#include <algorithm>
#include <limits>

#include "pack_controller.h"

namespace packcontroller::app {
namespace {

constexpr std::uint8_t kServiceOk =
    PACK_CONTROLLER_BMS_SERVICE_RESPONSE_SERVICE_RESULT_SERVICE_OK_CHOICE;
constexpr std::uint8_t kServiceBusy =
    PACK_CONTROLLER_BMS_SERVICE_RESPONSE_SERVICE_RESULT_SERVICE_BUSY_CHOICE;
constexpr std::uint8_t kServiceDenied =
    PACK_CONTROLLER_BMS_SERVICE_RESPONSE_SERVICE_RESULT_SERVICE_DENIED_STATE_CHOICE;
constexpr std::uint8_t kServiceInvalidTarget =
    PACK_CONTROLLER_BMS_SERVICE_RESPONSE_SERVICE_RESULT_SERVICE_INVALID_TARGET_CHOICE;
constexpr std::uint8_t kServiceInvalidValue =
    PACK_CONTROLLER_BMS_SERVICE_RESPONSE_SERVICE_RESULT_SERVICE_INVALID_VALUE_CHOICE;
constexpr std::uint8_t kServiceNvmError =
    PACK_CONTROLLER_BMS_SERVICE_RESPONSE_SERVICE_RESULT_SERVICE_NVM_ERROR_CHOICE;

constexpr std::uint16_t kTargetSystemConfig =
    PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_TARGET_SYSTEM_CONFIG_CHOICE;
constexpr std::uint16_t kTargetCellProfile =
    PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_TARGET_CELL_PROFILE_CONFIG_CHOICE;
constexpr std::uint16_t kTargetHvConfig =
    PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_TARGET_HV_CONFIG_CHOICE;
constexpr std::uint16_t kTargetChargeConfig =
    PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_TARGET_CHARGE_CONFIG_CHOICE;
constexpr std::uint16_t kTargetImdConfig =
    PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_TARGET_IMD_CONFIG_CHOICE;
constexpr std::uint16_t kTargetLeakageConfig =
    PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_TARGET_LEAKAGE_CONFIG_CHOICE;
constexpr std::uint16_t kTargetAdcCalibration =
    PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_TARGET_ADC_CALIBRATION_CHOICE;

std::uint16_t read_u16(const std::uint8_t* data) noexcept {
  return static_cast<std::uint16_t>(
      static_cast<std::uint16_t>(data[0U]) |
      static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1U]) << 8U));
}

std::uint32_t read_u32(const std::uint8_t* data) noexcept {
  return static_cast<std::uint32_t>(data[0U]) |
         (static_cast<std::uint32_t>(data[1U]) << 8U) |
         (static_cast<std::uint32_t>(data[2U]) << 16U) |
         (static_cast<std::uint32_t>(data[3U]) << 24U);
}

std::int32_t read_i32(const std::uint8_t* data) noexcept {
  return static_cast<std::int32_t>(read_u32(data));
}

void write_u16(std::uint8_t* data, std::uint16_t value) noexcept {
  data[0U] = static_cast<std::uint8_t>(value & 0xFFU);
  data[1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void write_u32(std::uint8_t* data, std::uint32_t value) noexcept {
  data[0U] = static_cast<std::uint8_t>(value & 0xFFU);
  data[1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
  data[2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
  data[3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

void write_i32(std::uint8_t* data, std::int32_t value) noexcept {
  write_u32(data, static_cast<std::uint32_t>(value));
}

bool sequence_newer(std::uint32_t candidate, std::uint32_t current) noexcept {
  const std::uint32_t distance = candidate - current;
  return (distance != 0U) && (distance < 0x80000000U);
}

std::uint32_t record_crc(const std::uint8_t* slot,
                         std::uint16_t payload_length) noexcept {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (std::size_t index = 0U; index < 16U; ++index) {
    crc ^= static_cast<std::uint32_t>(slot[index]);
    for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
      const bool lsb = (crc & 1U) != 0U;
      crc >>= 1U;
      if (lsb) {
        crc ^= 0xEDB88320U;
      }
    }
  }
  for (std::size_t index = 0U; index < payload_length; ++index) {
    crc ^= static_cast<std::uint32_t>(slot[20U + index]);
    for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
      const bool lsb = (crc & 1U) != 0U;
      crc >>= 1U;
      if (lsb) {
        crc ^= 0xEDB88320U;
      }
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

std::uint32_t request_payload_word(
    const pack_controller_bms_service_request_t& request,
    std::size_t word) noexcept {
  if (word == 0U) {
    return request.service_value0;
  }
  if (word == 1U) {
    return request.service_value1;
  }
  return request.service_value2;
}

std::uint8_t request_payload_byte(
    const pack_controller_bms_service_request_t& request,
    std::size_t index) noexcept {
  const std::uint32_t word = request_payload_word(request, index / 4U);
  const auto shift = static_cast<std::uint32_t>((index % 4U) * 8U);
  return static_cast<std::uint8_t>((word >> shift) & 0xFFU);
}

void set_reply_payload_word(NvmServiceReply& reply, std::size_t word,
                            std::uint32_t value) noexcept {
  if (word == 0U) {
    reply.value0 = value;
  } else if (word == 1U) {
    reply.value1 = value;
  } else {
    reply.value2 = value;
  }
}

}  // namespace

std::uint32_t nvm_crc32(const std::uint8_t* data,
                        std::size_t length) noexcept {
  std::uint32_t crc = 0xFFFFFFFFU;
  if (data == nullptr) {
    return 0U;
  }
  for (std::size_t index = 0U; index < length; ++index) {
    crc ^= static_cast<std::uint32_t>(data[index]);
    for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
      const bool lsb = (crc & 1U) != 0U;
      crc >>= 1U;
      if (lsb) {
        crc ^= 0xEDB88320U;
      }
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

bool NvmRecordStore::region_valid(const NvmRegion& region) const noexcept {
  const std::uint32_t end = static_cast<std::uint32_t>(region.base_address) +
                            static_cast<std::uint32_t>(region.slot_count) *
                                kNvmSlotSize;
  return (region.slot_count > 0U) && (region.record_id != 0U) &&
         (region.schema != 0U) &&
         (end <= services::EepromDriver::kSizeBytes);
}

void NvmRecordStore::reset_scan() noexcept {
  slot_index_ = 0U;
  latest_slot_ = 0U;
  latest_sequence_ = 0U;
  latest_payload_length_ = 0U;
  valid_record_seen_ = false;
  crc_error_seen_ = false;
  schema_mismatch_seen_ = false;
}

bool NvmRecordStore::start_read_latest(const NvmRegion& region,
                                       std::uint8_t* payload,
                                       std::uint16_t capacity) noexcept {
  if (busy()) {
    return false;
  }
  if (!region_valid(region) || (payload == nullptr) || (capacity == 0U) ||
      (capacity > kMaximumPayloadSize)) {
    result_ = NvmRecordResult::kInvalidArgument;
    return false;
  }
  region_ = region;
  read_destination_ = payload;
  read_capacity_ = capacity;
  operation_ = Operation::kRead;
  phase_ = Phase::kStartSlotRead;
  result_ = NvmRecordResult::kBusy;
  reset_scan();
  return true;
}

bool NvmRecordStore::start_write_next(const NvmRegion& region,
                                      const std::uint8_t* payload,
                                      std::uint16_t length) noexcept {
  if (busy()) {
    return false;
  }
  if (!region_valid(region) || (payload == nullptr) || (length == 0U) ||
      (length > kMaximumPayloadSize)) {
    result_ = NvmRecordResult::kInvalidArgument;
    return false;
  }
  std::copy_n(payload, length, write_payload_.begin());
  region_ = region;
  write_length_ = length;
  read_destination_ = nullptr;
  read_capacity_ = 0U;
  operation_ = Operation::kWrite;
  phase_ = Phase::kStartSlotRead;
  result_ = NvmRecordResult::kBusy;
  reset_scan();
  return true;
}

void NvmRecordStore::start_next_slot_read() noexcept {
  const std::uint32_t address =
      static_cast<std::uint32_t>(region_.base_address) +
      static_cast<std::uint32_t>(slot_index_) * kNvmSlotSize;
  if (!eeprom_.start_read(static_cast<std::uint16_t>(address),
                          slot_buffer_.data(), kNvmSlotSize)) {
    finish(NvmRecordResult::kCommunicationError);
    return;
  }
  phase_ = Phase::kAwaitSlotRead;
}

void NvmRecordStore::process_scanned_slot() noexcept {
  const std::uint32_t marker = read_u32(slot_buffer_.data() + kNvmCommitOffset);
  if (marker != kNvmCommitMarker) {
    return;
  }
  if ((read_u32(slot_buffer_.data()) != kNvmRecordMagic) ||
      (read_u16(slot_buffer_.data() + 4U) != region_.record_id) ||
      (read_u16(slot_buffer_.data() + 10U) != kNvmFormatVersion)) {
    crc_error_seen_ = true;
    return;
  }
  if (read_u16(slot_buffer_.data() + 6U) != region_.schema) {
    schema_mismatch_seen_ = true;
    return;
  }
  const std::uint16_t payload_length = read_u16(slot_buffer_.data() + 8U);
  if ((payload_length == 0U) || (payload_length > kMaximumPayloadSize)) {
    crc_error_seen_ = true;
    return;
  }
  const std::uint32_t stored_crc = read_u32(slot_buffer_.data() + 16U);
  if (stored_crc != record_crc(slot_buffer_.data(), payload_length)) {
    crc_error_seen_ = true;
    return;
  }
  const std::uint32_t sequence = read_u32(slot_buffer_.data() + 12U);
  if (!valid_record_seen_ || sequence_newer(sequence, latest_sequence_)) {
    valid_record_seen_ = true;
    latest_slot_ = slot_index_;
    latest_sequence_ = sequence;
    latest_payload_length_ = payload_length;
    std::copy_n(slot_buffer_.begin() + kHeaderSize, payload_length,
                retained_payload_.begin());
  }
}

void NvmRecordStore::finish_scan() noexcept {
  if (operation_ == Operation::kRead) {
    if (valid_record_seen_) {
      if (latest_payload_length_ > read_capacity_) {
        finish(NvmRecordResult::kInvalidArgument);
        return;
      }
      std::copy_n(retained_payload_.begin(), latest_payload_length_,
                  read_destination_);
      finish(NvmRecordResult::kSuccess);
      return;
    }
    if (schema_mismatch_seen_) {
      finish(NvmRecordResult::kSchemaMismatch);
    } else if (crc_error_seen_) {
      finish(NvmRecordResult::kCrcError);
    } else {
      finish(NvmRecordResult::kNotFound);
    }
    return;
  }

  const std::uint8_t target_slot = valid_record_seen_
                                       ? static_cast<std::uint8_t>(
                                             (latest_slot_ + 1U) %
                                             region_.slot_count)
                                       : 0U;
  target_address_ = static_cast<std::uint16_t>(
      static_cast<std::uint32_t>(region_.base_address) +
      static_cast<std::uint32_t>(target_slot) * kNvmSlotSize);
  latest_slot_ = target_slot;
  const std::uint32_t next_sequence =
      valid_record_seen_ ? latest_sequence_ + 1U : 1U;
  build_record(next_sequence);
  marker_buffer_.fill(0U);
  phase_ = Phase::kInvalidateMarker;
}

void NvmRecordStore::build_record(std::uint32_t sequence) noexcept {
  slot_buffer_.fill(0xFFU);
  write_u32(slot_buffer_.data(), kNvmRecordMagic);
  write_u16(slot_buffer_.data() + 4U, region_.record_id);
  write_u16(slot_buffer_.data() + 6U, region_.schema);
  write_u16(slot_buffer_.data() + 8U, write_length_);
  write_u16(slot_buffer_.data() + 10U, kNvmFormatVersion);
  write_u32(slot_buffer_.data() + 12U, sequence);
  std::copy_n(write_payload_.begin(), write_length_,
              slot_buffer_.begin() + kHeaderSize);
  write_u32(slot_buffer_.data() + 16U,
            record_crc(slot_buffer_.data(), write_length_));
  write_u32(slot_buffer_.data() + kNvmCommitOffset, kNvmCommitMarker);
  write_u32(marker_buffer_.data(), kNvmCommitMarker);
  latest_sequence_ = sequence;
  latest_payload_length_ = write_length_;
}

void NvmRecordStore::finish(NvmRecordResult result) noexcept {
  result_ = result;
  operation_ = Operation::kNone;
  phase_ = Phase::kIdle;
}

void NvmRecordStore::service() noexcept {
  if (!busy()) {
    return;
  }
  eeprom_.service();

  if (phase_ == Phase::kStartSlotRead) {
    start_next_slot_read();
    return;
  }
  if (phase_ == Phase::kAwaitSlotRead) {
    if (eeprom_.busy()) {
      return;
    }
    if (eeprom_.result() != services::EepromResult::kSuccess) {
      finish(NvmRecordResult::kCommunicationError);
      return;
    }
    process_scanned_slot();
    ++slot_index_;
    if (slot_index_ < region_.slot_count) {
      phase_ = Phase::kStartSlotRead;
    } else {
      finish_scan();
    }
    return;
  }
  if (phase_ == Phase::kInvalidateMarker) {
    if (!eeprom_.start_write(
            static_cast<std::uint16_t>(target_address_ + kNvmCommitOffset),
            marker_buffer_.data(),
            static_cast<std::uint16_t>(marker_buffer_.size()))) {
      finish(NvmRecordResult::kCommunicationError);
      return;
    }
    phase_ = Phase::kAwaitInvalidateMarker;
    return;
  }
  if (phase_ == Phase::kAwaitInvalidateMarker) {
    if (eeprom_.busy()) {
      return;
    }
    if (eeprom_.result() != services::EepromResult::kSuccess) {
      finish(eeprom_.result() == services::EepromResult::kVerifyError
                 ? NvmRecordResult::kVerifyError
                 : NvmRecordResult::kCommunicationError);
      return;
    }
    phase_ = Phase::kWriteRecordBody;
    return;
  }
  if (phase_ == Phase::kWriteRecordBody) {
    if (!eeprom_.start_write(target_address_, slot_buffer_.data(),
                             kNvmCommitOffset)) {
      finish(NvmRecordResult::kCommunicationError);
      return;
    }
    phase_ = Phase::kAwaitRecordBody;
    return;
  }
  if (phase_ == Phase::kAwaitRecordBody) {
    if (eeprom_.busy()) {
      return;
    }
    if (eeprom_.result() != services::EepromResult::kSuccess) {
      finish(eeprom_.result() == services::EepromResult::kVerifyError
                 ? NvmRecordResult::kVerifyError
                 : NvmRecordResult::kCommunicationError);
      return;
    }
    write_u32(marker_buffer_.data(), kNvmCommitMarker);
    phase_ = Phase::kWriteCommitMarker;
    return;
  }
  if (phase_ == Phase::kWriteCommitMarker) {
    if (!eeprom_.start_write(
            static_cast<std::uint16_t>(target_address_ + kNvmCommitOffset),
            marker_buffer_.data(),
            static_cast<std::uint16_t>(marker_buffer_.size()))) {
      finish(NvmRecordResult::kCommunicationError);
      return;
    }
    phase_ = Phase::kAwaitCommitMarker;
    return;
  }
  if (phase_ == Phase::kAwaitCommitMarker) {
    if (eeprom_.busy()) {
      return;
    }
    if (eeprom_.result() != services::EepromResult::kSuccess) {
      finish(eeprom_.result() == services::EepromResult::kVerifyError
                 ? NvmRecordResult::kVerifyError
                 : NvmRecordResult::kCommunicationError);
      return;
    }
    phase_ = Phase::kVerifyCommittedSlot;
    return;
  }
  if (phase_ == Phase::kVerifyCommittedSlot) {
    if (!eeprom_.start_read(target_address_, verify_buffer_.data(),
                            kNvmSlotSize)) {
      finish(NvmRecordResult::kCommunicationError);
      return;
    }
    phase_ = Phase::kAwaitCommittedSlot;
    return;
  }
  if (phase_ == Phase::kAwaitCommittedSlot) {
    if (eeprom_.busy()) {
      return;
    }
    if (eeprom_.result() != services::EepromResult::kSuccess) {
      finish(NvmRecordResult::kCommunicationError);
      return;
    }
    const bool matches = std::equal(slot_buffer_.begin(), slot_buffer_.end(),
                                    verify_buffer_.begin());
    finish(matches ? NvmRecordResult::kSuccess
                   : NvmRecordResult::kVerifyError);
  }
}

SystemConfig default_system_config() noexcept { return SystemConfig{}; }

bool validate_system_config(const SystemConfig& config) noexcept {
  return (config.schema == kSystemConfigSchema) &&
         (config.cell_profile_id == 1U) && (config.series_cells == 162U) &&
         (config.parallel_cells == 2U) &&
         (config.hv.precharge_timeout_ms >= 100U) &&
         (config.hv.precharge_timeout_ms <= 10000U) &&
         (config.hv.feedback_confirm_ms >= 10U) &&
         (config.hv.feedback_confirm_ms <= config.hv.feedback_timeout_ms) &&
         (config.hv.feedback_timeout_ms <= 1000U) &&
         (config.hv.precharge_ratio_min_permille <
          config.hv.precharge_ratio_max_permille) &&
         (config.hv.precharge_ratio_min_permille >= 500U) &&
         (config.hv.precharge_ratio_max_permille <= 1500U) &&
         (config.hv.precharge_min_mv <= 800000U) &&
         (config.charge.taper_start_mv < config.charge.charge_end_mv) &&
         (config.charge.charge_end_mv <= 4250U) &&
         (config.charge.full_max_cell_min_mv <=
          config.charge.charge_end_mv) &&
         (config.charge.full_pack_min_mv <=
          config.charge.full_max_cell_min_mv) &&
         (config.charge.recharge_mv < config.charge.taper_start_mv) &&
         (config.charge.full_spread_max_mv <= 500U) &&
         (config.charge.full_time_ms >= 1000U) &&
         (config.charge.full_time_ms <= 120000U) &&
         (config.charge.learn_low_soc_permille <= 500U) &&
         (config.charge.energy_learn_alpha_permille <= 1000U) &&
         (config.imd.hardware_type <= 2U) &&
         (config.imd.average_count > 0U) &&
         (config.imd.average_count <= 100U) &&
         (config.imd.ran_kohm >= 100U) &&
         (config.imd.ran_kohm <= 10000U) &&
         (config.imd.pwm_timeout_ms >= 100U) &&
         (config.imd.pwm_timeout_ms <= 5000U) &&
         (config.imd.startup_timeout_ms >= 1000U) &&
         (config.imd.startup_timeout_ms <= 60000U) &&
         (config.imd.isolation_critical_kohm <
          config.imd.isolation_recovery_kohm) &&
         (config.leakage.settle_ms >= 10U) &&
         (config.leakage.settle_ms <= 50U) &&
         (config.leakage.sample_count >= 8U) &&
         (config.leakage.sample_count <= 64U) &&
         (config.leakage.warning_kohm > config.leakage.leak_kohm) &&
         (config.leakage.leak_kohm > config.leakage.severe_kohm) &&
         (config.leakage.confirmation_count > 0U) &&
         (config.leakage.confirmation_count <= 10U) &&
         (config.analog.hv_gain_milli > 0U) &&
         (config.analog.hv_gain_milli <= 1000000U) &&
         (config.analog.vbatt_gain_millionths > 0U) &&
         (config.analog.vbatt_gain_millionths <= 20000000U) &&
         (config.analog.nominal_vref_mv >= 3000U) &&
         (config.analog.nominal_vref_mv <= 3600U) &&
         (config.analog.leakage_supply_mv >= 3000U) &&
         (config.analog.leakage_supply_mv <= 3600U) &&
         (config.analog.hv_offset_uv[0U] >= -50000000) &&
         (config.analog.hv_offset_uv[0U] <= 50000000) &&
         (config.analog.hv_offset_uv[1U] >= -50000000) &&
         (config.analog.hv_offset_uv[1U] <= 50000000) &&
         (config.analog.hv_offset_uv[2U] >= -50000000) &&
         (config.analog.hv_offset_uv[2U] <= 50000000) &&
         (config.analog.vbatt_offset_uv >= -2000000) &&
         (config.analog.vbatt_offset_uv <= 2000000);
}

void serialize_system_config(
    const SystemConfig& config,
    std::array<std::uint8_t, kSystemConfigPayloadSize>& payload) noexcept {
  payload.fill(0U);
  write_u16(payload.data(), config.schema);
  write_u16(payload.data() + 2U, config.cell_profile_id);
  write_u16(payload.data() + 4U, config.series_cells);
  payload[6U] = config.parallel_cells;

  write_u16(payload.data() + 8U, config.hv.precharge_timeout_ms);
  write_u16(payload.data() + 10U, config.hv.feedback_confirm_ms);
  write_u16(payload.data() + 12U, config.hv.feedback_timeout_ms);
  write_u16(payload.data() + 14U,
            config.hv.precharge_ratio_min_permille);
  write_u16(payload.data() + 16U,
            config.hv.precharge_ratio_max_permille);
  write_u32(payload.data() + 20U, config.hv.precharge_min_mv);

  write_u16(payload.data() + 24U, config.charge.taper_start_mv);
  write_u16(payload.data() + 26U, config.charge.charge_end_mv);
  write_u16(payload.data() + 28U, config.charge.full_max_cell_min_mv);
  write_u16(payload.data() + 30U, config.charge.full_pack_min_mv);
  write_u16(payload.data() + 32U, config.charge.full_spread_max_mv);
  write_u16(payload.data() + 34U, config.charge.recharge_mv);
  write_u16(payload.data() + 36U, config.charge.charge_detect_ma);
  write_u16(payload.data() + 38U, config.charge.full_current_ma);
  write_u32(payload.data() + 40U, config.charge.full_time_ms);
  write_u16(payload.data() + 44U,
            config.charge.learn_low_soc_permille);
  write_u16(payload.data() + 46U,
            config.charge.energy_learn_alpha_permille);

  payload[56U] = config.imd.hardware_type;
  payload[57U] = config.imd.undervoltage_behavior;
  write_u16(payload.data() + 58U, config.imd.average_count);
  write_u32(payload.data() + 60U, config.imd.ran_kohm);
  write_u32(payload.data() + 64U, config.imd.pwm_timeout_ms);
  write_u32(payload.data() + 68U, config.imd.startup_timeout_ms);
  write_u16(payload.data() + 72U, config.imd.isolation_critical_kohm);
  write_u16(payload.data() + 74U, config.imd.isolation_recovery_kohm);

  write_u16(payload.data() + 76U, config.leakage.settle_ms);
  write_u16(payload.data() + 78U, config.leakage.sample_count);
  write_u32(payload.data() + 80U, config.leakage.warning_kohm);
  write_u32(payload.data() + 84U, config.leakage.leak_kohm);
  write_u32(payload.data() + 88U, config.leakage.severe_kohm);
  write_u16(payload.data() + 92U,
            config.leakage.recovery_hysteresis_permille);
  payload[94U] = config.leakage.confirmation_count;

  write_u32(payload.data() + 96U, config.analog.hv_gain_milli);
  write_u32(payload.data() + 100U,
            config.analog.vbatt_gain_millionths);
  write_u16(payload.data() + 104U, config.analog.nominal_vref_mv);
  for (std::size_t index = 0U; index < config.analog.hv_offset_uv.size();
       ++index) {
    write_i32(payload.data() + 108U + index * 4U,
              config.analog.hv_offset_uv[index]);
  }
  write_i32(payload.data() + 120U, config.analog.vbatt_offset_uv);
  write_u16(payload.data() + 124U,
            config.analog.leakage_supply_mv);
}

bool deserialize_system_config(
    const std::array<std::uint8_t, kSystemConfigPayloadSize>& payload,
    SystemConfig& config) noexcept {
  SystemConfig decoded{};
  decoded.schema = read_u16(payload.data());
  decoded.cell_profile_id = read_u16(payload.data() + 2U);
  decoded.series_cells = read_u16(payload.data() + 4U);
  decoded.parallel_cells = payload[6U];
  decoded.hv.precharge_timeout_ms = read_u16(payload.data() + 8U);
  decoded.hv.feedback_confirm_ms = read_u16(payload.data() + 10U);
  decoded.hv.feedback_timeout_ms = read_u16(payload.data() + 12U);
  decoded.hv.precharge_ratio_min_permille = read_u16(payload.data() + 14U);
  decoded.hv.precharge_ratio_max_permille = read_u16(payload.data() + 16U);
  decoded.hv.precharge_min_mv = read_u32(payload.data() + 20U);
  decoded.charge.taper_start_mv = read_u16(payload.data() + 24U);
  decoded.charge.charge_end_mv = read_u16(payload.data() + 26U);
  decoded.charge.full_max_cell_min_mv = read_u16(payload.data() + 28U);
  decoded.charge.full_pack_min_mv = read_u16(payload.data() + 30U);
  decoded.charge.full_spread_max_mv = read_u16(payload.data() + 32U);
  decoded.charge.recharge_mv = read_u16(payload.data() + 34U);
  decoded.charge.charge_detect_ma = read_u16(payload.data() + 36U);
  decoded.charge.full_current_ma = read_u16(payload.data() + 38U);
  decoded.charge.full_time_ms = read_u32(payload.data() + 40U);
  decoded.charge.learn_low_soc_permille = read_u16(payload.data() + 44U);
  decoded.charge.energy_learn_alpha_permille =
      read_u16(payload.data() + 46U);
  decoded.imd.hardware_type = payload[56U];
  decoded.imd.undervoltage_behavior = payload[57U];
  decoded.imd.average_count = read_u16(payload.data() + 58U);
  decoded.imd.ran_kohm = read_u32(payload.data() + 60U);
  decoded.imd.pwm_timeout_ms = read_u32(payload.data() + 64U);
  decoded.imd.startup_timeout_ms = read_u32(payload.data() + 68U);
  decoded.imd.isolation_critical_kohm = read_u16(payload.data() + 72U);
  decoded.imd.isolation_recovery_kohm = read_u16(payload.data() + 74U);
  decoded.leakage.settle_ms = read_u16(payload.data() + 76U);
  decoded.leakage.sample_count = read_u16(payload.data() + 78U);
  decoded.leakage.warning_kohm = read_u32(payload.data() + 80U);
  decoded.leakage.leak_kohm = read_u32(payload.data() + 84U);
  decoded.leakage.severe_kohm = read_u32(payload.data() + 88U);
  decoded.leakage.recovery_hysteresis_permille =
      read_u16(payload.data() + 92U);
  decoded.leakage.confirmation_count = payload[94U];
  decoded.analog.hv_gain_milli = read_u32(payload.data() + 96U);
  decoded.analog.vbatt_gain_millionths = read_u32(payload.data() + 100U);
  decoded.analog.nominal_vref_mv = read_u16(payload.data() + 104U);
  for (std::size_t index = 0U; index < decoded.analog.hv_offset_uv.size();
       ++index) {
    decoded.analog.hv_offset_uv[index] =
        read_i32(payload.data() + 108U + index * 4U);
  }
  decoded.analog.vbatt_offset_uv = read_i32(payload.data() + 120U);
  decoded.analog.leakage_supply_mv = read_u16(payload.data() + 124U);
  if (!validate_system_config(decoded)) {
    return false;
  }
  config = decoded;
  return true;
}

NvmManager::NvmManager(services::EepromDriver& eeprom) noexcept
    : eeprom_(eeprom), store_(eeprom) {
  serialize_system_config(stored_config_, stored_payload_);
  staged_payload_ = stored_payload_;
}

bool NvmManager::start_boot_load() noexcept {
  if (state_ != State::kIdle) {
    return false;
  }
  initialized_ = false;
  if (!store_.start_read_latest(kSystemConfigRegion, stored_payload_.data(),
                                static_cast<std::uint16_t>(
                                    stored_payload_.size()))) {
    communication_fault_ = true;
    count_error();
    initialized_ = true;
    return false;
  }
  state_ = State::kBootLoad;
  return true;
}

NvmManager::TargetRange NvmManager::target_range(
    std::uint16_t target) noexcept {
  switch (target) {
    case kTargetSystemConfig:
      return {0U, 128U, true};
    case kTargetCellProfile:
      return {0U, 8U, true};
    case kTargetHvConfig:
      return {8U, 16U, true};
    case kTargetChargeConfig:
      return {24U, 32U, true};
    case kTargetImdConfig:
      return {56U, 20U, true};
    case kTargetLeakageConfig:
      return {76U, 20U, true};
    case kTargetAdcCalibration:
      return {96U, 32U, true};
    default:
      return {};
  }
}

NvmServiceReply NvmManager::handle_config_read(
    const services::ServiceRequest& request) const noexcept {
  NvmServiceReply reply{true, false, kServiceInvalidTarget};
  const TargetRange range = target_range(request.message.service_target);
  if (!range.valid) {
    return reply;
  }
  const std::uint16_t subindex = request.message.service_sub_index;
  if ((subindex >= range.length) ||
      (request.message.service_payload_length != 0U) ||
      (request.message.service_commit_request != 0U)) {
    reply.result = kServiceInvalidValue;
    return reply;
  }
  const std::uint16_t remaining =
      static_cast<std::uint16_t>(range.length - subindex);
  const std::size_t count = std::min<std::size_t>(12U, remaining);
  for (std::size_t index = 0U; index < count; ++index) {
    const std::size_t word = index / 4U;
    const auto shift = static_cast<std::uint32_t>((index % 4U) * 8U);
    std::uint32_t value = word == 0U ? reply.value0
                                    : (word == 1U ? reply.value1
                                                  : reply.value2);
    value |= static_cast<std::uint32_t>(
                 stored_payload_[range.offset + subindex + index])
             << shift;
    set_reply_payload_word(reply, word, value);
  }
  reply.result = kServiceOk;
  reply.nvm_sequence = sequence_;
  return reply;
}

NvmServiceReply NvmManager::handle_config_stage(
    const services::ServiceRequest& request) noexcept {
  NvmServiceReply reply{true, false, kServiceInvalidTarget};
  const TargetRange range = target_range(request.message.service_target);
  if (!range.valid) {
    return reply;
  }
  const std::uint8_t length = request.message.service_payload_length;
  const std::uint16_t subindex = request.message.service_sub_index;
  if ((length == 0U) || (length > 12U) ||
      (static_cast<std::uint32_t>(subindex) + length > range.length) ||
      (request.message.service_commit_request != 0U)) {
    reply.result = kServiceInvalidValue;
    return reply;
  }
  if (staging_active_ && (staged_target_ != request.message.service_target)) {
    reply.result = kServiceBusy;
    return reply;
  }
  if (!staging_active_) {
    staged_payload_ = stored_payload_;
    staged_target_ = request.message.service_target;
    staging_active_ = true;
  }
  for (std::size_t index = 0U; index < length; ++index) {
    staged_payload_[range.offset + subindex + index] =
        request_payload_byte(request.message, index);
  }
  reply.result = kServiceOk;
  reply.value0 = length;
  reply.nvm_sequence = sequence_;
  return reply;
}

NvmServiceReply NvmManager::handle_config_commit(
    const services::ServiceRequest& request, bool write_allowed) noexcept {
  NvmServiceReply reply{true, false, kServiceInvalidTarget};
  const TargetRange range = target_range(request.message.service_target);
  if (!range.valid) {
    return reply;
  }
  if (!write_allowed) {
    reply.result = kServiceDenied;
    return reply;
  }
  if (!staging_active_ ||
      (staged_target_ != request.message.service_target) ||
      (request.message.service_commit_request != 1U) ||
      (request.message.service_payload_length != 0U)) {
    reply.result = kServiceInvalidValue;
    return reply;
  }
  SystemConfig candidate{};
  if (!deserialize_system_config(staged_payload_, candidate)) {
    reply.result = kServiceInvalidValue;
    return reply;
  }
  if (!store_.start_write_next(
          kSystemConfigRegion, staged_payload_.data(),
          static_cast<std::uint16_t>(staged_payload_.size()))) {
    reply.result = kServiceBusy;
    return reply;
  }
  pending_request_ = request;
  pending_service_ = true;
  state_ = State::kConfigCommit;
  reply.result = kServiceBusy;
  reply.deferred = true;
  return reply;
}

void NvmManager::build_selftest_page(std::uint32_t sequence) noexcept {
  selftest_page_.fill(0U);
  write_u32(selftest_page_.data(), kSelftestMagic);
  write_u32(selftest_page_.data() + 4U, sequence);
  for (std::size_t index = 8U; index < 56U; ++index) {
    selftest_page_[index] = static_cast<std::uint8_t>(
        static_cast<std::uint8_t>(index) ^
        static_cast<std::uint8_t>(sequence & 0xFFU) ^ 0xA5U);
  }
  write_u32(selftest_page_.data() + 56U,
            nvm_crc32(selftest_page_.data(), 56U));
  write_u32(selftest_page_.data() + 60U, kSelftestMarker);
}

bool NvmManager::validate_selftest_page() const noexcept {
  return (read_u32(selftest_page_.data()) == kSelftestMagic) &&
         (read_u32(selftest_page_.data() + 56U) ==
          nvm_crc32(selftest_page_.data(), 56U)) &&
         (read_u32(selftest_page_.data() + 60U) == kSelftestMarker);
}

NvmServiceReply NvmManager::handle_selftest(
    const services::ServiceRequest& request, bool write_allowed) noexcept {
  NvmServiceReply reply{true, false, kServiceInvalidValue};
  if (!write_allowed) {
    reply.result = kServiceDenied;
    return reply;
  }
  if ((request.message.service_commit_request != 1U) ||
      (request.message.service_payload_length != 0U)) {
    return reply;
  }
  ++selftest_sequence_;
  build_selftest_page(selftest_sequence_);
  if (!eeprom_.start_write(
          kEepromTestPageAddress, selftest_page_.data(),
          static_cast<std::uint16_t>(selftest_page_.size()))) {
    reply.result = kServiceBusy;
    return reply;
  }
  pending_request_ = request;
  pending_service_ = true;
  state_ = State::kSelftestWrite;
  reply.result = kServiceBusy;
  reply.deferred = true;
  return reply;
}

NvmServiceReply NvmManager::handle(const services::ServiceRequest& request,
                                   bool write_allowed) noexcept {
  NvmServiceReply reply{};
  if (!request.valid) {
    return reply;
  }
  const std::uint8_t command = request.message.service_command;
  const bool recognized =
      (command ==
       PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_CONFIG_READ_CHOICE) ||
      (command ==
       PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_CONFIG_STAGE_CHOICE) ||
      (command ==
       PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_CONFIG_COMMIT_CHOICE) ||
      (command ==
       PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_EEPROM_SELFTEST_CHOICE);
  if (!recognized) {
    return reply;
  }
  reply.handled = true;
  if (!initialized_ || (state_ != State::kIdle)) {
    reply.result = kServiceBusy;
    return reply;
  }
  if (command ==
      PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_CONFIG_READ_CHOICE) {
    return handle_config_read(request);
  }
  if (command ==
      PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_CONFIG_STAGE_CHOICE) {
    return handle_config_stage(request);
  }
  if (command ==
      PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_CONFIG_COMMIT_CHOICE) {
    return handle_config_commit(request, write_allowed);
  }
  return handle_selftest(request, write_allowed);
}

void NvmManager::count_error() noexcept {
  if (error_count_ != std::numeric_limits<std::uint16_t>::max()) {
    ++error_count_;
  }
}

void NvmManager::complete_pending(std::uint8_t result) noexcept {
  if (!pending_service_) {
    return;
  }
  completion_.request = pending_request_;
  completion_.reply = {true, false, result, 0U, 0U, 0U, sequence_};
  completion_available_ = true;
  pending_service_ = false;
}

void NvmManager::service() noexcept {
  if (state_ == State::kBootLoad || state_ == State::kConfigCommit) {
    store_.service();
    if (store_.busy()) {
      return;
    }
    const NvmRecordResult result = store_.result();
    if (state_ == State::kBootLoad) {
      initialized_ = true;
      record_crc_fault_ = store_.crc_error_seen();
      schema_mismatch_fault_ = store_.schema_mismatch_seen() ||
                               (result == NvmRecordResult::kSchemaMismatch);
      if (result == NvmRecordResult::kSuccess) {
        SystemConfig loaded{};
        if ((store_.payload_length() == stored_payload_.size()) &&
            deserialize_system_config(stored_payload_, loaded)) {
          active_config_ = loaded;
          stored_config_ = loaded;
          staged_payload_ = stored_payload_;
          sequence_ = store_.latest_sequence();
          communication_fault_ = false;
          record_crc_fault_ = false;
          schema_mismatch_fault_ = false;
        } else {
          cell_profile_invalid_fault_ =
              (read_u16(stored_payload_.data() + 2U) != 1U) ||
              (read_u16(stored_payload_.data() + 4U) != 162U) ||
              (stored_payload_[6U] != 2U);
          config_invalid_fault_ = !cell_profile_invalid_fault_;
          count_error();
        }
      } else if (result == NvmRecordResult::kCommunicationError) {
        communication_fault_ = true;
        count_error();
      } else if ((result == NvmRecordResult::kCrcError) ||
                 (result == NvmRecordResult::kSchemaMismatch)) {
        count_error();
      }
      state_ = State::kIdle;
      return;
    }

    if (result == NvmRecordResult::kSuccess) {
      SystemConfig stored{};
      if (deserialize_system_config(staged_payload_, stored)) {
        stored_config_ = stored;
        stored_payload_ = staged_payload_;
        sequence_ = store_.latest_sequence();
        staging_active_ = false;
        communication_fault_ = false;
        write_verify_fault_ = false;
        complete_pending(kServiceOk);
      } else {
        config_invalid_fault_ = true;
        count_error();
        complete_pending(kServiceNvmError);
      }
    } else {
      communication_fault_ = result == NvmRecordResult::kCommunicationError;
      write_verify_fault_ = result == NvmRecordResult::kVerifyError;
      count_error();
      complete_pending(kServiceNvmError);
    }
    state_ = State::kIdle;
    return;
  }

  if (state_ == State::kSelftestWrite) {
    eeprom_.service();
    if (eeprom_.busy()) {
      return;
    }
    if (eeprom_.result() != services::EepromResult::kSuccess) {
      communication_fault_ =
          eeprom_.result() == services::EepromResult::kCommunicationError;
      write_verify_fault_ =
          eeprom_.result() == services::EepromResult::kVerifyError;
      selftest_fault_ = true;
      count_error();
      complete_pending(kServiceNvmError);
      state_ = State::kIdle;
      return;
    }
    selftest_page_.fill(0U);
    if (!eeprom_.start_read(
            kEepromTestPageAddress, selftest_page_.data(),
            static_cast<std::uint16_t>(selftest_page_.size()))) {
      communication_fault_ = true;
      selftest_fault_ = true;
      count_error();
      complete_pending(kServiceNvmError);
      state_ = State::kIdle;
      return;
    }
    state_ = State::kSelftestRead;
    return;
  }

  if (state_ == State::kSelftestRead) {
    eeprom_.service();
    if (eeprom_.busy()) {
      return;
    }
    const bool success =
        (eeprom_.result() == services::EepromResult::kSuccess) &&
        validate_selftest_page();
    if (!success) {
      communication_fault_ =
          eeprom_.result() == services::EepromResult::kCommunicationError;
      selftest_fault_ = true;
      count_error();
    } else {
      communication_fault_ = false;
      write_verify_fault_ = false;
      selftest_fault_ = false;
    }
    complete_pending(success ? kServiceOk : kServiceNvmError);
    state_ = State::kIdle;
  }
}

bool NvmManager::take_completion(
    NvmServiceCompletion& completion) noexcept {
  if (!completion_available_) {
    return false;
  }
  completion = completion_;
  completion_available_ = false;
  return true;
}

}  // namespace packcontroller::app
