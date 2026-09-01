#include "packcontroller/services/eeprom_driver.hpp"

#include <algorithm>

namespace packcontroller::services {

bool EepromDriver::transport_valid() const noexcept {
  return (transport_.start_read != nullptr) &&
         (transport_.start_page_write != nullptr) &&
         (transport_.start_ack_poll != nullptr) &&
         (transport_.status != nullptr) &&
         (transport_.clear_result != nullptr) &&
         (transport_.set_write_protected != nullptr);
}

bool EepromDriver::range_valid(std::uint16_t address,
                               std::uint16_t length) const noexcept {
  return (length > 0U) &&
         (length <= static_cast<std::uint16_t>(kMaximumTransferSize)) &&
         (static_cast<std::uint32_t>(address) + length <= kSizeBytes);
}

bool EepromDriver::start_read(std::uint16_t address, std::uint8_t* data,
                              std::uint16_t length) noexcept {
  if (busy()) {
    return false;
  }
  if (!transport_valid() || (data == nullptr) ||
      !range_valid(address, length)) {
    result_ = EepromResult::kInvalidArgument;
    return false;
  }
  address_ = address;
  length_ = length;
  read_destination_ = data;
  result_ = EepromResult::kBusy;
  phase_ = Phase::kStartRead;
  return true;
}

bool EepromDriver::start_write(std::uint16_t address,
                               const std::uint8_t* data,
                               std::uint16_t length) noexcept {
  if (busy()) {
    return false;
  }
  if (!transport_valid() || (data == nullptr) ||
      !range_valid(address, length)) {
    result_ = EepromResult::kInvalidArgument;
    return false;
  }
  std::copy_n(data, length, expected_.begin());
  address_ = address;
  length_ = length;
  offset_ = 0U;
  current_page_length_ = 0U;
  ack_poll_attempts_ = 0U;
  page_write_count_ = 0U;
  read_destination_ = nullptr;
  result_ = EepromResult::kBusy;
  phase_ = Phase::kStartPageWrite;
  return true;
}

void EepromDriver::reset_result() noexcept {
  if (!busy()) {
    result_ = EepromResult::kIdle;
    phase_ = Phase::kIdle;
  }
}

void EepromDriver::fail(EepromResult result) noexcept {
  transport_.set_write_protected(transport_.context, true);
  result_ = result;
  phase_ = Phase::kIdle;
}

void EepromDriver::finish_success() noexcept {
  transport_.set_write_protected(transport_.context, true);
  result_ = EepromResult::kSuccess;
  phase_ = Phase::kIdle;
}

bool EepromDriver::io_still_pending(EepromIoStatus status) noexcept {
  if (status != EepromIoStatus::kBusy) {
    io_wait_calls_ = 0U;
    return false;
  }
  ++io_wait_calls_;
  if (io_wait_calls_ >= kMaximumIoWaitCalls) {
    fail(EepromResult::kCommunicationError);
  }
  return true;
}

std::uint8_t EepromDriver::next_page_length() const noexcept {
  const std::uint16_t absolute =
      static_cast<std::uint16_t>(address_ + offset_);
  const std::uint16_t page_remaining = static_cast<std::uint16_t>(
      kPageSize - static_cast<std::uint16_t>(absolute % kPageSize));
  const std::uint16_t transfer_remaining =
      static_cast<std::uint16_t>(length_ - offset_);
  return static_cast<std::uint8_t>(
      std::min(page_remaining, transfer_remaining));
}

void EepromDriver::service() noexcept {
  if (!busy()) {
    return;
  }

  if (phase_ == Phase::kStartRead) {
    transport_.clear_result(transport_.context);
    if (!transport_.start_read(transport_.context, address_,
                               read_destination_, length_)) {
      fail(EepromResult::kCommunicationError);
      return;
    }
    phase_ = Phase::kAwaitRead;
    return;
  }

  if (phase_ == Phase::kAwaitRead) {
    const EepromIoStatus status = transport_.status(transport_.context);
    if (io_still_pending(status)) {
      return;
    }
    transport_.clear_result(transport_.context);
    if (status == EepromIoStatus::kComplete) {
      finish_success();
    } else {
      fail(EepromResult::kCommunicationError);
    }
    return;
  }

  if (phase_ == Phase::kStartPageWrite) {
    current_page_length_ = next_page_length();
    transport_.clear_result(transport_.context);
    transport_.set_write_protected(transport_.context, false);
    if (!transport_.start_page_write(
            transport_.context,
            static_cast<std::uint16_t>(address_ + offset_),
            expected_.data() + offset_, current_page_length_)) {
      fail(EepromResult::kCommunicationError);
      return;
    }
    phase_ = Phase::kAwaitPageWrite;
    return;
  }

  if (phase_ == Phase::kAwaitPageWrite) {
    const EepromIoStatus status = transport_.status(transport_.context);
    if (io_still_pending(status)) {
      return;
    }
    transport_.clear_result(transport_.context);
    transport_.set_write_protected(transport_.context, true);
    if (status != EepromIoStatus::kComplete) {
      fail(EepromResult::kCommunicationError);
      return;
    }
    ++page_write_count_;
    ack_poll_attempts_ = 0U;
    phase_ = Phase::kStartAckPoll;
    return;
  }

  if (phase_ == Phase::kStartAckPoll) {
    transport_.clear_result(transport_.context);
    if (!transport_.start_ack_poll(transport_.context)) {
      fail(EepromResult::kCommunicationError);
      return;
    }
    phase_ = Phase::kAwaitAckPoll;
    return;
  }

  if (phase_ == Phase::kAwaitAckPoll) {
    const EepromIoStatus status = transport_.status(transport_.context);
    if (io_still_pending(status)) {
      return;
    }
    transport_.clear_result(transport_.context);
    if (status == EepromIoStatus::kComplete) {
      offset_ = static_cast<std::uint16_t>(offset_ + current_page_length_);
      phase_ = (offset_ < length_) ? Phase::kStartPageWrite
                                  : Phase::kStartVerifyRead;
      return;
    }
    if ((status == EepromIoStatus::kNotReady) &&
        (++ack_poll_attempts_ < kMaximumAckPollAttempts)) {
      phase_ = Phase::kStartAckPoll;
      return;
    }
    fail(EepromResult::kCommunicationError);
    return;
  }

  if (phase_ == Phase::kStartVerifyRead) {
    transport_.clear_result(transport_.context);
    if (!transport_.start_read(transport_.context, address_, readback_.data(),
                               length_)) {
      fail(EepromResult::kCommunicationError);
      return;
    }
    phase_ = Phase::kAwaitVerifyRead;
    return;
  }

  if (phase_ == Phase::kAwaitVerifyRead) {
    const EepromIoStatus status = transport_.status(transport_.context);
    if (io_still_pending(status)) {
      return;
    }
    transport_.clear_result(transport_.context);
    if (status != EepromIoStatus::kComplete) {
      fail(EepromResult::kCommunicationError);
      return;
    }
    const bool verified = std::equal(expected_.begin(),
                                     expected_.begin() + length_,
                                     readback_.begin());
    if (!verified) {
      fail(EepromResult::kVerifyError);
      return;
    }
    finish_success();
  }
}

}  // namespace packcontroller::services
