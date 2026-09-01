#include <gtest/gtest.h>

#include <cstdint>

#include "pack_controller.h"
#include "packcontroller/services/can_service.hpp"

namespace packcontroller::services {
namespace {

packcontroller_can_frame_t control_frame(packcontroller_can_bus_t bus,
                                         std::uint8_t alive,
                                         bool request = false) {
  pack_controller_vcu_bms_control_t message{};
  message.control_alive_counter = alive;
  message.hv_on_request = request ? 1U : 0U;
  message.control_request_sequence = static_cast<std::uint16_t>(100U + alive);
  packcontroller_can_frame_t frame{};
  frame.identifier = PACK_CONTROLLER_VCU_BMS_CONTROL_FRAME_ID;
  frame.length = PACK_CONTROLLER_VCU_BMS_CONTROL_LENGTH;
  frame.bus = static_cast<std::uint8_t>(bus);
  frame.is_fd = true;
  frame.bit_rate_switch = true;
  EXPECT_EQ(pack_controller_vcu_bms_control_pack(frame.data, &message,
                                                 frame.length),
            frame.length);
  return frame;
}

TEST(DbcCodec, ControlRoundTripPreservesContractFields) {
  const auto frame = control_frame(PACKCONTROLLER_CAN_BUS_1, 15U, true);
  pack_controller_vcu_bms_control_t decoded{};
  ASSERT_EQ(pack_controller_vcu_bms_control_unpack(
                &decoded, frame.data, frame.length),
            0);
  EXPECT_EQ(decoded.control_alive_counter, 15U);
  EXPECT_EQ(decoded.hv_on_request, 1U);
  EXPECT_EQ(decoded.control_request_sequence, 115U);
}

TEST(DbcCodec, ExposesNormativeCanalyzerChoiceValues) {
  EXPECT_EQ(PACK_CONTROLLER_BMS_STATUS_HV_STATE_HV_NOT_READY_CHOICE, 2U);
  EXPECT_EQ(
      PACK_CONTROLLER_BMS_STATUS_PRIMARY_FAULT_ID_CAN_COMMAND_LOSS_CHOICE,
      20U);
  EXPECT_EQ(PACK_CONTROLLER_BMS_STATUS_POR_STATE_N_POR_STATE_VALID_CHOICE,
            1U);
  EXPECT_EQ(
      PACK_CONTROLLER_BMS_SAFETY_DIAG_SCHEDULER_HEALTHY_SCHEDULER_HEALTHY_CHOICE,
      1U);
  EXPECT_EQ(
      PACK_CONTROLLER_BMS_SAFETY_DIAG_CAN1_STATE_CAN_ERROR_ACTIVE_CHOICE,
      1U);
  EXPECT_EQ(static_cast<std::uint8_t>(CanBusState::kNotStarted),
            PACK_CONTROLLER_BMS_SAFETY_DIAG_CAN1_STATE_CAN_NOT_STARTED_CHOICE);
  EXPECT_EQ(static_cast<std::uint8_t>(CanBusState::kErrorActive),
            PACK_CONTROLLER_BMS_SAFETY_DIAG_CAN1_STATE_CAN_ERROR_ACTIVE_CHOICE);
  EXPECT_EQ(static_cast<std::uint8_t>(CanBusState::kErrorPassive),
            PACK_CONTROLLER_BMS_SAFETY_DIAG_CAN1_STATE_CAN_ERROR_PASSIVE_CHOICE);
  EXPECT_EQ(static_cast<std::uint8_t>(CanBusState::kBusOff),
            PACK_CONTROLLER_BMS_SAFETY_DIAG_CAN1_STATE_CAN_BUS_OFF_CHOICE);
  EXPECT_EQ(
      PACK_CONTROLLER_VCU_BMS_CONTROL_CONTROL_PROTOCOL_VERSION_CONTROL_PROTOCOL_V0_CHOICE,
      0U);
  EXPECT_EQ(
      PACK_CONTROLLER_BMS_ANALOG_ANALOG_QUALITY_SIGNAL_INVALID_CHOICE, 0U);
  EXPECT_EQ(PACK_CONTROLLER_BMS_ANALOG_ANALOG_QUALITY_SIGNAL_VALID_CHOICE,
            1U);
  EXPECT_EQ(PACK_CONTROLLER_BMS_ANALOG_ANALOG_QUALITY_SIGNAL_STALE_CHOICE,
            2U);
  EXPECT_EQ(PACK_CONTROLLER_BMS_ANALOG_ANALOG_QUALITY_SIGNAL_FAULT_CHOICE,
            3U);
}

TEST(AliveMonitor, AcceptsWrapDetectsGapAndDropsDuplicate) {
  AliveMonitor alive{};
  EXPECT_EQ(alive.observe(15U, 0U), AliveResult::kFirst);
  EXPECT_EQ(alive.observe(0U, 100U), AliveResult::kAccepted);
  EXPECT_EQ(alive.observe(3U, 200U), AliveResult::kGap);
  EXPECT_EQ(alive.dropped_frames(), 2U);
  EXPECT_EQ(alive.observe(3U, 300U), AliveResult::kDuplicate);
  EXPECT_EQ(alive.dropped_frames(), 3U);
  EXPECT_EQ(alive.discontinuities(), 2U);
  EXPECT_TRUE(alive.fresh(700U));
  EXPECT_FALSE(alive.fresh(701U));
}

TEST(CanBusState, ReportsControllerErrorStateWithSafePrecedence) {
  packcontroller_can_bus_diagnostics_t diagnostics{};
  EXPECT_EQ(can_bus_state(diagnostics), CanBusState::kNotStarted);

  diagnostics.started = true;
  EXPECT_EQ(can_bus_state(diagnostics), CanBusState::kErrorActive);

  diagnostics.error_passive = true;
  EXPECT_EQ(can_bus_state(diagnostics), CanBusState::kErrorPassive);

  diagnostics.bus_off = true;
  EXPECT_EQ(can_bus_state(diagnostics), CanBusState::kBusOff);
}

TEST(CanService, DuplicateDoesNotReplaceLastControlSnapshot) {
  CanService service{};
  auto first = control_frame(PACKCONTROLLER_CAN_BUS_1, 1U, false);
  auto duplicate = control_frame(PACKCONTROLLER_CAN_BUS_1, 1U, true);
  EXPECT_TRUE(service.receive(first, 10U).accepted);
  const auto result = service.receive(duplicate, 20U);
  EXPECT_FALSE(result.accepted);
  EXPECT_TRUE(result.discontinuity);
  service.update_source(20U);
  const auto* snapshot = service.authoritative_control(20U);
  ASSERT_NE(snapshot, nullptr);
  EXPECT_EQ(snapshot->message.hv_on_request, 0U);
}

TEST(CanService, RejectsClassicOrWrongLengthControlFrames) {
  CanService service{};
  auto frame = control_frame(PACKCONTROLLER_CAN_BUS_1, 0U);
  frame.is_fd = false;
  EXPECT_FALSE(service.receive(frame, 0U).accepted);
  frame.is_fd = true;
  frame.length = 12U;
  EXPECT_FALSE(service.receive(frame, 0U).accepted);
}

TEST(CanService, FailsOverAndRequiresStableMainBeforeReturning) {
  CanService service{};
  EXPECT_TRUE(service.receive(
      control_frame(PACKCONTROLLER_CAN_BUS_1, 0U, true), 0U).accepted);
  EXPECT_TRUE(service.receive(
      control_frame(PACKCONTROLLER_CAN_BUS_2, 0U, false), 0U).accepted);
  service.update_source(0U);
  EXPECT_EQ(service.source(), CanSource::kMain);
  ASSERT_NE(service.authoritative_control(0U), nullptr);
  EXPECT_EQ(service.authoritative_control(0U)->message.hv_on_request, 1U);

  EXPECT_TRUE(service.receive(
      control_frame(PACKCONTROLLER_CAN_BUS_2, 1U, false), 500U).accepted);
  service.update_source(501U);
  EXPECT_EQ(service.source(), CanSource::kBackup);
  ASSERT_NE(service.authoritative_control(501U), nullptr);
  EXPECT_EQ(service.authoritative_control(501U)->message.hv_on_request, 0U);

  EXPECT_TRUE(service.receive(
      control_frame(PACKCONTROLLER_CAN_BUS_1, 1U, true), 600U).accepted);
  service.update_source(600U);
  EXPECT_EQ(service.source(), CanSource::kBackup);
  for (std::uint32_t now = 700U; now <= 1100U; now += 100U) {
    const auto alive = static_cast<std::uint8_t>((now - 500U) / 100U);
    EXPECT_TRUE(service.receive(
        control_frame(PACKCONTROLLER_CAN_BUS_1, alive, true), now).accepted);
    service.update_source(now);
  }
  EXPECT_EQ(service.source(), CanSource::kMain);
  EXPECT_EQ(service.source_switch_count(), 3U);
}

TEST(CanService, BothStaleRemoveAuthoritativeRequest) {
  CanService service{};
  EXPECT_TRUE(service.receive(
      control_frame(PACKCONTROLLER_CAN_BUS_1, 0U), 0U).accepted);
  service.update_source(0U);
  ASSERT_NE(service.authoritative_control(500U), nullptr);
  service.update_source(501U);
  EXPECT_EQ(service.source(), CanSource::kNone);
  EXPECT_EQ(service.authoritative_control(501U), nullptr);
}

TEST(CanService, OneReturningMainFrameIsNotAStableRecovery) {
  CanService service{};
  EXPECT_TRUE(service.receive(
      control_frame(PACKCONTROLLER_CAN_BUS_2, 0U), 0U).accepted);
  service.update_source(0U);
  ASSERT_EQ(service.source(), CanSource::kBackup);
  EXPECT_TRUE(service.receive(
      control_frame(PACKCONTROLLER_CAN_BUS_1, 0U), 100U).accepted);
  service.update_source(100U);
  service.update_source(600U);
  EXPECT_EQ(service.source(), CanSource::kBackup);
}

TEST(CanService, ServiceAliveIsIndependentPerBusAndRejectsDuplicate) {
  CanService service{};
  pack_controller_bms_service_request_t message{};
  message.service_request_alive_counter = 4U;
  message.service_command =
      PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_DEV_MODE_EXIT_CHOICE;
  packcontroller_can_frame_t frame{};
  frame.identifier = PACK_CONTROLLER_BMS_SERVICE_REQUEST_FRAME_ID;
  frame.length = PACK_CONTROLLER_BMS_SERVICE_REQUEST_LENGTH;
  frame.bus = static_cast<std::uint8_t>(PACKCONTROLLER_CAN_BUS_1);
  frame.is_fd = true;
  frame.bit_rate_switch = true;
  ASSERT_EQ(pack_controller_bms_service_request_pack(
                frame.data, &message, frame.length),
            frame.length);
  EXPECT_TRUE(service.receive(frame, 10U).service_available);
  const auto duplicate = service.receive(frame, 11U);
  EXPECT_FALSE(duplicate.service_available);
  EXPECT_TRUE(duplicate.discontinuity);
  EXPECT_FALSE(service.service_alive(PACKCONTROLLER_CAN_BUS_2).initialized());
}

TEST(CanService, EncodesIndependentStatusAliveCountersOnBothBuses) {
  CanService service{};
  StatusData status{};
  status.developer_mode = 1U;
  status.air_n = true;
  status.air_n_actual = true;
  status.precharge_actual = true;
  status.air_p_actual = true;
  status.dcdc_actual = true;
  status.runtime_seconds = 123456U;
  packcontroller_can_frame_t bus1_first{};
  packcontroller_can_frame_t bus2_first{};
  packcontroller_can_frame_t bus1_second{};
  ASSERT_TRUE(service.make_status_frame(PACKCONTROLLER_CAN_BUS_1, status,
                                        bus1_first));
  ASSERT_TRUE(service.make_status_frame(PACKCONTROLLER_CAN_BUS_2, status,
                                        bus2_first));
  ASSERT_TRUE(service.make_status_frame(PACKCONTROLLER_CAN_BUS_1, status,
                                        bus1_second));

  pack_controller_bms_status_t decoded1{};
  pack_controller_bms_status_t decoded2{};
  pack_controller_bms_status_t decoded3{};
  ASSERT_EQ(pack_controller_bms_status_unpack(
                &decoded1, bus1_first.data, bus1_first.length),
            0);
  ASSERT_EQ(pack_controller_bms_status_unpack(
                &decoded2, bus2_first.data, bus2_first.length),
            0);
  ASSERT_EQ(pack_controller_bms_status_unpack(
                &decoded3, bus1_second.data, bus1_second.length),
            0);
  EXPECT_EQ(decoded1.status_alive_counter, 0U);
  EXPECT_EQ(decoded2.status_alive_counter, 0U);
  EXPECT_EQ(decoded3.status_alive_counter, 1U);
  EXPECT_EQ(decoded3.developer_mode, 1U);
  EXPECT_EQ(decoded3.air_n_switch, 1U);
  EXPECT_EQ(decoded3.air_n_actual, 1U);
  EXPECT_EQ(decoded3.precharge_actual, 1U);
  EXPECT_EQ(decoded3.air_p_actual, 1U);
  EXPECT_EQ(decoded3.dcdc_actual, 1U);
  EXPECT_EQ(decoded3.runtime_seconds, 123456U);
}

TEST(CanService, EncodesAnalogDiagnosticsAndIndependentAliveCounters) {
  CanService service{};
  AnalogData analog{};
  for (std::size_t channel = 0U; channel < analog.raw.size(); ++channel) {
    analog.raw[channel] = static_cast<std::uint16_t>(100U + channel);
    analog.physical[channel] = static_cast<float>(channel) + 0.5F;
    analog.quality[channel] = 1U;
  }
  analog.sample_counter = 0x12345U;
  analog.timestamp_ms = 9876U;
  analog.dma_error_count = 2U;
  analog.dropped_block_count = 3U;
  analog.coherent = true;
  analog.overall_quality = 1U;

  packcontroller_can_frame_t bus1_first{};
  packcontroller_can_frame_t bus2_first{};
  packcontroller_can_frame_t bus1_second{};
  ASSERT_TRUE(service.make_analog_frame(PACKCONTROLLER_CAN_BUS_1, analog,
                                        bus1_first));
  ASSERT_TRUE(service.make_analog_frame(PACKCONTROLLER_CAN_BUS_2, analog,
                                        bus2_first));
  ASSERT_TRUE(service.make_analog_frame(PACKCONTROLLER_CAN_BUS_1, analog,
                                        bus1_second));

  EXPECT_EQ(bus1_first.identifier, PACK_CONTROLLER_BMS_ANALOG_FRAME_ID);
  EXPECT_EQ(bus1_first.length, PACK_CONTROLLER_BMS_ANALOG_LENGTH);
  EXPECT_TRUE(bus1_first.is_fd);
  EXPECT_TRUE(bus1_first.bit_rate_switch);

  pack_controller_bms_analog_t decoded1{};
  pack_controller_bms_analog_t decoded2{};
  pack_controller_bms_analog_t decoded3{};
  ASSERT_EQ(pack_controller_bms_analog_unpack(
                &decoded1, bus1_first.data, bus1_first.length),
            0);
  ASSERT_EQ(pack_controller_bms_analog_unpack(
                &decoded2, bus2_first.data, bus2_first.length),
            0);
  ASSERT_EQ(pack_controller_bms_analog_unpack(
                &decoded3, bus1_second.data, bus1_second.length),
            0);

  EXPECT_EQ(decoded1.analog_alive_counter, 0U);
  EXPECT_EQ(decoded2.analog_alive_counter, 0U);
  EXPECT_EQ(decoded3.analog_alive_counter, 1U);
  EXPECT_EQ(decoded1.analog_protocol_version,
            PACK_CONTROLLER_BMS_ANALOG_ANALOG_PROTOCOL_VERSION_ANALOG_PROTOCOL_V0_CHOICE);
  EXPECT_EQ(decoded1.analog_frame_coherent, 1U);
  EXPECT_EQ(decoded1.analog_quality, 1U);
  EXPECT_EQ(decoded1.analog_sample_counter, 0x2345U);
  EXPECT_EQ(decoded1.analog_sample_timestamp, 9876U);
  EXPECT_EQ(decoded1.raw_r_leak1, 100U);
  EXPECT_EQ(decoded1.raw_tntc2, 111U);
  EXPECT_EQ(decoded1.v_accu_quality, 1U);
  EXPECT_EQ(decoded1.analog_dma_error_count, 2U);
  EXPECT_EQ(decoded1.analog_dropped_block_count, 3U);
  EXPECT_NEAR(pack_controller_bms_analog_v_accu_analog_decode(
                  decoded1.v_accu_analog),
              8.5F, 0.05F);
}

}  // namespace
}  // namespace packcontroller::services
