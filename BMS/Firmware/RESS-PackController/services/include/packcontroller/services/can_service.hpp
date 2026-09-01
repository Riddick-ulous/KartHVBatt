#ifndef PACKCONTROLLER_SERVICES_CAN_SERVICE_HPP
#define PACKCONTROLLER_SERVICES_CAN_SERVICE_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "pack_controller.h"
#include "packcontroller/platform/fdcan.h"

namespace packcontroller::services {

constexpr std::uint32_t kCanStaleTimeoutMs = 500U;

enum class CanSource : std::uint8_t {
  kNone = 0U,
  kMain = 1U,
  kBackup = 2U,
};

enum class AliveResult : std::uint8_t {
  kFirst = 0U,
  kAccepted = 1U,
  kGap = 2U,
  kDuplicate = 3U,
};

enum class CanBusState : std::uint8_t {
  kNotStarted = 0U,
  kErrorActive = 1U,
  kErrorPassive = 2U,
  kBusOff = 3U,
};

[[nodiscard]] CanBusState can_bus_state(
    const packcontroller_can_bus_diagnostics_t& diagnostics) noexcept;

class AliveMonitor final {
 public:
  AliveResult observe(std::uint8_t alive, std::uint32_t now_ms) noexcept;
  [[nodiscard]] bool fresh(std::uint32_t now_ms) const noexcept;
  [[nodiscard]] bool initialized() const noexcept { return initialized_; }
  [[nodiscard]] std::uint16_t dropped_frames() const noexcept {
    return dropped_frames_;
  }
  [[nodiscard]] std::uint16_t discontinuities() const noexcept {
    return discontinuities_;
  }
  [[nodiscard]] std::uint32_t progress_count() const noexcept {
    return progress_count_;
  }

 private:
  bool initialized_{false};
  std::uint8_t last_alive_{0U};
  std::uint32_t last_progress_ms_{0U};
  std::uint16_t dropped_frames_{0U};
  std::uint16_t discontinuities_{0U};
  std::uint32_t progress_count_{0U};
};

struct ControlSnapshot final {
  pack_controller_vcu_bms_control_t message{};
  std::uint32_t received_ms{0U};
  bool valid{false};
};

struct ServiceRequest final {
  pack_controller_bms_service_request_t message{};
  packcontroller_can_bus_t bus{PACKCONTROLLER_CAN_BUS_1};
  std::uint32_t received_ms{0U};
  bool valid{false};
};

struct ReceiveResult final {
  bool recognized{false};
  bool accepted{false};
  bool discontinuity{false};
  bool service_available{false};
  ServiceRequest service{};
};

struct StatusData final {
  bool critical_error_active{false};
  std::uint8_t hv_state{2U};
  std::uint8_t dcdc_state{0U};
  std::uint8_t developer_mode{0U};
  bool air_n{false};
  bool air_n_actual{false};
  bool precharge{false};
  bool precharge_actual{false};
  bool air_p{false};
  bool air_p_actual{false};
  bool dcdc{false};
  bool dcdc_actual{false};
  bool danger_voltage{true};
  bool por_state_n{false};
  bool sc_latched{false};
  std::uint8_t primary_fault_id{0U};
  std::uint8_t fault_severity{0U};
  std::uint32_t runtime_seconds{0U};
};

struct SafetyData final {
  std::uint64_t fault_active_low{0U};
  std::uint64_t fault_active_high{0U};
  std::uint64_t fault_latched_low{0U};
  std::uint64_t fault_latched_high{0U};
  std::uint32_t digital_raw_bitmap{0U};
  std::uint32_t digital_confirmed_bitmap{0U};
  std::uint16_t scheduler_loop_last_us{0U};
  std::uint16_t scheduler_loop_max_us{0U};
  bool scheduler_healthy{false};
  bool watchdog_feed_enabled{false};
  std::uint8_t can1_state{0U};
  std::uint8_t can2_state{0U};
  std::uint16_t can1_dropped_frames{0U};
  std::uint16_t can2_dropped_frames{0U};
  std::uint8_t can1_bus_off_count{0U};
  std::uint8_t can2_bus_off_count{0U};
  std::uint8_t adc_quality{0U};
  std::uint16_t adc_dma_error_count{0U};
  std::uint16_t eeprom_error_count{0U};
  std::uint8_t imd_hardware_type{0U};
};

struct AnalogData final {
  std::array<std::uint16_t, 12U> raw{};
  std::array<float, 12U> physical{};
  std::array<std::uint8_t, 12U> quality{};
  std::uint32_t sample_counter{0U};
  std::uint32_t timestamp_ms{0U};
  std::uint16_t dma_error_count{0U};
  std::uint16_t dropped_block_count{0U};
  bool coherent{false};
  std::uint8_t overall_quality{0U};
};

class CanService final {
 public:
  ReceiveResult receive(const packcontroller_can_frame_t& frame,
                        std::uint32_t now_ms) noexcept;
  void update_source(std::uint32_t now_ms) noexcept;

  [[nodiscard]] CanSource source() const noexcept { return source_; }
  [[nodiscard]] const ControlSnapshot* authoritative_control(
      std::uint32_t now_ms) const noexcept;
  [[nodiscard]] const AliveMonitor& control_alive(
      packcontroller_can_bus_t bus) const noexcept;
  [[nodiscard]] const AliveMonitor& service_alive(
      packcontroller_can_bus_t bus) const noexcept;
  [[nodiscard]] std::uint32_t source_switch_count() const noexcept {
    return source_switch_count_;
  }

  bool make_status_frame(packcontroller_can_bus_t bus,
                         const StatusData& status,
                         packcontroller_can_frame_t& frame) noexcept;
  bool make_safety_frame(packcontroller_can_bus_t bus,
                         const SafetyData& safety,
                         packcontroller_can_frame_t& frame) noexcept;
  bool make_analog_frame(packcontroller_can_bus_t bus,
                         const AnalogData& analog,
                         packcontroller_can_frame_t& frame) noexcept;
  bool make_service_response_frame(
      packcontroller_can_bus_t bus,
      const pack_controller_bms_service_response_t& response,
      packcontroller_can_frame_t& frame) noexcept;

 private:
  static constexpr std::size_t bus_index(packcontroller_can_bus_t bus) noexcept {
    return bus == PACKCONTROLLER_CAN_BUS_2 ? 1U : 0U;
  }
  void set_source(CanSource source) noexcept;

  std::array<AliveMonitor, 2U> control_alive_{};
  std::array<AliveMonitor, 2U> service_alive_{};
  std::array<ControlSnapshot, 2U> controls_{};
  std::array<std::uint8_t, 2U> status_alive_{};
  std::array<std::uint8_t, 2U> safety_alive_{};
  std::array<std::uint8_t, 2U> analog_alive_{};
  std::array<std::uint8_t, 2U> service_response_alive_{};
  CanSource source_{CanSource::kNone};
  bool main_recovery_tracking_{false};
  std::uint32_t main_recovery_since_ms_{0U};
  std::uint32_t main_recovery_start_progress_{0U};
  std::uint32_t source_switch_count_{0U};
};

}  // namespace packcontroller::services

#endif
