#ifndef PACKCONTROLLER_APP_MEASUREMENTS_HPP
#define PACKCONTROLLER_APP_MEASUREMENTS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "packcontroller/core/signals.hpp"
#include "packcontroller/platform/adc.h"

namespace packcontroller::app {

constexpr std::uint32_t kAdcBlockTimeoutMs = 20U;

struct MeasurementCalibration final {
  std::uint16_t vrefint_calibration_raw{0U};
  std::uint16_t vrefint_calibration_mv{3000U};
  float hv_gain{280.167F};
  float vbatt_gain{5.54545F};
  std::array<float, 3U> hv_offset_v{};  // VVEHI, VACCU, VDCDC
  float vbatt_offset_v{0.0F};
};

struct AnalogChannelValue final {
  std::uint16_t raw_mean{0U};
  float physical{0.0F};
  core::SignalQuality quality{core::SignalQuality::kInvalid};
};

struct MeasurementSnapshot final {
  std::array<AnalogChannelValue, PACKCONTROLLER_ADC_CHANNEL_COUNT> channels{};
  std::uint32_t sample_counter{0U};
  std::uint32_t timestamp_ms{0U};
  std::uint16_t dma_error_count{0U};
  std::uint16_t dropped_block_count{0U};
  bool coherent{false};
  core::SignalQuality overall_quality{core::SignalQuality::kInvalid};
};

class MeasurementPipeline final {
 public:
  explicit MeasurementPipeline(MeasurementCalibration calibration) noexcept
      : calibration_(calibration) {}

  void set_calibration(MeasurementCalibration calibration) noexcept {
    calibration_ = calibration;
  }

  void process(const packcontroller_adc_block_t& block,
               packcontroller_adc_diagnostics_t diagnostics) noexcept;

  [[nodiscard]] MeasurementSnapshot snapshot(
      std::uint32_t now_ms) const noexcept;
  [[nodiscard]] bool pipeline_invalid(std::uint32_t now_ms) const noexcept;
  [[nodiscard]] bool reference_invalid(std::uint32_t now_ms) const noexcept;
  [[nodiscard]] bool channel_invalid(packcontroller_adc_channel_t channel,
                                     std::uint32_t now_ms) const noexcept;

 private:
  static constexpr std::size_t index(
      packcontroller_adc_channel_t channel) noexcept {
    return static_cast<std::size_t>(channel);
  }

  MeasurementCalibration calibration_{};
  MeasurementSnapshot snapshot_{};
  std::uint32_t previous_sequence_{0U};
  std::uint16_t previous_error_count_{0U};
  std::uint16_t previous_dropped_count_{0U};
  bool initialized_{false};
  bool pipeline_fault_active_{false};
};

}  // namespace packcontroller::app

#endif
