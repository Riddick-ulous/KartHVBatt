#ifndef PACKCONTROLLER_CORE_FAULTS_HPP
#define PACKCONTROLLER_CORE_FAULTS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace packcontroller::core {

enum class FaultId : std::uint8_t {
  kNone = 0U,
  kStmHardfault = 1U,
  kClockFailure = 2U,
  kSchedulerHealthLoss = 3U,
  kSchedulerTaskOverrun = 4U,
  kWatchdogResetDetected = 5U,
  kUnexpectedResetCause = 6U,
  kPlatformInitFailed = 7U,
  kConfigInvalid = 8U,
  kCellProfileInvalid = 9U,
  kImdTypeUnset = 10U,
  kEepromCommunication = 11U,
  kEepromRecordCrc = 12U,
  kEepromWriteVerify = 13U,
  kEepromSelftestFailed = 14U,
  kNvmSchemaMismatch = 15U,
  kCan1BusOff = 16U,
  kCan2BusOff = 17U,
  kCan1CommandStale = 18U,
  kCan2CommandStale = 19U,
  kCanCommandLoss = 20U,
  kCanCounterDiscontinuity = 21U,
  kCanRxOverflow = 22U,
  kCanTxOverflow = 23U,
  kAirErrorActive = 24U,
  kPorStateInvalid = 25U,
  kScLatched = 26U,
  kAirNIntendedMismatch = 27U,
  kAirPIntendedMismatch = 28U,
  kPchargeActualTimeout = 29U,
  kAirPCloseTimeout = 30U,
  kPrechargeDoneTimeout = 31U,
  kPrechargeVoltageTimeout = 32U,
  kAirNStuckClosed = 33U,
  kPchargeStuckClosed = 34U,
  kAirPStuckClosed = 35U,
  kDcdcActualTimeout = 36U,
  kDcdcVoltageTimeout = 37U,
  kDcdcStuckClosed = 38U,
  kPackVoltagePlausibility = 39U,
  kDangerVFalseNegative = 40U,
  kDangerVFalsePositive = 41U,
  kTsalUnsafeGreen = 42U,
  kTsalConservativeRed = 43U,
  kContactorSequenceIllegal = 44U,
  kActuatorOutputConflict = 45U,
  kHvMeasurementStale = 46U,
  kSafeOpenFeedbackFailed = 47U,
  kAdcPipelineInvalid = 48U,
  kAdcReferenceInvalid = 49U,
  kVaccuMeasInvalid = 50U,
  kVvehiMeasInvalid = 51U,
  kVdcdcMeasInvalid = 52U,
  kVbattMeasInvalid = 53U,
  kLeakageWarning = 54U,
  kLeakageDetected = 55U,
  kLeakageSevere = 56U,
  kImdNotReady = 57U,
  kImdConfigMismatch = 58U,
  kImdIsolationCritical = 59U,
  kImdSpeedStartBad = 60U,
  kImdDeviceError = 61U,
  kImdEarthFault = 62U,
  kImdSignalInvalid = 63U,
  kImdUndervoltage = 64U,
  kTleTransceiverComm = 65U,
  kTleSlaveCount = 66U,
  kTleRingDegraded = 67U,
  kTleStackCommLoss = 68U,
  kTleReferenceWarning = 69U,
  kTleReferenceFault = 70U,
  kTleCellDataStale = 71U,
  kTleTempDataStale = 72U,
  kNtcCoverageCritical = 73U,
  kNtcColdInhibit = 74U,
  kCellUndervoltage = 75U,
  kCellOvervoltage = 76U,
  kCellOvertemperature = 77U,
  kCellVoltageSpread = 78U,
  kBalancingDiagnostic = 79U,
  kPackCurrentInvalid = 80U,
  kInverterPowerInvalid = 81U,
  kPowerLimitViolation = 82U,
  kPowerPlausibility = 83U,
  kSocFallbackLow = 84U,
  kSocQualityInvalid = 85U,
  kDeveloperSessionLoss = 86U,
  kLastDefined = 86U,
};

enum class FaultSeverity : std::uint8_t {
  kNone = 0U,
  kWarning = 1U,
  kControlledCritical = 2U,
  kStmHardfault = 3U,
  kHvHardfault = 4U,
};

enum class FaultResetPolicy : std::uint8_t {
  kAutoClear = 0U,
  kCanResettable = 1U,
  kPowerCycle = 2U,
};

struct Fault final {
  FaultId id{FaultId::kNone};
  FaultSeverity severity{FaultSeverity::kNone};
  FaultResetPolicy reset{FaultResetPolicy::kAutoClear};
  bool active{false};
  bool latched{false};
  std::uint32_t first_seen_ms{0U};
  std::uint32_t last_seen_ms{0U};
  std::uint16_t occurrence_count{0U};
};

struct FaultBitmap final {
  std::uint64_t low{0U};
  std::uint64_t high{0U};
};

class FaultManager final {
 public:
  static constexpr std::size_t kFaultCount = 128U;

  void update(FaultId id, FaultSeverity severity, FaultResetPolicy reset,
              bool condition_active, std::uint32_t now_ms) noexcept;
  bool clear_can_latch(FaultId id) noexcept;

  [[nodiscard]] const Fault& get(FaultId id) const noexcept;
  [[nodiscard]] bool critical_error_active() const noexcept;
  [[nodiscard]] bool hv_hardfault_active() const noexcept;
  [[nodiscard]] FaultBitmap active_bitmap() const noexcept;
  [[nodiscard]] FaultBitmap latched_bitmap() const noexcept;

 private:
  static constexpr std::size_t index(FaultId id) noexcept {
    return static_cast<std::size_t>(id);
  }

  std::array<Fault, kFaultCount> faults_{};
};

}  // namespace packcontroller::core

#endif
