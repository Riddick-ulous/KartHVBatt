#include "packcontroller/app/runtime.h"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "pack_controller.h"
#include "packcontroller/app/build_profile.hpp"
#include "packcontroller/app/developer_session.hpp"
#include "packcontroller/app/measurements.hpp"
#include "packcontroller/app/nvm.hpp"
#include "packcontroller/core/faults.hpp"
#include "packcontroller/core/scheduler.hpp"
#include "packcontroller/platform/adc.h"
#include "packcontroller/platform/eeprom.h"
#include "packcontroller/platform/fdcan.h"
#include "packcontroller/platform/io.h"
#include "packcontroller/platform/runtime.h"
#include "packcontroller/services/can_service.hpp"

volatile std::uint32_t g_packcontroller_debug_stall_scheduler = 0U;
volatile packcontroller_runtime_diagnostics_t
    g_packcontroller_runtime_diagnostics = {};

namespace packcontroller::app {
namespace {

using core::FaultId;
using core::FaultManager;
using core::FaultResetPolicy;
using core::FaultSeverity;
using core::Scheduler;
using core::TaskConfig;
using core::TaskCallback;
using core::WatchdogHealthGate;

constexpr std::array<std::uint32_t, PACKCONTROLLER_RUNTIME_TASK_COUNT>
    kTaskPeriodsUs{1000U, 10000U, 20000U, 100000U, 1000000U};
constexpr std::array<bool, PACKCONTROLLER_RUNTIME_TASK_COUNT> kTaskCritical{
    true, true, true, false, false};
constexpr std::array<const char*, PACKCONTROLLER_RUNTIME_TASK_COUNT> kTaskNames{
    "1ms", "10ms", "20ms", "100ms", "1000ms"};
constexpr std::uint32_t kCanStartupGraceMs = 500U;
constexpr std::size_t kMaxCanFramesPerRelease = 32U;
constexpr std::uint8_t kHvNotReady = 2U;
constexpr std::uint8_t kHvDeveloperOutputTest = 10U;

std::uint32_t platform_time(void*) noexcept {
  return packcontroller_platform_time_us();
}

Scheduler scheduler{platform_time, nullptr};
WatchdogHealthGate watchdog_health_gate{};
FaultManager fault_manager{};
services::CanService can_service{};
MeasurementPipeline measurement_pipeline{{0U, 3000U}};
bool eeprom_start_read(void*, std::uint16_t address, std::uint8_t* data,
                       std::uint16_t length) noexcept {
  return packcontroller_platform_eeprom_start_read(address, data, length);
}
bool eeprom_start_page_write(void*, std::uint16_t address,
                             const std::uint8_t* data,
                             std::uint8_t length) noexcept {
  return packcontroller_platform_eeprom_start_page_write(address, data,
                                                         length);
}
bool eeprom_start_ack_poll(void*) noexcept {
  return packcontroller_platform_eeprom_start_ack_poll();
}
services::EepromIoStatus eeprom_status(void*) noexcept {
  return static_cast<services::EepromIoStatus>(
      packcontroller_platform_eeprom_status());
}
void eeprom_clear_result(void*) noexcept {
  packcontroller_platform_eeprom_clear_result();
}
void eeprom_set_write_protected(void*, bool enabled) noexcept {
  packcontroller_platform_eeprom_set_write_protected(enabled);
}
services::EepromDriver eeprom_driver{{
    nullptr, eeprom_start_read, eeprom_start_page_write,
    eeprom_start_ack_poll, eeprom_status, eeprom_clear_result,
    eeprom_set_write_protected}};
NvmManager nvm_manager{eeprom_driver};
SafetyInputConfirmation safety_input_confirmation{};
DeveloperSession developer_session{
    build_profile() == BuildProfile::kBoardBringup, kDeveloperBuildKey, true};
OutputArbiter output_arbiter{};
packcontroller_switch_outputs_t committed_outputs{};
std::uint32_t handled_watchdog_task_runs{0U};
std::uint32_t handled_heartbeat_task_runs{0U};
std::array<std::uint32_t, PACKCONTROLLER_RUNTIME_TASK_COUNT>
    observed_deadline_misses{};
bool initialized{false};
bool stored_hardfault{false};
bool overrun_in_report_window{false};
std::uint32_t loop_count{0U};
std::uint32_t idle_iterations{0U};
std::uint32_t last_poll_us{0U};
std::uint32_t last_loop_gap_us{0U};
std::uint32_t max_loop_gap_us{0U};
bool counter_discontinuity_seen{false};
bool nvm_config_applied{false};
std::array<std::uint16_t, 2U> observed_can_rx_dropped{};
std::array<std::uint16_t, 2U> observed_can_tx_dropped{};
std::uint32_t monotonic_ms{0U};
std::uint32_t monotonic_last_us{0U};
std::uint16_t monotonic_remainder_us{0U};

void update_can_faults(std::uint32_t now_ms) noexcept;
void task_1ms(void*) noexcept;
void task_10ms(void*) noexcept;
void task_20ms(void*) noexcept;
void task_100ms(void*) noexcept;
void task_1000ms(void*) noexcept;

constexpr std::array<TaskCallback, PACKCONTROLLER_RUNTIME_TASK_COUNT>
    kTaskCallbacks{task_1ms, task_10ms, task_20ms, task_100ms, task_1000ms};

std::uint32_t runtime_now_ms() noexcept {
  const std::uint32_t raw_us = packcontroller_platform_time_us();
  const std::uint32_t delta_us = raw_us - monotonic_last_us;
  monotonic_last_us = raw_us;
  monotonic_ms += delta_us / 1000U;
  const std::uint32_t remainder =
      static_cast<std::uint32_t>(monotonic_remainder_us) +
      (delta_us % 1000U);
  monotonic_ms += remainder / 1000U;
  monotonic_remainder_us = static_cast<std::uint16_t>(remainder % 1000U);
  return monotonic_ms;
}

std::uint8_t primary_fault_id() noexcept {
  FaultSeverity highest = FaultSeverity::kNone;
  std::uint8_t primary = 0U;
  for (std::uint8_t raw = 1U;
       raw <= static_cast<std::uint8_t>(FaultId::kLastDefined); ++raw) {
    const auto id = static_cast<FaultId>(raw);
    const auto& fault = fault_manager.get(id);
    if ((fault.active || fault.latched) && (fault.severity > highest)) {
      highest = fault.severity;
      primary = raw;
    }
  }
  return primary;
}

FaultSeverity primary_fault_severity() noexcept {
  const std::uint8_t primary = primary_fault_id();
  return primary == 0U
             ? FaultSeverity::kNone
             : fault_manager.get(static_cast<FaultId>(primary)).severity;
}

bool service_bus_is_authoritative(packcontroller_can_bus_t bus) noexcept {
  return (can_service.source() == services::CanSource::kMain &&
          bus == PACKCONTROLLER_CAN_BUS_1) ||
         (can_service.source() == services::CanSource::kBackup &&
          bus == PACKCONTROLLER_CAN_BUS_2);
}

void send_service_response(const services::ServiceRequest& request,
                           std::uint8_t result,
                           std::uint32_t value0 = 0U,
                           std::uint32_t value1 = 0U,
                           std::uint32_t value2 = 0U,
                           std::uint32_t nvm_sequence = 0U) noexcept {
  pack_controller_bms_service_response_t response{};
  response.service_response_command = request.message.service_command;
  response.service_response_protocol_version =
      request.message.service_request_protocol_version;
  response.service_response_sequence = request.message.service_sequence;
  response.service_result = result;
  response.service_response_target = request.message.service_target;
  response.service_response_value0 = value0;
  response.service_response_value1 = value1;
  response.service_response_value2 = value2;
  response.service_nvm_sequence = nvm_sequence;
  packcontroller_can_frame_t frame{};
  if (can_service.make_service_response_frame(request.bus, response, frame)) {
    frame.high_priority = true;
    (void)packcontroller_platform_can_transmit(&frame);
  }
}

void process_service_request(const services::ServiceRequest& request) noexcept {
  if (!service_bus_is_authoritative(request.bus)) {
    send_service_response(
        request, static_cast<std::uint8_t>(ServiceResult::kDeniedState));
    return;
  }
  const bool all_low = !committed_outputs.air_n &&
                       !committed_outputs.precharge &&
                       !committed_outputs.air_p && !committed_outputs.dcdc;
  const DeveloperContext context{true, all_low};
  const DeveloperResult result = developer_session.handle(request, context);
  if (result.handled) {
    send_service_response(request, static_cast<std::uint8_t>(result.result));
    return;
  }
  const bool write_allowed =
      all_low && safety_input_confirmation.all_power_paths_open() &&
      (developer_session.mode() == DeveloperMode::kDisabled);
  const NvmServiceReply nvm = nvm_manager.handle(request, write_allowed);
  if (nvm.handled) {
    send_service_response(request, nvm.result, nvm.value0, nvm.value1,
                          nvm.value2, nvm.nvm_sequence);
    return;
  }
  send_service_response(
      request, static_cast<std::uint8_t>(ServiceResult::kInvalidTarget));
}

void update_nvm_faults(std::uint32_t now_ms) noexcept {
  fault_manager.update(FaultId::kEepromCommunication, FaultSeverity::kWarning,
                       FaultResetPolicy::kAutoClear,
                       nvm_manager.communication_fault(), now_ms);
  fault_manager.update(FaultId::kEepromRecordCrc, FaultSeverity::kWarning,
                       FaultResetPolicy::kAutoClear,
                       nvm_manager.record_crc_fault(), now_ms);
  fault_manager.update(FaultId::kEepromWriteVerify, FaultSeverity::kWarning,
                       FaultResetPolicy::kCanResettable,
                       nvm_manager.write_verify_fault(), now_ms);
  fault_manager.update(FaultId::kEepromSelftestFailed,
                       FaultSeverity::kWarning,
                       FaultResetPolicy::kCanResettable,
                       nvm_manager.selftest_fault(), now_ms);
  fault_manager.update(FaultId::kNvmSchemaMismatch, FaultSeverity::kWarning,
                       FaultResetPolicy::kPowerCycle,
                       nvm_manager.schema_mismatch_fault(), now_ms);
  fault_manager.update(FaultId::kConfigInvalid, FaultSeverity::kWarning,
                       FaultResetPolicy::kPowerCycle,
                       nvm_manager.config_invalid_fault(), now_ms);
  fault_manager.update(FaultId::kCellProfileInvalid, FaultSeverity::kWarning,
                       FaultResetPolicy::kPowerCycle,
                       nvm_manager.cell_profile_invalid_fault(), now_ms);
  fault_manager.update(
      FaultId::kImdTypeUnset, FaultSeverity::kWarning,
      FaultResetPolicy::kPowerCycle,
      nvm_manager.initialized() &&
          (nvm_manager.active_config().imd.hardware_type == 0U),
      now_ms);
}

void apply_nvm_config_if_ready() noexcept {
  if (!nvm_manager.initialized() || nvm_config_applied) {
    return;
  }
  const auto& analog = nvm_manager.active_config().analog;
  MeasurementCalibration calibration{};
  calibration.vrefint_calibration_raw =
      packcontroller_platform_adc_vref_calibration_raw();
  calibration.vrefint_calibration_mv =
      packcontroller_platform_adc_vref_calibration_mv();
  calibration.hv_gain = static_cast<float>(analog.hv_gain_milli) / 1000.0F;
  calibration.vbatt_gain =
      static_cast<float>(analog.vbatt_gain_millionths) / 1000000.0F;
  for (std::size_t index = 0U; index < analog.hv_offset_uv.size(); ++index) {
    calibration.hv_offset_v[index] =
        static_cast<float>(analog.hv_offset_uv[index]) / 1000000.0F;
  }
  calibration.vbatt_offset_v =
      static_cast<float>(analog.vbatt_offset_uv) / 1000000.0F;
  measurement_pipeline.set_calibration(calibration);
  nvm_config_applied = true;
}

void update_can_faults(std::uint32_t now_ms) noexcept {
  const auto can1 = packcontroller_platform_can_diagnostics(
      PACKCONTROLLER_CAN_BUS_1);
  const auto can2 = packcontroller_platform_can_diagnostics(
      PACKCONTROLLER_CAN_BUS_2);
  const bool startup_grace_complete = now_ms > kCanStartupGraceMs;
  const bool can1_stale = startup_grace_complete &&
      !can_service.control_alive(PACKCONTROLLER_CAN_BUS_1).fresh(now_ms);
  const bool can2_stale = startup_grace_complete &&
      !can_service.control_alive(PACKCONTROLLER_CAN_BUS_2).fresh(now_ms);
  const bool rx_overflow =
      (can1.rx_dropped != observed_can_rx_dropped[0U]) ||
      (can2.rx_dropped != observed_can_rx_dropped[1U]);
  const bool tx_overflow =
      (can1.tx_dropped != observed_can_tx_dropped[0U]) ||
      (can2.tx_dropped != observed_can_tx_dropped[1U]);
  observed_can_rx_dropped = {can1.rx_dropped, can2.rx_dropped};
  observed_can_tx_dropped = {can1.tx_dropped, can2.tx_dropped};

  fault_manager.update(FaultId::kCan1BusOff, FaultSeverity::kWarning,
                       FaultResetPolicy::kAutoClear, can1.bus_off, now_ms);
  fault_manager.update(FaultId::kCan2BusOff, FaultSeverity::kWarning,
                       FaultResetPolicy::kAutoClear, can2.bus_off, now_ms);
  fault_manager.update(FaultId::kCan1CommandStale, FaultSeverity::kWarning,
                       FaultResetPolicy::kAutoClear, can1_stale, now_ms);
  fault_manager.update(FaultId::kCan2CommandStale, FaultSeverity::kWarning,
                       FaultResetPolicy::kAutoClear, can2_stale, now_ms);
  fault_manager.update(FaultId::kCanCommandLoss,
                       FaultSeverity::kControlledCritical,
                       FaultResetPolicy::kCanResettable,
                       can1_stale && can2_stale, now_ms);
  fault_manager.update(FaultId::kCanCounterDiscontinuity,
                       FaultSeverity::kWarning,
                       FaultResetPolicy::kAutoClear,
                       counter_discontinuity_seen, now_ms);
  fault_manager.update(FaultId::kCanRxOverflow, FaultSeverity::kWarning,
                       FaultResetPolicy::kAutoClear,
                       rx_overflow, now_ms);
  fault_manager.update(FaultId::kCanTxOverflow, FaultSeverity::kWarning,
                       FaultResetPolicy::kAutoClear,
                       tx_overflow, now_ms);
  counter_discontinuity_seen = false;
}

void task_1ms(void*) noexcept {
  const std::uint32_t now_ms = runtime_now_ms();
  nvm_manager.service();
  apply_nvm_config_if_ready();
  NvmServiceCompletion completion{};
  if (nvm_manager.take_completion(completion)) {
    send_service_response(completion.request, completion.reply.result,
                          completion.reply.value0, completion.reply.value1,
                          completion.reply.value2,
                          completion.reply.nvm_sequence);
  }
  update_nvm_faults(now_ms);
  packcontroller_platform_can_service();
  packcontroller_can_frame_t frame{};
  for (std::size_t count = 0U;
       count < kMaxCanFramesPerRelease &&
       packcontroller_platform_can_receive(&frame);
       ++count) {
    const auto result = can_service.receive(frame, now_ms);
    counter_discontinuity_seen =
        counter_discontinuity_seen || result.discontinuity;
    can_service.update_source(now_ms);
    if (result.service_available) {
      process_service_request(result.service);
    }
  }
  can_service.update_source(now_ms);
  update_can_faults(now_ms);

  safety_input_confirmation.update(
      packcontroller_platform_read_safety_inputs(), now_ms);
  const DeveloperGates gates = safety_input_confirmation.gates(
      watchdog_health_gate.healthy(),
      !fault_manager.get(FaultId::kClockFailure).active);
  developer_session.update(now_ms, gates);
  fault_manager.update(FaultId::kDeveloperSessionLoss,
                       FaultSeverity::kControlledCritical,
                       FaultResetPolicy::kAutoClear,
                       developer_session.commissioning_session_lost(), now_ms);
  const std::uint8_t mask = developer_session.output_mask(now_ms, gates);
  committed_outputs = output_arbiter.resolve(
      developer_session.mode(), mask, gates,
      fault_manager.critical_error_active());
  packcontroller_platform_commit_switch_outputs(committed_outputs);
}

void update_adc_faults(std::uint32_t now_ms) noexcept {
  fault_manager.update(FaultId::kAdcPipelineInvalid, FaultSeverity::kWarning,
                       FaultResetPolicy::kCanResettable,
                       measurement_pipeline.pipeline_invalid(now_ms), now_ms);
  fault_manager.update(FaultId::kAdcReferenceInvalid, FaultSeverity::kWarning,
                       FaultResetPolicy::kCanResettable,
                       measurement_pipeline.reference_invalid(now_ms), now_ms);
  fault_manager.update(
      FaultId::kVaccuMeasInvalid, FaultSeverity::kWarning,
      FaultResetPolicy::kCanResettable,
      measurement_pipeline.channel_invalid(PACKCONTROLLER_ADC_VACCU, now_ms),
      now_ms);
  fault_manager.update(
      FaultId::kVvehiMeasInvalid, FaultSeverity::kWarning,
      FaultResetPolicy::kCanResettable,
      measurement_pipeline.channel_invalid(PACKCONTROLLER_ADC_VVEHI, now_ms),
      now_ms);
  fault_manager.update(
      FaultId::kVdcdcMeasInvalid, FaultSeverity::kWarning,
      FaultResetPolicy::kCanResettable,
      measurement_pipeline.channel_invalid(PACKCONTROLLER_ADC_VDCDC, now_ms),
      now_ms);
  fault_manager.update(
      FaultId::kVbattMeasInvalid, FaultSeverity::kWarning,
      FaultResetPolicy::kAutoClear,
      measurement_pipeline.channel_invalid(PACKCONTROLLER_ADC_VBATT, now_ms),
      now_ms);
}

void task_10ms(void*) noexcept {
  packcontroller_adc_block_t block{};
  while (packcontroller_platform_adc_receive(&block)) {
    measurement_pipeline.process(
        block, packcontroller_platform_adc_diagnostics());
  }
  update_adc_faults(runtime_now_ms());
}

void task_20ms(void*) noexcept {
  const std::uint32_t now_ms = runtime_now_ms();
  const auto analog_snapshot = measurement_pipeline.snapshot(now_ms);
  const DeveloperGates confirmed_inputs =
      safety_input_confirmation.gates(true, true);
  services::StatusData status{};
  status.critical_error_active = fault_manager.critical_error_active();
  status.hv_state = developer_session.mode() == DeveloperMode::kOutputTest
                        ? kHvDeveloperOutputTest
                        : kHvNotReady;
  status.developer_mode =
      static_cast<std::uint8_t>(developer_session.mode());
  const auto actuals = safety_input_confirmation.confirmed_actuals();
  status.air_n = committed_outputs.air_n;
  status.air_n_actual = actuals.air_n;
  status.precharge = committed_outputs.precharge;
  status.precharge_actual = actuals.precharge;
  status.air_p = committed_outputs.air_p;
  status.air_p_actual = actuals.air_p;
  status.dcdc = committed_outputs.dcdc;
  status.dcdc_actual = actuals.dcdc;
  status.danger_voltage = !confirmed_inputs.danger_voltage_clear;
  status.por_state_n = confirmed_inputs.por_valid;
  status.sc_latched = !confirmed_inputs.sc_not_latched;
  status.primary_fault_id = primary_fault_id();
  status.fault_severity =
      static_cast<std::uint8_t>(primary_fault_severity());
  status.runtime_seconds = now_ms / 1000U;

  services::AnalogData analog{};
  analog.sample_counter = analog_snapshot.sample_counter;
  analog.timestamp_ms = analog_snapshot.timestamp_ms;
  analog.dma_error_count = analog_snapshot.dma_error_count;
  analog.dropped_block_count = analog_snapshot.dropped_block_count;
  analog.coherent = analog_snapshot.coherent;
  analog.overall_quality =
      static_cast<std::uint8_t>(analog_snapshot.overall_quality);
  for (std::size_t channel = 0U; channel < analog_snapshot.channels.size();
       ++channel) {
    analog.raw[channel] = analog_snapshot.channels[channel].raw_mean;
    analog.physical[channel] = analog_snapshot.channels[channel].physical;
    analog.quality[channel] =
        static_cast<std::uint8_t>(analog_snapshot.channels[channel].quality);
  }

  for (const auto bus : {PACKCONTROLLER_CAN_BUS_1,
                         PACKCONTROLLER_CAN_BUS_2}) {
    packcontroller_can_frame_t tx{};
    if (can_service.make_status_frame(bus, status, tx)) {
      (void)packcontroller_platform_can_transmit(&tx);
    }
    if (can_service.make_analog_frame(bus, analog, tx)) {
      (void)packcontroller_platform_can_transmit(&tx);
    }
  }
}

std::uint16_t narrow_u16(std::uint32_t value) noexcept {
  return static_cast<std::uint16_t>(
      std::min(value,
               static_cast<std::uint32_t>(
                   std::numeric_limits<std::uint16_t>::max())));
}

void task_100ms(void*) noexcept {
  const auto analog_snapshot = measurement_pipeline.snapshot(runtime_now_ms());
  const auto active = fault_manager.active_bitmap();
  const auto latched = fault_manager.latched_bitmap();
  const auto can1 = packcontroller_platform_can_diagnostics(
      PACKCONTROLLER_CAN_BUS_1);
  const auto can2 = packcontroller_platform_can_diagnostics(
      PACKCONTROLLER_CAN_BUS_2);
  services::SafetyData safety{};
  safety.fault_active_low = active.low;
  safety.fault_active_high = active.high;
  safety.fault_latched_low = latched.low;
  safety.fault_latched_high = latched.high;
  safety.digital_raw_bitmap = safety_input_confirmation.raw_bitmap();
  safety.digital_confirmed_bitmap =
      safety_input_confirmation.confirmed_bitmap();
  safety.scheduler_loop_last_us = narrow_u16(last_loop_gap_us);
  safety.scheduler_loop_max_us = narrow_u16(max_loop_gap_us);
  safety.scheduler_healthy = watchdog_health_gate.healthy();
  safety.watchdog_feed_enabled =
      watchdog_health_gate.healthy() && !stored_hardfault;
  safety.can1_state = static_cast<std::uint8_t>(services::can_bus_state(can1));
  safety.can2_state = static_cast<std::uint8_t>(services::can_bus_state(can2));
  safety.can1_dropped_frames = narrow_u16(
      static_cast<std::uint32_t>(can1.rx_dropped) +
      can_service.control_alive(
          PACKCONTROLLER_CAN_BUS_1).dropped_frames() +
      can_service.service_alive(
          PACKCONTROLLER_CAN_BUS_1).dropped_frames());
  safety.can2_dropped_frames = narrow_u16(
      static_cast<std::uint32_t>(can2.rx_dropped) +
      can_service.control_alive(
          PACKCONTROLLER_CAN_BUS_2).dropped_frames() +
      can_service.service_alive(
          PACKCONTROLLER_CAN_BUS_2).dropped_frames());
  safety.can1_bus_off_count = can1.bus_off_count;
  safety.can2_bus_off_count = can2.bus_off_count;
  safety.adc_quality =
      static_cast<std::uint8_t>(analog_snapshot.overall_quality);
  safety.adc_dma_error_count = analog_snapshot.dma_error_count;
  safety.eeprom_error_count = nvm_manager.error_count();
  safety.imd_hardware_type =
      nvm_manager.active_config().imd.hardware_type;
  for (const auto bus : {PACKCONTROLLER_CAN_BUS_1,
                         PACKCONTROLLER_CAN_BUS_2}) {
    packcontroller_can_frame_t tx{};
    if (can_service.make_safety_frame(bus, safety, tx)) {
      (void)packcontroller_platform_can_transmit(&tx);
    }
  }
}

void task_1000ms(void*) noexcept {}

std::uint32_t total_task_runs() noexcept {
  std::uint32_t total = 0U;
  for (std::size_t index = 0U; index < scheduler.task_count(); ++index) {
    total += scheduler.metrics(index).run_count;
  }
  return total;
}

void update_diagnostics() noexcept {
  for (std::size_t index = 0U; index < scheduler.task_count(); ++index) {
    const auto& metrics = scheduler.metrics(index);
    g_packcontroller_runtime_diagnostics.run_count[index] = metrics.run_count;
    g_packcontroller_runtime_diagnostics.last_runtime_us[index] =
        metrics.last_runtime_us;
    g_packcontroller_runtime_diagnostics.max_runtime_us[index] =
        metrics.max_runtime_us;
    g_packcontroller_runtime_diagnostics.max_start_lateness_us[index] =
        metrics.max_start_lateness_us;
    g_packcontroller_runtime_diagnostics.deadline_misses[index] =
        metrics.deadline_misses;
    g_packcontroller_runtime_diagnostics.consecutive_deadline_misses[index] =
        metrics.consecutive_deadline_misses;
    g_packcontroller_runtime_diagnostics.overrun_limit_violations[index] =
        metrics.overrun_limit_violations;
    g_packcontroller_runtime_diagnostics.skipped_releases[index] =
        metrics.skipped_releases;
  }

  const auto active = fault_manager.active_bitmap();
  const auto latched = fault_manager.latched_bitmap();
  g_packcontroller_runtime_diagnostics.fault_active_low = active.low;
  g_packcontroller_runtime_diagnostics.fault_active_high = active.high;
  g_packcontroller_runtime_diagnostics.fault_latched_low = latched.low;
  g_packcontroller_runtime_diagnostics.fault_latched_high = latched.high;
  g_packcontroller_runtime_diagnostics.scheduler_healthy =
      watchdog_health_gate.healthy() ? 1U : 0U;
  g_packcontroller_runtime_diagnostics.stored_hardfault =
      stored_hardfault ? 1U : 0U;
  g_packcontroller_runtime_diagnostics.loop_count = loop_count;
  g_packcontroller_runtime_diagnostics.idle_iterations = idle_iterations;
  g_packcontroller_runtime_diagnostics.last_loop_gap_us = last_loop_gap_us;
  g_packcontroller_runtime_diagnostics.max_loop_gap_us = max_loop_gap_us;

  const auto analog_snapshot = measurement_pipeline.snapshot(monotonic_ms);
  for (std::size_t channel = 0U; channel < analog_snapshot.channels.size();
       ++channel) {
    g_packcontroller_runtime_diagnostics.adc_raw[channel] =
        analog_snapshot.channels[channel].raw_mean;
    g_packcontroller_runtime_diagnostics.adc_physical[channel] =
        analog_snapshot.channels[channel].physical;
    g_packcontroller_runtime_diagnostics.adc_quality[channel] =
        static_cast<std::uint8_t>(analog_snapshot.channels[channel].quality);
  }
  g_packcontroller_runtime_diagnostics.adc_sample_counter =
      analog_snapshot.sample_counter;
  g_packcontroller_runtime_diagnostics.adc_timestamp_ms =
      analog_snapshot.timestamp_ms;
  g_packcontroller_runtime_diagnostics.runtime_seconds = monotonic_ms / 1000U;
  g_packcontroller_runtime_diagnostics.adc_dma_error_count =
      analog_snapshot.dma_error_count;
  g_packcontroller_runtime_diagnostics.adc_dropped_block_count =
      analog_snapshot.dropped_block_count;
  g_packcontroller_runtime_diagnostics.eeprom_error_count =
      nvm_manager.error_count();
  g_packcontroller_runtime_diagnostics.nvm_sequence = nvm_manager.sequence();
  g_packcontroller_runtime_diagnostics.nvm_initialized =
      nvm_manager.initialized() ? 1U : 0U;
  g_packcontroller_runtime_diagnostics.adc_overall_quality =
      static_cast<std::uint8_t>(analog_snapshot.overall_quality);
  g_packcontroller_runtime_diagnostics.adc_coherent =
      analog_snapshot.coherent ? 1U : 0U;
}

void process_deadline_faults(std::uint32_t now_ms) noexcept {
  bool new_overrun = false;
  for (std::size_t index = 0U; index < scheduler.task_count(); ++index) {
    const auto misses = scheduler.metrics(index).deadline_misses;
    if (misses != observed_deadline_misses[index]) {
      new_overrun = true;
      observed_deadline_misses[index] = misses;
    }
  }

  overrun_in_report_window = overrun_in_report_window || new_overrun;

  fault_manager.update(FaultId::kSchedulerTaskOverrun,
                       FaultSeverity::kWarning,
                       FaultResetPolicy::kAutoClear,
                       overrun_in_report_window, now_ms);
}

void process_watchdog_release(std::uint32_t now_us) noexcept {
  const auto watchdog_runs = scheduler.metrics(3U).run_count;
  if (watchdog_runs == handled_watchdog_task_runs) {
    return;
  }
  handled_watchdog_task_runs = watchdog_runs;

  const bool scheduler_allows_edge =
      watchdog_health_gate.allow_edge(scheduler, now_us);
  const bool allow_edge = !stored_hardfault && scheduler_allows_edge;
  fault_manager.update(FaultId::kSchedulerHealthLoss,
                       FaultSeverity::kStmHardfault,
                       FaultResetPolicy::kPowerCycle,
                       !scheduler_allows_edge,
                       now_us / 1000U);
  if (allow_edge) {
    packcontroller_platform_toggle_wdbeat();
    ++g_packcontroller_runtime_diagnostics.watchdog_edges;
  } else {
    packcontroller_platform_set_error_led(true);
  }
}

void process_heartbeat_release(std::uint32_t now_ms) noexcept {
  const auto heartbeat_runs = scheduler.metrics(4U).run_count;
  if (heartbeat_runs == handled_heartbeat_task_runs) {
    return;
  }
  handled_heartbeat_task_runs = heartbeat_runs;
  packcontroller_platform_toggle_heartbeat();
  ++g_packcontroller_runtime_diagnostics.heartbeat_edges;
  update_diagnostics();
  overrun_in_report_window = false;
  fault_manager.update(FaultId::kSchedulerTaskOverrun,
                       FaultSeverity::kWarning,
                       FaultResetPolicy::kAutoClear, false, now_ms);
  scheduler.reset_report_window();
}

}  // namespace
}  // namespace packcontroller::app

extern "C" void packcontroller_runtime_init(void) {
  using namespace packcontroller::app;
  if (initialized) {
    return;
  }

  for (std::size_t index = 0U; index < kTaskPeriodsUs.size(); ++index) {
    const TaskConfig config{kTaskNames[index], kTaskPeriodsUs[index],
                            kTaskCritical[index], kTaskCallbacks[index],
                            nullptr};
    if (!scheduler.add_task(config)) {
      fault_manager.update(FaultId::kPlatformInitFailed,
                           FaultSeverity::kWarning,
                           FaultResetPolicy::kPowerCycle, true, 0U);
      packcontroller_platform_set_error_led(true);
      return;
    }
  }

  stored_hardfault = packcontroller_platform_stored_hardfault_valid();
  if (stored_hardfault) {
    fault_manager.update(FaultId::kStmHardfault,
                         FaultSeverity::kStmHardfault,
                         FaultResetPolicy::kPowerCycle, true, 0U);
    packcontroller_platform_set_error_led(true);
  }
  measurement_pipeline = MeasurementPipeline{{
      packcontroller_platform_adc_vref_calibration_raw(),
      packcontroller_platform_adc_vref_calibration_mv()}};
  if (!nvm_manager.start_boot_load()) {
    fault_manager.update(FaultId::kEepromCommunication,
                         FaultSeverity::kWarning,
                         FaultResetPolicy::kAutoClear, true, 0U);
  }
  last_poll_us = packcontroller_platform_time_us();
  monotonic_last_us = last_poll_us;
  scheduler.start(last_poll_us);
  initialized = true;
  update_diagnostics();
}

extern "C" void packcontroller_runtime_poll(void) {
  using namespace packcontroller::app;
  if (!initialized || (g_packcontroller_debug_stall_scheduler != 0U)) {
    return;
  }

  const std::uint32_t poll_start_us = packcontroller_platform_time_us();
  const std::uint32_t loop_gap_us = poll_start_us - last_poll_us;
  last_poll_us = poll_start_us;
  last_loop_gap_us = loop_gap_us;
  if (loop_gap_us > max_loop_gap_us) {
    max_loop_gap_us = loop_gap_us;
  }
  ++loop_count;

  const std::uint32_t runs_before = total_task_runs();
  scheduler.run_due_tasks();
  if (total_task_runs() == runs_before) {
    ++idle_iterations;
  }
  const std::uint32_t now_us = packcontroller_platform_time_us();
  const std::uint32_t now_ms = runtime_now_ms();
  process_deadline_faults(now_ms);
  process_watchdog_release(now_us);
  process_heartbeat_release(now_ms);
}
