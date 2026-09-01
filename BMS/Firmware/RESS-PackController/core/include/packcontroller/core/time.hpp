#ifndef PACKCONTROLLER_CORE_TIME_HPP
#define PACKCONTROLLER_CORE_TIME_HPP

#include <cstdint>

namespace packcontroller::core {

constexpr bool time_due(std::uint32_t now, std::uint32_t deadline) noexcept {
  return static_cast<std::int32_t>(now - deadline) >= 0;
}

constexpr std::uint32_t elapsed(std::uint32_t now,
                                std::uint32_t then) noexcept {
  return now - then;
}

}  // namespace packcontroller::core

#endif
