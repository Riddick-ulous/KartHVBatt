#include <gtest/gtest.h>

#include <cstdint>

#include "pack_controller.h"
#include "packcontroller/app/developer_session.hpp"

namespace packcontroller::app {
namespace {

services::ServiceRequest request(std::uint8_t command,
                                 std::uint32_t value0,
                                 std::uint32_t value1,
                                 std::uint32_t now_ms) {
  services::ServiceRequest value{};
  value.valid = true;
  value.received_ms = now_ms;
  value.message.service_command = command;
  value.message.service_value0 = value0;
  value.message.service_value1 = value1;
  return value;
}

services::ServiceRequest enter(DeveloperMode mode, std::uint32_t key,
                               std::uint32_t now_ms) {
  return request(
      PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_DEV_MODE_ENTER_CHOICE,
      static_cast<std::uint32_t>(mode), key, now_ms);
}

services::ServiceRequest output(std::uint32_t mask, std::uint32_t now_ms) {
  return request(
      PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_DEV_OUTPUT_SET_CHOICE,
      mask, 0U, now_ms);
}

DeveloperGates all_gates() {
  return DeveloperGates{true, true, true, true, true};
}

TEST(DeveloperSession, UsesNormativeBrtBuildKey) {
  EXPECT_EQ(kDeveloperBuildKey, 0x00425254U);
}

TEST(DeveloperSession, ProductionProfileRejectsDeveloperCommands) {
  DeveloperSession session{false, kDeveloperBuildKey};
  const auto result = session.handle(
      enter(DeveloperMode::kOutputTest, kDeveloperBuildKey, 0U), {true, true});
  EXPECT_TRUE(result.handled);
  EXPECT_EQ(result.result, ServiceResult::kDeniedState);
  EXPECT_EQ(session.mode(), DeveloperMode::kDisabled);
}

TEST(DeveloperSession, MissingOrWrongBuildKeyCannotEnter) {
  DeveloperSession missing{true, 0U, false};
  EXPECT_EQ(missing.handle(
                enter(DeveloperMode::kOutputTest, 0U, 0U), {true, true})
                .result,
            ServiceResult::kInvalidValue);

  DeveloperSession session{true, kDeveloperBuildKey};
  EXPECT_EQ(session.handle(
                enter(DeveloperMode::kOutputTest, kDeveloperBuildKey + 1U, 0U),
                {true, true})
                .result,
            ServiceResult::kInvalidValue);
}

TEST(DeveloperSession, EntryRequiresSafeStateAndAllRequestsLow) {
  DeveloperSession session{true, kDeveloperBuildKey};
  EXPECT_EQ(session.handle(
                enter(DeveloperMode::kOutputTest, kDeveloperBuildKey, 0U),
                {false, true})
                .result,
            ServiceResult::kDeniedState);
  EXPECT_EQ(session.handle(
                enter(DeveloperMode::kOutputTest, kDeveloperBuildKey, 0U),
                {true, false})
                .result,
            ServiceResult::kDeniedState);
}

TEST(DeveloperSession, MaskNeedsFreshSessionAndAllIndependentGates) {
  DeveloperSession session{true, kDeveloperBuildKey};
  ASSERT_EQ(session.handle(
                enter(DeveloperMode::kOutputTest, kDeveloperBuildKey, 0U),
                {true, true})
                .result,
            ServiceResult::kOk);
  ASSERT_EQ(session.handle(output(0x0FU, 1U), {false, false}).result,
            ServiceResult::kOk);
  EXPECT_EQ(session.output_mask(1U, all_gates()), 0x0FU);

  DeveloperGates gates = all_gates();
  gates.sc_not_latched = false;
  EXPECT_EQ(session.output_mask(1U, gates), 0U);
  gates = all_gates();
  gates.danger_voltage_clear = false;
  EXPECT_EQ(session.output_mask(1U, gates), 0U);
  gates = all_gates();
  gates.scheduler_healthy = false;
  EXPECT_EQ(session.output_mask(1U, gates), 0U);
}

TEST(DeveloperSession, OutputSetDoesNotRenewEnterKeepalive) {
  DeveloperSession session{true, kDeveloperBuildKey};
  ASSERT_EQ(session.handle(
                enter(DeveloperMode::kOutputTest, kDeveloperBuildKey, 0U),
                {true, true})
                .result,
            ServiceResult::kOk);
  ASSERT_EQ(session.handle(output(0x01U, 500U), {false, false}).result,
            ServiceResult::kOk);
  EXPECT_EQ(session.output_mask(500U, all_gates()), 0x01U);
  session.update(501U, all_gates());
  EXPECT_EQ(session.mode(), DeveloperMode::kDisabled);
  EXPECT_EQ(session.output_mask(501U, all_gates()), 0U);
}

TEST(DeveloperSession, RepeatedEnterRenewsSessionWithoutOutputGlitch) {
  DeveloperSession session{true, kDeveloperBuildKey};
  ASSERT_EQ(session.handle(
                enter(DeveloperMode::kOutputTest, kDeveloperBuildKey, 0U),
                {true, true})
                .result,
            ServiceResult::kOk);
  ASSERT_EQ(session.handle(output(0x08U, 1U), {false, false}).result,
            ServiceResult::kOk);
  EXPECT_EQ(session.handle(
                enter(DeveloperMode::kOutputTest, kDeveloperBuildKey, 500U),
                {false, false})
                .result,
            ServiceResult::kOk);
  EXPECT_TRUE(session.session_fresh(1000U));
  EXPECT_EQ(session.output_mask(1000U, all_gates()), 0x08U);
}

TEST(DeveloperSession, ModeChangeRequiresExitAndFreshSafeEntry) {
  DeveloperSession session{true, kDeveloperBuildKey};
  ASSERT_EQ(session.handle(
                enter(DeveloperMode::kOutputTest, kDeveloperBuildKey, 0U),
                {true, true})
                .result,
            ServiceResult::kOk);
  EXPECT_EQ(session.handle(
                enter(DeveloperMode::kCommissioning, kDeveloperBuildKey, 1U),
                {true, true})
                .result,
            ServiceResult::kDeniedState);
  EXPECT_EQ(session.mode(), DeveloperMode::kOutputTest);
}

TEST(DeveloperSession, InvalidMaskIsRejectedWithoutChangingOutputs) {
  DeveloperSession session{true, kDeveloperBuildKey};
  ASSERT_EQ(session.handle(
                enter(DeveloperMode::kOutputTest, kDeveloperBuildKey, 0U),
                {true, true})
                .result,
            ServiceResult::kOk);
  EXPECT_EQ(session.handle(output(0x10U, 1U), {false, false}).result,
            ServiceResult::kInvalidValue);
  EXPECT_EQ(session.output_mask(1U, all_gates()), 0U);
}

TEST(DeveloperSession, CommissioningTimeoutReportsSessionLoss) {
  DeveloperSession session{true, kDeveloperBuildKey};
  ASSERT_EQ(session.handle(
                enter(DeveloperMode::kCommissioning, kDeveloperBuildKey, 0U),
                {true, true})
                .result,
            ServiceResult::kOk);
  session.update(501U, all_gates());
  EXPECT_TRUE(session.commissioning_session_lost());
  EXPECT_EQ(session.mode(), DeveloperMode::kDisabled);
}

TEST(DeveloperSession, CommissioningExitOnlyReportsLossWithActiveRequests) {
  DeveloperSession safe_exit{true, kDeveloperBuildKey};
  ASSERT_EQ(safe_exit.handle(
                enter(DeveloperMode::kCommissioning, kDeveloperBuildKey, 0U),
                {true, true})
                .result,
            ServiceResult::kOk);
  EXPECT_EQ(safe_exit.handle(
                request(
                    PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_DEV_MODE_EXIT_CHOICE,
                    0U, 0U, 1U),
                {false, true})
                .result,
            ServiceResult::kOk);
  EXPECT_FALSE(safe_exit.commissioning_session_lost());

  DeveloperSession active_exit{true, kDeveloperBuildKey};
  ASSERT_EQ(active_exit.handle(
                enter(DeveloperMode::kCommissioning, kDeveloperBuildKey, 0U),
                {true, true})
                .result,
            ServiceResult::kOk);
  EXPECT_EQ(active_exit.handle(
                request(
                    PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_DEV_MODE_EXIT_CHOICE,
                    0U, 0U, 1U),
                {false, false})
                .result,
            ServiceResult::kOk);
  EXPECT_TRUE(active_exit.commissioning_session_lost());
}

TEST(SafetyInputConfirmation, StartsUnsafeAndConfirmsAfterOneHundredMs) {
  SafetyInputConfirmation confirmation{};
  EXPECT_FALSE(confirmation.all_power_paths_open());
  const packcontroller_safety_inputs_t safe{true, true, false};
  confirmation.update(safe, 10U);
  EXPECT_FALSE(confirmation.all_power_paths_open());
  EXPECT_FALSE(confirmation.gates(true, true).all_allow_output());
  confirmation.update(safe, 109U);
  EXPECT_FALSE(confirmation.gates(true, true).all_allow_output());
  confirmation.update(safe, 110U);
  EXPECT_TRUE(confirmation.gates(true, true).all_allow_output());
  EXPECT_TRUE(confirmation.all_power_paths_open());
  const auto actuals = confirmation.confirmed_actuals();
  EXPECT_FALSE(actuals.air_n);
  EXPECT_FALSE(actuals.precharge);
  EXPECT_FALSE(actuals.air_p);
  EXPECT_FALSE(actuals.dcdc);
  EXPECT_EQ(confirmation.confirmed_bitmap() & (1U << 10U), 0U);
}

TEST(OutputArbiter, RechecksGatesAndMapsAllFourMaskBits) {
  OutputArbiter arbiter{};
  const auto outputs = arbiter.resolve(DeveloperMode::kOutputTest, 0x0FU,
                                       all_gates(), false);
  EXPECT_TRUE(outputs.air_n);
  EXPECT_TRUE(outputs.precharge);
  EXPECT_TRUE(outputs.air_p);
  EXPECT_TRUE(outputs.dcdc);

  auto unsafe = all_gates();
  unsafe.por_valid = false;
  const auto inhibited = arbiter.resolve(DeveloperMode::kOutputTest, 0x0FU,
                                         unsafe, false);
  EXPECT_FALSE(inhibited.air_n);
  EXPECT_FALSE(inhibited.precharge);
  EXPECT_FALSE(inhibited.air_p);
  EXPECT_FALSE(inhibited.dcdc);
}

}  // namespace
}  // namespace packcontroller::app
