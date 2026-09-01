#ifndef PACKCONTROLLER_APP_NVM_HPP
#define PACKCONTROLLER_APP_NVM_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "packcontroller/services/can_service.hpp"
#include "packcontroller/services/eeprom_driver.hpp"

namespace packcontroller::app {

inline constexpr std::uint16_t kSystemConfigSlotAAddress = 0x0000U;
inline constexpr std::uint16_t kSystemConfigSlotBAddress = 0x0200U;
inline constexpr std::uint16_t kSocSohJournalAddress = 0x0400U;
inline constexpr std::uint8_t kSocSohJournalSlotCount = 14U;
inline constexpr std::uint16_t kEepromTestPageAddress = 0x7FC0U;
inline constexpr std::uint16_t kNvmSlotSize = 512U;
inline constexpr std::uint16_t kNvmCommitOffset = 508U;
inline constexpr std::uint32_t kNvmRecordMagic = 0x564E4350U;
inline constexpr std::uint32_t kNvmCommitMarker = 0xC35AA53CU;
inline constexpr std::uint16_t kNvmFormatVersion = 1U;
inline constexpr std::uint16_t kSystemConfigRecordId = 1U;
inline constexpr std::uint16_t kSystemConfigSchema = 1U;
inline constexpr std::size_t kSystemConfigPayloadSize = 128U;

static_assert(kSocSohJournalAddress +
                  (kSocSohJournalSlotCount * kNvmSlotSize) ==
              0x2000U);
static_assert(kEepromTestPageAddress + 64U ==
              services::EepromDriver::kSizeBytes);

enum class NvmRecordResult : std::uint8_t {
  kIdle = 0U,
  kBusy = 1U,
  kSuccess = 2U,
  kNotFound = 3U,
  kInvalidArgument = 4U,
  kCommunicationError = 5U,
  kCrcError = 6U,
  kVerifyError = 7U,
  kSchemaMismatch = 8U,
};

struct NvmRegion final {
  std::uint16_t base_address{0U};
  std::uint16_t record_id{0U};
  std::uint16_t schema{0U};
  std::uint8_t slot_count{0U};
};

class NvmRecordStore final {
 public:
  explicit NvmRecordStore(services::EepromDriver& eeprom) noexcept
      : eeprom_(eeprom) {}

  bool start_read_latest(const NvmRegion& region, std::uint8_t* payload,
                         std::uint16_t capacity) noexcept;
  bool start_write_next(const NvmRegion& region, const std::uint8_t* payload,
                        std::uint16_t length) noexcept;
  void service() noexcept;

  [[nodiscard]] NvmRecordResult result() const noexcept { return result_; }
  [[nodiscard]] bool busy() const noexcept {
    return result_ == NvmRecordResult::kBusy;
  }
  [[nodiscard]] std::uint32_t latest_sequence() const noexcept {
    return latest_sequence_;
  }
  [[nodiscard]] std::uint16_t payload_length() const noexcept {
    return latest_payload_length_;
  }
  [[nodiscard]] bool crc_error_seen() const noexcept {
    return crc_error_seen_;
  }
  [[nodiscard]] bool schema_mismatch_seen() const noexcept {
    return schema_mismatch_seen_;
  }

 private:
  enum class Operation : std::uint8_t { kNone, kRead, kWrite };
  enum class Phase : std::uint8_t {
    kIdle,
    kStartSlotRead,
    kAwaitSlotRead,
    kInvalidateMarker,
    kAwaitInvalidateMarker,
    kWriteRecordBody,
    kAwaitRecordBody,
    kWriteCommitMarker,
    kAwaitCommitMarker,
    kVerifyCommittedSlot,
    kAwaitCommittedSlot,
  };

  static constexpr std::size_t kHeaderSize = 20U;
  static constexpr std::size_t kMaximumPayloadSize =
      kNvmCommitOffset - kHeaderSize;

  [[nodiscard]] bool region_valid(const NvmRegion& region) const noexcept;
  void reset_scan() noexcept;
  void start_next_slot_read() noexcept;
  void process_scanned_slot() noexcept;
  void finish_scan() noexcept;
  void build_record(std::uint32_t sequence) noexcept;
  void finish(NvmRecordResult result) noexcept;

  services::EepromDriver& eeprom_;
  NvmRecordResult result_{NvmRecordResult::kIdle};
  Operation operation_{Operation::kNone};
  Phase phase_{Phase::kIdle};
  NvmRegion region_{};
  std::uint8_t slot_index_{0U};
  std::uint8_t latest_slot_{0U};
  std::uint32_t latest_sequence_{0U};
  std::uint16_t latest_payload_length_{0U};
  bool valid_record_seen_{false};
  bool crc_error_seen_{false};
  bool schema_mismatch_seen_{false};
  std::uint8_t* read_destination_{nullptr};
  std::uint16_t read_capacity_{0U};
  std::uint16_t write_length_{0U};
  std::uint16_t target_address_{0U};
  std::array<std::uint8_t, kNvmSlotSize> slot_buffer_{};
  std::array<std::uint8_t, kNvmSlotSize> verify_buffer_{};
  std::array<std::uint8_t, kMaximumPayloadSize> retained_payload_{};
  std::array<std::uint8_t, kMaximumPayloadSize> write_payload_{};
  std::array<std::uint8_t, 4U> marker_buffer_{};
};

struct HvConfig final {
  std::uint16_t precharge_timeout_ms{3000U};
  std::uint16_t feedback_confirm_ms{100U};
  std::uint16_t feedback_timeout_ms{200U};
  std::uint16_t precharge_ratio_min_permille{900U};
  std::uint16_t precharge_ratio_max_permille{1100U};
  std::uint32_t precharge_min_mv{364500U};
};

struct ChargeConfig final {
  std::uint16_t taper_start_mv{4150U};
  std::uint16_t charge_end_mv{4200U};
  std::uint16_t full_max_cell_min_mv{4195U};
  std::uint16_t full_pack_min_mv{4150U};
  std::uint16_t full_spread_max_mv{50U};
  std::uint16_t recharge_mv{4100U};
  std::uint16_t charge_detect_ma{500U};
  std::uint16_t full_current_ma{1000U};
  std::uint32_t full_time_ms{30000U};
  std::uint16_t learn_low_soc_permille{100U};
  std::uint16_t energy_learn_alpha_permille{100U};
};

struct ImdConfig final {
  std::uint8_t hardware_type{0U};
  std::uint8_t undervoltage_behavior{0U};
  std::uint16_t average_count{10U};
  std::uint32_t ran_kohm{300U};
  std::uint32_t pwm_timeout_ms{500U};
  std::uint32_t startup_timeout_ms{25000U};
  std::uint16_t isolation_critical_kohm{300U};
  std::uint16_t isolation_recovery_kohm{330U};
};

struct LeakageConfig final {
  std::uint16_t settle_ms{20U};
  std::uint16_t sample_count{8U};
  std::uint32_t warning_kohm{3000U};
  std::uint32_t leak_kohm{1000U};
  std::uint32_t severe_kohm{300U};
  std::uint16_t recovery_hysteresis_permille{100U};
  std::uint8_t confirmation_count{2U};
};

struct AnalogCalibration final {
  std::uint32_t hv_gain_milli{280167U};
  std::uint32_t vbatt_gain_millionths{5545450U};
  std::uint16_t nominal_vref_mv{3300U};
  std::array<std::int32_t, 3U> hv_offset_uv{};
  std::int32_t vbatt_offset_uv{0};
  std::uint16_t leakage_supply_mv{3300U};
};

struct SystemConfig final {
  std::uint16_t schema{kSystemConfigSchema};
  std::uint16_t cell_profile_id{1U};
  std::uint16_t series_cells{162U};
  std::uint8_t parallel_cells{2U};
  HvConfig hv{};
  ChargeConfig charge{};
  ImdConfig imd{};
  LeakageConfig leakage{};
  AnalogCalibration analog{};
};

[[nodiscard]] SystemConfig default_system_config() noexcept;
[[nodiscard]] bool validate_system_config(const SystemConfig& config) noexcept;
void serialize_system_config(
    const SystemConfig& config,
    std::array<std::uint8_t, kSystemConfigPayloadSize>& payload) noexcept;
[[nodiscard]] bool deserialize_system_config(
    const std::array<std::uint8_t, kSystemConfigPayloadSize>& payload,
    SystemConfig& config) noexcept;
[[nodiscard]] std::uint32_t nvm_crc32(const std::uint8_t* data,
                                      std::size_t length) noexcept;

struct NvmServiceReply final {
  bool handled{false};
  bool deferred{false};
  std::uint8_t result{3U};
  std::uint32_t value0{0U};
  std::uint32_t value1{0U};
  std::uint32_t value2{0U};
  std::uint32_t nvm_sequence{0U};
};

struct NvmServiceCompletion final {
  services::ServiceRequest request{};
  NvmServiceReply reply{};
};

class NvmManager final {
 public:
  explicit NvmManager(services::EepromDriver& eeprom) noexcept;

  bool start_boot_load() noexcept;
  void service() noexcept;
  NvmServiceReply handle(const services::ServiceRequest& request,
                         bool write_allowed) noexcept;
  bool take_completion(NvmServiceCompletion& completion) noexcept;

  [[nodiscard]] bool initialized() const noexcept { return initialized_; }
  [[nodiscard]] const SystemConfig& active_config() const noexcept {
    return active_config_;
  }
  [[nodiscard]] std::uint32_t sequence() const noexcept { return sequence_; }
  [[nodiscard]] bool communication_fault() const noexcept {
    return communication_fault_;
  }
  [[nodiscard]] bool record_crc_fault() const noexcept {
    return record_crc_fault_;
  }
  [[nodiscard]] bool write_verify_fault() const noexcept {
    return write_verify_fault_;
  }
  [[nodiscard]] bool selftest_fault() const noexcept {
    return selftest_fault_;
  }
  [[nodiscard]] bool schema_mismatch_fault() const noexcept {
    return schema_mismatch_fault_;
  }
  [[nodiscard]] bool config_invalid_fault() const noexcept {
    return config_invalid_fault_;
  }
  [[nodiscard]] bool cell_profile_invalid_fault() const noexcept {
    return cell_profile_invalid_fault_;
  }
  [[nodiscard]] std::uint16_t error_count() const noexcept {
    return error_count_;
  }

 private:
  enum class State : std::uint8_t {
    kIdle,
    kBootLoad,
    kConfigCommit,
    kSelftestWrite,
    kSelftestRead,
  };

  struct TargetRange final {
    std::uint16_t offset{0U};
    std::uint16_t length{0U};
    bool valid{false};
  };

  static constexpr NvmRegion kSystemConfigRegion{
      kSystemConfigSlotAAddress, kSystemConfigRecordId, kSystemConfigSchema,
      2U};
  static constexpr std::uint32_t kSelftestMagic = 0x54534554U;
  static constexpr std::uint32_t kSelftestMarker = 0x5AA5C33CU;

  [[nodiscard]] static TargetRange target_range(std::uint16_t target) noexcept;
  [[nodiscard]] NvmServiceReply handle_config_read(
      const services::ServiceRequest& request) const noexcept;
  [[nodiscard]] NvmServiceReply handle_config_stage(
      const services::ServiceRequest& request) noexcept;
  [[nodiscard]] NvmServiceReply handle_config_commit(
      const services::ServiceRequest& request, bool write_allowed) noexcept;
  [[nodiscard]] NvmServiceReply handle_selftest(
      const services::ServiceRequest& request, bool write_allowed) noexcept;
  void complete_pending(std::uint8_t result) noexcept;
  void build_selftest_page(std::uint32_t sequence) noexcept;
  [[nodiscard]] bool validate_selftest_page() const noexcept;
  void count_error() noexcept;

  services::EepromDriver& eeprom_;
  NvmRecordStore store_;
  State state_{State::kIdle};
  SystemConfig active_config_{default_system_config()};
  SystemConfig stored_config_{default_system_config()};
  std::array<std::uint8_t, kSystemConfigPayloadSize> stored_payload_{};
  std::array<std::uint8_t, kSystemConfigPayloadSize> staged_payload_{};
  std::array<std::uint8_t, 64U> selftest_page_{};
  services::ServiceRequest pending_request_{};
  NvmServiceCompletion completion_{};
  std::uint32_t sequence_{0U};
  std::uint32_t selftest_sequence_{0U};
  std::uint16_t staged_target_{0U};
  bool initialized_{false};
  bool staging_active_{false};
  bool pending_service_{false};
  bool completion_available_{false};
  bool communication_fault_{false};
  bool record_crc_fault_{false};
  bool write_verify_fault_{false};
  bool selftest_fault_{false};
  bool schema_mismatch_fault_{false};
  bool config_invalid_fault_{false};
  bool cell_profile_invalid_fault_{false};
  std::uint16_t error_count_{0U};
};

}  // namespace packcontroller::app

#endif
