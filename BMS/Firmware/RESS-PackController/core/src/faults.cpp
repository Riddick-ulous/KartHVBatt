#include "packcontroller/core/faults.hpp"

#include <limits>

namespace packcontroller::core {
namespace {

void set_bitmap_bit(FaultBitmap& bitmap, std::size_t bit) noexcept {
  if (bit < 64U) {
    bitmap.low |= (std::uint64_t{1U} << bit);
  } else {
    bitmap.high |= (std::uint64_t{1U} << (bit - 64U));
  }
}

}  // namespace

void FaultManager::update(FaultId id, FaultSeverity severity,
                          FaultResetPolicy reset, bool condition_active,
                          std::uint32_t now_ms) noexcept {
  const std::size_t fault_index = index(id);
  if ((id == FaultId::kNone) || (fault_index >= faults_.size())) {
    return;
  }

  auto& fault = faults_[fault_index];
  fault.id = id;
  fault.severity = severity;
  fault.reset = reset;

  if (condition_active) {
    if (!fault.active) {
      fault.first_seen_ms = now_ms;
      if (fault.occurrence_count <
          std::numeric_limits<std::uint16_t>::max()) {
        ++fault.occurrence_count;
      }
    }
    fault.active = true;
    fault.latched = true;
    fault.last_seen_ms = now_ms;
    return;
  }

  fault.active = false;
  if (fault.reset == FaultResetPolicy::kAutoClear) {
    fault.latched = false;
  }
}

bool FaultManager::clear_can_latch(FaultId id) noexcept {
  const std::size_t fault_index = index(id);
  if ((id == FaultId::kNone) || (fault_index >= faults_.size())) {
    return false;
  }

  auto& fault = faults_[fault_index];
  if (fault.active || (fault.reset != FaultResetPolicy::kCanResettable)) {
    return false;
  }
  fault.latched = false;
  return true;
}

const Fault& FaultManager::get(FaultId id) const noexcept {
  return faults_[index(id)];
}

bool FaultManager::critical_error_active() const noexcept {
  for (const auto& fault : faults_) {
    if ((fault.active || fault.latched) &&
        (fault.severity >= FaultSeverity::kControlledCritical)) {
      return true;
    }
  }
  return false;
}

bool FaultManager::hv_hardfault_active() const noexcept {
  for (const auto& fault : faults_) {
    if ((fault.active || fault.latched) &&
        (fault.severity == FaultSeverity::kHvHardfault)) {
      return true;
    }
  }
  return false;
}

FaultBitmap FaultManager::active_bitmap() const noexcept {
  FaultBitmap bitmap{};
  for (std::size_t bit = 1U; bit < faults_.size(); ++bit) {
    if (faults_[bit].active) {
      set_bitmap_bit(bitmap, bit);
    }
  }
  return bitmap;
}

FaultBitmap FaultManager::latched_bitmap() const noexcept {
  FaultBitmap bitmap{};
  for (std::size_t bit = 1U; bit < faults_.size(); ++bit) {
    if (faults_[bit].latched) {
      set_bitmap_bit(bitmap, bit);
    }
  }
  return bitmap;
}

}  // namespace packcontroller::core
