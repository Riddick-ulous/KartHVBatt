#ifndef PACKCONTROLLER_CORE_SIGNALS_HPP
#define PACKCONTROLLER_CORE_SIGNALS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "packcontroller/core/time.hpp"

namespace packcontroller::core {

enum class SignalQuality : std::uint8_t {
  kInvalid = 0U,
  kValid = 1U,
  kStale = 2U,
  kFault = 3U,
};

template <typename T>
struct Signal final {
  T value{};
  std::uint32_t timestamp_ms{0U};
  SignalQuality quality{SignalQuality::kInvalid};
};

template <typename SignalId, typename T, std::size_t Count>
class SignalStore final {
  static_assert(std::is_enum_v<SignalId>, "SignalId must be an enum type");

 public:
  void publish(SignalId id, T value, std::uint32_t timestamp_ms,
               SignalQuality quality = SignalQuality::kValid) noexcept {
    signals_[index(id)] = Signal<T>{value, timestamp_ms, quality};
  }

  [[nodiscard]] Signal<T> read(SignalId id, std::uint32_t now_ms,
                               std::uint32_t timeout_ms) const noexcept {
    Signal<T> result = signals_[index(id)];
    if ((result.quality == SignalQuality::kValid) &&
        (elapsed(now_ms, result.timestamp_ms) > timeout_ms)) {
      result.quality = SignalQuality::kStale;
    }
    return result;
  }

  void invalidate(SignalId id) noexcept {
    signals_[index(id)].quality = SignalQuality::kInvalid;
  }

  void mark_fault(SignalId id, std::uint32_t timestamp_ms) noexcept {
    auto& signal = signals_[index(id)];
    signal.timestamp_ms = timestamp_ms;
    signal.quality = SignalQuality::kFault;
  }

 private:
  static constexpr std::size_t index(SignalId id) noexcept {
    return static_cast<std::size_t>(id);
  }

  std::array<Signal<T>, Count> signals_{};
};

}  // namespace packcontroller::core

#endif
