#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "pack_controller.h"
#include "packcontroller/app/nvm.hpp"
#include "packcontroller/services/eeprom_driver.hpp"

namespace packcontroller::app {
namespace {

constexpr std::uint16_t kImdTarget =
    PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_TARGET_IMD_CONFIG_CHOICE;

using services::EepromDriver;
using services::EepromIoStatus;
using services::EepromResult;
using services::EepromTransport;

struct FakeEeprom final {
  enum class Pending : std::uint8_t { kNone, kRead, kWrite, kProbe };
  struct Write final {
    std::uint16_t address;
    std::uint8_t length;
  };

  std::array<std::uint8_t, EepromDriver::kSizeBytes> memory{};
  std::vector<Write> writes{};
  std::vector<bool> wp_levels{true};
  Pending pending{Pending::kNone};
  EepromIoStatus current{EepromIoStatus::kIdle};
  std::uint16_t address{0U};
  std::uint16_t length{0U};
  std::uint8_t* read_data{nullptr};
  const std::uint8_t* write_data{nullptr};
  std::uint8_t not_ready_polls{0U};
  bool fail_next_transfer{false};
  bool corrupt_next_read{false};
  bool hold_busy{false};

  FakeEeprom() { memory.fill(0xFFU); }

  static bool start_read(void* context, std::uint16_t address,
                         std::uint8_t* data, std::uint16_t length) {
    auto& self = *static_cast<FakeEeprom*>(context);
    if (self.current == EepromIoStatus::kBusy) {
      return false;
    }
    self.pending = Pending::kRead;
    self.current = EepromIoStatus::kBusy;
    self.address = address;
    self.length = length;
    self.read_data = data;
    return true;
  }

  static bool start_page_write(void* context, std::uint16_t address,
                               const std::uint8_t* data,
                               std::uint8_t length) {
    auto& self = *static_cast<FakeEeprom*>(context);
    if (self.current == EepromIoStatus::kBusy) {
      return false;
    }
    self.pending = Pending::kWrite;
    self.current = EepromIoStatus::kBusy;
    self.address = address;
    self.length = length;
    self.write_data = data;
    return true;
  }

  static bool start_probe(void* context) {
    auto& self = *static_cast<FakeEeprom*>(context);
    if (self.current == EepromIoStatus::kBusy) {
      return false;
    }
    self.pending = Pending::kProbe;
    self.current = EepromIoStatus::kBusy;
    return true;
  }

  static EepromIoStatus status(void* context) {
    auto& self = *static_cast<FakeEeprom*>(context);
    if (self.current != EepromIoStatus::kBusy) {
      return self.current;
    }
    if (self.hold_busy) {
      return self.current;
    }
    if (self.pending == Pending::kProbe) {
      if (self.not_ready_polls > 0U) {
        --self.not_ready_polls;
        self.current = EepromIoStatus::kNotReady;
      } else {
        self.current = EepromIoStatus::kComplete;
      }
      return self.current;
    }
    if (self.fail_next_transfer) {
      self.fail_next_transfer = false;
      self.current = EepromIoStatus::kError;
      return self.current;
    }
    if (self.pending == Pending::kRead) {
      std::copy_n(self.memory.begin() + self.address, self.length,
                  self.read_data);
      if (self.corrupt_next_read && (self.length > 0U)) {
        self.read_data[0U] ^= 0x01U;
        self.corrupt_next_read = false;
      }
    } else if (self.pending == Pending::kWrite) {
      std::copy_n(self.write_data, self.length,
                  self.memory.begin() + self.address);
      self.writes.push_back(
          {self.address, static_cast<std::uint8_t>(self.length)});
    }
    self.current = EepromIoStatus::kComplete;
    return self.current;
  }

  static void clear(void* context) {
    auto& self = *static_cast<FakeEeprom*>(context);
    if (self.current != EepromIoStatus::kBusy) {
      self.current = EepromIoStatus::kIdle;
      self.pending = Pending::kNone;
    }
  }

  static void set_wp(void* context, bool enabled) {
    static_cast<FakeEeprom*>(context)->wp_levels.push_back(enabled);
  }

  EepromTransport transport() {
    return {this, start_read, start_page_write, start_probe, status, clear,
            set_wp};
  }

  void abandon_pending() {
    pending = Pending::kNone;
    current = EepromIoStatus::kIdle;
  }
};

template <typename Predicate, typename Service>
bool run_until(Predicate predicate, Service service,
               std::size_t limit = 10000U) {
  for (std::size_t iteration = 0U; iteration < limit; ++iteration) {
    if (predicate()) {
      return true;
    }
    service();
  }
  return predicate();
}

services::ServiceRequest service_request(std::uint8_t command,
                                         std::uint16_t target = 0U) {
  services::ServiceRequest request{};
  request.valid = true;
  request.bus = PACKCONTROLLER_CAN_BUS_1;
  request.message.service_command = command;
  request.message.service_target = target;
  request.message.service_sequence = 42U;
  return request;
}

TEST(EepromDriver, SplitsWritesAtPhysicalPageBoundariesAndAckPolls) {
  FakeEeprom fake{};
  fake.not_ready_polls = 2U;
  EepromDriver driver{fake.transport()};
  std::array<std::uint8_t, 130U> data{};
  for (std::size_t index = 0U; index < data.size(); ++index) {
    data[index] = static_cast<std::uint8_t>(index);
  }

  ASSERT_TRUE(driver.start_write(60U, data.data(),
                                 static_cast<std::uint16_t>(data.size())));
  ASSERT_TRUE(run_until([&] { return !driver.busy(); },
                        [&] { driver.service(); }));
  EXPECT_EQ(driver.result(), EepromResult::kSuccess);
  ASSERT_EQ(fake.writes.size(), 3U);
  EXPECT_EQ(fake.writes[0U].address, 60U);
  EXPECT_EQ(fake.writes[0U].length, 4U);
  EXPECT_EQ(fake.writes[1U].address, 64U);
  EXPECT_EQ(fake.writes[1U].length, 64U);
  EXPECT_EQ(fake.writes[2U].address, 128U);
  EXPECT_EQ(fake.writes[2U].length, 62U);
  EXPECT_TRUE(fake.wp_levels.back());
  EXPECT_NE(std::find(fake.wp_levels.begin(), fake.wp_levels.end(), false),
            fake.wp_levels.end());
  EXPECT_EQ(std::vector<std::uint8_t>(fake.memory.begin() + 60U,
                                      fake.memory.begin() + 190U),
            std::vector<std::uint8_t>(data.begin(), data.end()));
}

TEST(EepromDriver, ReportsReadbackMismatchAndRestoresWriteProtection) {
  FakeEeprom fake{};
  EepromDriver driver{fake.transport()};
  const std::array<std::uint8_t, 4U> data{1U, 2U, 3U, 4U};
  ASSERT_TRUE(driver.start_write(0U, data.data(), 4U));
  fake.corrupt_next_read = true;

  ASSERT_TRUE(run_until([&] { return !driver.busy(); },
                        [&] { driver.service(); }));
  EXPECT_EQ(driver.result(), EepromResult::kVerifyError);
  EXPECT_TRUE(fake.wp_levels.back());
}

TEST(EepromDriver, TimesOutAStuckAsyncTransferAndRestoresWriteProtection) {
  FakeEeprom fake{};
  fake.hold_busy = true;
  EepromDriver driver{fake.transport()};
  const std::array<std::uint8_t, 1U> data{0x5AU};
  ASSERT_TRUE(driver.start_write(0U, data.data(), 1U));

  ASSERT_TRUE(run_until([&] { return !driver.busy(); },
                        [&] { driver.service(); }, 100U));
  EXPECT_EQ(driver.result(), EepromResult::kCommunicationError);
  EXPECT_TRUE(fake.wp_levels.back());
}

TEST(SystemConfig, ExplicitSerializationRoundTripsAndRejectsInvalidTopology) {
  SystemConfig config = default_system_config();
  config.imd.hardware_type = 2U;
  config.analog.hv_offset_uv = {-1000, 2000, -3000};
  std::array<std::uint8_t, kSystemConfigPayloadSize> payload{};
  serialize_system_config(config, payload);
  SystemConfig decoded{};

  ASSERT_TRUE(deserialize_system_config(payload, decoded));
  EXPECT_EQ(decoded.imd.hardware_type, 2U);
  EXPECT_EQ(decoded.hv.precharge_min_mv, 364500U);
  EXPECT_EQ(decoded.analog.hv_offset_uv[2U], -3000);
  payload[4U] = 161U;
  payload[5U] = 0U;
  EXPECT_FALSE(deserialize_system_config(payload, decoded));
}

TEST(NvmCrc, MatchesStandardIeeeTestVector) {
  constexpr std::array<std::uint8_t, 9U> data{
      '1', '2', '3', '4', '5', '6', '7', '8', '9'};
  EXPECT_EQ(nvm_crc32(data.data(), data.size()), 0xCBF43926U);
}

TEST(NvmRecordStore, SelectsNewestValidAbSlot) {
  FakeEeprom fake{};
  EepromDriver driver{fake.transport()};
  NvmRecordStore store{driver};
  const NvmRegion region{0U, 1U, 1U, 2U};
  std::array<std::uint8_t, 8U> first{1U, 2U, 3U, 4U};
  std::array<std::uint8_t, 8U> second{9U, 8U, 7U, 6U};

  ASSERT_TRUE(store.start_write_next(region, first.data(), 4U));
  ASSERT_TRUE(run_until([&] { return !store.busy(); },
                        [&] { store.service(); }));
  ASSERT_EQ(store.result(), NvmRecordResult::kSuccess);
  ASSERT_TRUE(store.start_write_next(region, second.data(), 4U));
  ASSERT_TRUE(run_until([&] { return !store.busy(); },
                        [&] { store.service(); }));
  ASSERT_EQ(store.result(), NvmRecordResult::kSuccess);

  std::array<std::uint8_t, 8U> loaded{};
  ASSERT_TRUE(store.start_read_latest(region, loaded.data(), 8U));
  ASSERT_TRUE(run_until([&] { return !store.busy(); },
                        [&] { store.service(); }));
  EXPECT_EQ(store.result(), NvmRecordResult::kSuccess);
  EXPECT_EQ(store.latest_sequence(), 2U);
  EXPECT_EQ(std::vector<std::uint8_t>(loaded.begin(), loaded.begin() + 4U),
            std::vector<std::uint8_t>(second.begin(), second.begin() + 4U));
}

TEST(NvmRecordStore, InterruptedCommitKeepsPreviousSlotRecoverable) {
  FakeEeprom fake{};
  EepromDriver driver{fake.transport()};
  NvmRecordStore store{driver};
  const NvmRegion region{0U, 1U, 1U, 2U};
  const std::array<std::uint8_t, 4U> first{1U, 2U, 3U, 4U};
  const std::array<std::uint8_t, 4U> second{5U, 6U, 7U, 8U};
  ASSERT_TRUE(store.start_write_next(region, first.data(), 4U));
  ASSERT_TRUE(run_until([&] { return !store.busy(); },
                        [&] { store.service(); }));
  ASSERT_TRUE(store.start_write_next(region, second.data(), 4U));

  const bool body_written_without_commit = run_until(
      [&] {
        return (fake.memory[0x0200U] == 0x50U) &&
               (fake.memory[0x0200U + kNvmCommitOffset] == 0U);
      },
      [&] { store.service(); });
  ASSERT_TRUE(body_written_without_commit);
  fake.abandon_pending();

  EepromDriver reboot_driver{fake.transport()};
  NvmRecordStore reboot_store{reboot_driver};
  std::array<std::uint8_t, 4U> loaded{};
  ASSERT_TRUE(reboot_store.start_read_latest(region, loaded.data(), 4U));
  ASSERT_TRUE(run_until([&] { return !reboot_store.busy(); },
                        [&] { reboot_store.service(); }));
  EXPECT_EQ(reboot_store.result(), NvmRecordResult::kSuccess);
  EXPECT_EQ(reboot_store.latest_sequence(), 1U);
  EXPECT_EQ(loaded, first);
}

TEST(NvmRecordStore, RejectsCorruptedCommittedRecordByCrc) {
  FakeEeprom fake{};
  EepromDriver driver{fake.transport()};
  NvmRecordStore store{driver};
  const NvmRegion region{0U, 1U, 1U, 2U};
  const std::array<std::uint8_t, 4U> payload{1U, 2U, 3U, 4U};
  ASSERT_TRUE(store.start_write_next(region, payload.data(), 4U));
  ASSERT_TRUE(run_until([&] { return !store.busy(); },
                        [&] { store.service(); }));
  fake.memory[20U] ^= 0x80U;

  std::array<std::uint8_t, 4U> loaded{};
  ASSERT_TRUE(store.start_read_latest(region, loaded.data(), 4U));
  ASSERT_TRUE(run_until([&] { return !store.busy(); },
                        [&] { store.service(); }));
  EXPECT_EQ(store.result(), NvmRecordResult::kCrcError);
  EXPECT_TRUE(store.crc_error_seen());
}

TEST(NvmRecordStore, RotatesJournalAndKeepsNewestRecord) {
  FakeEeprom fake{};
  EepromDriver driver{fake.transport()};
  NvmRecordStore store{driver};
  const NvmRegion region{kSocSohJournalAddress, 2U, 1U,
                         kSocSohJournalSlotCount};
  std::array<std::uint8_t, 4U> payload{};

  for (std::uint32_t sequence = 1U;
       sequence <= static_cast<std::uint32_t>(kSocSohJournalSlotCount) + 1U;
       ++sequence) {
    payload[0U] = static_cast<std::uint8_t>(sequence);
    ASSERT_TRUE(store.start_write_next(region, payload.data(), 1U));
    ASSERT_TRUE(run_until([&] { return !store.busy(); },
                          [&] { store.service(); }));
    ASSERT_EQ(store.result(), NvmRecordResult::kSuccess);
  }

  std::array<std::uint8_t, 4U> loaded{};
  ASSERT_TRUE(store.start_read_latest(
      region, loaded.data(), static_cast<std::uint16_t>(loaded.size())));
  ASSERT_TRUE(run_until([&] { return !store.busy(); },
                        [&] { store.service(); }));
  EXPECT_EQ(store.result(), NvmRecordResult::kSuccess);
  EXPECT_EQ(store.latest_sequence(), 15U);
  EXPECT_EQ(loaded[0U], 15U);
}

TEST(NvmManager, BlankEepromUsesDefaultsWithoutWriting) {
  FakeEeprom fake{};
  EepromDriver driver{fake.transport()};
  NvmManager manager{driver};
  ASSERT_TRUE(manager.start_boot_load());
  ASSERT_TRUE(run_until([&] { return manager.initialized(); },
                        [&] { manager.service(); }));
  EXPECT_EQ(manager.sequence(), 0U);
  EXPECT_EQ(manager.active_config().series_cells, 162U);
  EXPECT_TRUE(fake.writes.empty());
}

TEST(NvmManager, StagesCommitsAndRecoversConfigurationAfterRestart) {
  FakeEeprom fake{};
  EepromDriver driver{fake.transport()};
  NvmManager manager{driver};
  ASSERT_TRUE(manager.start_boot_load());
  ASSERT_TRUE(run_until([&] { return manager.initialized(); },
                        [&] { manager.service(); }));

  auto stage = service_request(
      PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_CONFIG_STAGE_CHOICE,
      kImdTarget);
  stage.message.service_sub_index = 0U;
  stage.message.service_payload_length = 1U;
  stage.message.service_value0 = 2U;
  EXPECT_EQ(manager.handle(stage, true).result, 0U);

  auto commit = service_request(
      PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_CONFIG_COMMIT_CHOICE,
      kImdTarget);
  commit.message.service_commit_request = 1U;
  const auto accepted = manager.handle(commit, true);
  EXPECT_TRUE(accepted.deferred);
  EXPECT_EQ(accepted.result, 1U);
  NvmServiceCompletion completion{};
  ASSERT_TRUE(run_until([&] { return manager.take_completion(completion); },
                        [&] { manager.service(); }));
  EXPECT_EQ(completion.reply.result, 0U);
  EXPECT_EQ(completion.reply.nvm_sequence, 1U);
  EXPECT_EQ(manager.active_config().imd.hardware_type, 0U);

  auto read = service_request(
      PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_CONFIG_READ_CHOICE,
      kImdTarget);
  const auto read_reply = manager.handle(read, true);
  EXPECT_EQ(read_reply.result, 0U);
  EXPECT_EQ(read_reply.value0 & 0xFFU, 2U);
  EXPECT_EQ(read_reply.nvm_sequence, 1U);

  EepromDriver reboot_driver{fake.transport()};
  NvmManager rebooted{reboot_driver};
  ASSERT_TRUE(rebooted.start_boot_load());
  ASSERT_TRUE(run_until([&] { return rebooted.initialized(); },
                        [&] { rebooted.service(); }));
  EXPECT_EQ(rebooted.active_config().imd.hardware_type, 2U);
  EXPECT_EQ(rebooted.sequence(), 1U);
}

TEST(NvmManager, SelftestRequiresSafeExplicitCommitAndTouchesOnlyTestPage) {
  FakeEeprom fake{};
  EepromDriver driver{fake.transport()};
  NvmManager manager{driver};
  ASSERT_TRUE(manager.start_boot_load());
  ASSERT_TRUE(run_until([&] { return manager.initialized(); },
                        [&] { manager.service(); }));
  const auto before = fake.memory;
  auto request = service_request(
      PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_EEPROM_SELFTEST_CHOICE);

  EXPECT_EQ(manager.handle(request, true).result, 4U);
  request.message.service_commit_request = 1U;
  EXPECT_EQ(manager.handle(request, false).result, 2U);
  const auto accepted = manager.handle(request, true);
  EXPECT_TRUE(accepted.deferred);
  NvmServiceCompletion completion{};
  ASSERT_TRUE(run_until([&] { return manager.take_completion(completion); },
                        [&] { manager.service(); }));
  EXPECT_EQ(completion.reply.result, 0U);
  EXPECT_FALSE(manager.selftest_fault());
  EXPECT_TRUE(std::equal(before.begin(),
                         before.begin() + kEepromTestPageAddress,
                         fake.memory.begin()));
  EXPECT_NE(fake.memory[kEepromTestPageAddress],
            before[kEepromTestPageAddress]);
}

TEST(NvmManager, FailedSelftestRaisesFaultAndRestoresWriteProtection) {
  FakeEeprom fake{};
  EepromDriver driver{fake.transport()};
  NvmManager manager{driver};
  ASSERT_TRUE(manager.start_boot_load());
  ASSERT_TRUE(run_until([&] { return manager.initialized(); },
                        [&] { manager.service(); }));
  auto request = service_request(
      PACK_CONTROLLER_BMS_SERVICE_REQUEST_SERVICE_COMMAND_EEPROM_SELFTEST_CHOICE);
  request.message.service_commit_request = 1U;
  fake.fail_next_transfer = true;

  ASSERT_TRUE(manager.handle(request, true).deferred);
  NvmServiceCompletion completion{};
  ASSERT_TRUE(run_until([&] { return manager.take_completion(completion); },
                        [&] { manager.service(); }));
  EXPECT_EQ(completion.reply.result, 6U);
  EXPECT_TRUE(manager.selftest_fault());
  EXPECT_TRUE(fake.wp_levels.back());
}

}  // namespace
}  // namespace packcontroller::app
