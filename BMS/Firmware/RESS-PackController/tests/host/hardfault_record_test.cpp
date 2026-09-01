#include <gtest/gtest.h>

#include <cstdint>

#include "packcontroller/startup/hardfault_record.h"

namespace packcontroller {
namespace {

TEST(HardFaultRecord, CapturesStackAndFaultStatusWithValidCrc) {
  const packcontroller_exception_frame_t frame{
      0U, 1U, 2U, 3U, 12U, 14U, 15U, 16U};
  const packcontroller_fault_status_t status{
      0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U};
  packcontroller_hardfault_record_t record{};

  packcontroller_hardfault_record_write(&record, &frame, &status,
                                         0xFFFFFFF9U);

  EXPECT_TRUE(packcontroller_hardfault_record_valid(&record));
  EXPECT_EQ(record.magic, PACKCONTROLLER_HARDFAULT_RECORD_MAGIC);
  EXPECT_EQ(record.frame.pc, 15U);
  EXPECT_EQ(record.status.cfsr, 0x01U);
  EXPECT_EQ(record.exc_return, 0xFFFFFFF9U);
}

TEST(HardFaultRecord, RejectsCorruptionAndUninitializedMemory) {
  packcontroller_hardfault_record_t record{};
  EXPECT_FALSE(packcontroller_hardfault_record_valid(&record));

  const packcontroller_exception_frame_t frame{};
  const packcontroller_fault_status_t status{};
  packcontroller_hardfault_record_write(&record, &frame, &status,
                                         0xFFFFFFFDU);
  record.frame.lr ^= 1U;
  EXPECT_FALSE(packcontroller_hardfault_record_valid(&record));
}

}  // namespace
}  // namespace packcontroller
