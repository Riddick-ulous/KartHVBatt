#ifndef PACKCONTROLLER_SERVICES_EEPROM_DRIVER_HPP
#define PACKCONTROLLER_SERVICES_EEPROM_DRIVER_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace packcontroller::services {

enum class EepromIoStatus : std::uint8_t {
  kIdle = 0U,
  kBusy = 1U,
  kComplete = 2U,
  kNotReady = 3U,
  kError = 4U,
};

struct EepromTransport final {
  void* context{nullptr};
  bool (*start_read)(void*, std::uint16_t, std::uint8_t*,
                     std::uint16_t){nullptr};
  bool (*start_page_write)(void*, std::uint16_t, const std::uint8_t*,
                           std::uint8_t){nullptr};
  bool (*start_ack_poll)(void*){nullptr};
  EepromIoStatus (*status)(void*){nullptr};
  void (*clear_result)(void*){nullptr};
  void (*set_write_protected)(void*, bool){nullptr};
};

enum class EepromResult : std::uint8_t {
  kIdle = 0U,
  kBusy = 1U,
  kSuccess = 2U,
  kInvalidArgument = 3U,
  kCommunicationError = 4U,
  kVerifyError = 5U,
};

class EepromDriver final {
 public:
  static constexpr std::uint16_t kSizeBytes = 32768U;
  static constexpr std::uint16_t kPageSize = 64U;
  static constexpr std::size_t kMaximumTransferSize = 512U;

  explicit EepromDriver(EepromTransport transport) noexcept
      : transport_(transport) {}

  bool start_read(std::uint16_t address, std::uint8_t* data,
                  std::uint16_t length) noexcept;
  bool start_write(std::uint16_t address, const std::uint8_t* data,
                   std::uint16_t length) noexcept;
  void service() noexcept;

  [[nodiscard]] EepromResult result() const noexcept { return result_; }
  [[nodiscard]] bool busy() const noexcept {
    return result_ == EepromResult::kBusy;
  }
  [[nodiscard]] std::uint16_t page_write_count() const noexcept {
    return page_write_count_;
  }
  void reset_result() noexcept;

 private:
  enum class Phase : std::uint8_t {
    kIdle,
    kStartRead,
    kAwaitRead,
    kStartPageWrite,
    kAwaitPageWrite,
    kStartAckPoll,
    kAwaitAckPoll,
    kStartVerifyRead,
    kAwaitVerifyRead,
  };

  static constexpr std::uint8_t kMaximumAckPollAttempts = 20U;
  static constexpr std::uint8_t kMaximumIoWaitCalls = 50U;

  [[nodiscard]] bool transport_valid() const noexcept;
  [[nodiscard]] bool range_valid(std::uint16_t address,
                                 std::uint16_t length) const noexcept;
  void fail(EepromResult result) noexcept;
  void finish_success() noexcept;
  [[nodiscard]] bool io_still_pending(EepromIoStatus status) noexcept;
  [[nodiscard]] std::uint8_t next_page_length() const noexcept;

  EepromTransport transport_{};
  EepromResult result_{EepromResult::kIdle};
  Phase phase_{Phase::kIdle};
  std::uint16_t address_{0U};
  std::uint16_t length_{0U};
  std::uint16_t offset_{0U};
  std::uint8_t current_page_length_{0U};
  std::uint8_t ack_poll_attempts_{0U};
  std::uint8_t io_wait_calls_{0U};
  std::uint16_t page_write_count_{0U};
  std::uint8_t* read_destination_{nullptr};
  std::array<std::uint8_t, kMaximumTransferSize> expected_{};
  std::array<std::uint8_t, kMaximumTransferSize> readback_{};
};

}  // namespace packcontroller::services

#endif
