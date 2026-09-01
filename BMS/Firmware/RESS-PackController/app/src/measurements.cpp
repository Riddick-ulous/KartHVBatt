#include "packcontroller/app/measurements.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

#include "packcontroller/core/time.hpp"

namespace packcontroller::app {
namespace {

constexpr float kAdcMaximum = 4095.0F;
constexpr float kNtcPullupOhm = 10000.0F;

struct NtcPoint final {
  std::int16_t temperature_tenth_c;
  std::uint32_t resistance_ohm;
};

constexpr std::array<NtcPoint, 34U> kNtcLut{{
    {-400, 335500U}, {-350, 247690U}, {-300, 184110U}, {-250, 138480U},
    {-200, 104650U}, {-150, 79180U},  {-100, 60210U},  {-50, 46050U},
    {0, 35360U},     {50, 26960U},    {100, 20760U},   {150, 16130U},
    {200, 12650U},   {250, 10000U},   {300, 7970U},    {350, 6410U},
    {400, 5180U},    {450, 4221U},    {500, 3460U},    {550, 2845U},
    {600, 2351U},    {650, 1953U},    {700, 1629U},    {750, 1365U},
    {800, 1148U},    {850, 970U},     {900, 825U},     {950, 705U},
    {1000, 604U},    {1050, 520U},    {1100, 449U},    {1150, 388U},
    {1200, 337U},    {1250, 294U},
}};

std::uint16_t saturating_add(std::uint16_t left,
                             std::uint16_t right) noexcept {
  const auto maximum = std::numeric_limits<std::uint16_t>::max();
  if (right > static_cast<std::uint16_t>(maximum - left)) {
    return maximum;
  }
  return static_cast<std::uint16_t>(left + right);
}

std::uint16_t block_mean(const packcontroller_adc_block_t& block,
                         std::size_t channel) noexcept {
  std::uint32_t sum = 0U;
  for (std::size_t sample = 0U;
       sample < PACKCONTROLLER_ADC_SAMPLES_PER_BLOCK; ++sample) {
    sum += block.raw[sample][channel];
  }
  return static_cast<std::uint16_t>(
      (sum + (PACKCONTROLLER_ADC_SAMPLES_PER_BLOCK / 2U)) /
      PACKCONTROLLER_ADC_SAMPLES_PER_BLOCK);
}

bool raw_in_range(std::uint16_t raw) noexcept { return raw < 4095U; }

float adc_voltage(std::uint16_t raw, float vdda_v) noexcept {
  return (static_cast<float>(raw) / kAdcMaximum) * vdda_v;
}

bool ntc_temperature(std::uint16_t raw, float& temperature_c) noexcept {
  if ((raw == 0U) || (raw >= 4095U)) {
    return false;
  }
  const float ratio = static_cast<float>(raw) / kAdcMaximum;
  const float resistance = kNtcPullupOhm * ratio / (1.0F - ratio);
  if ((resistance > static_cast<float>(kNtcLut.front().resistance_ohm)) ||
      (resistance < static_cast<float>(kNtcLut.back().resistance_ohm))) {
    return false;
  }
  for (std::size_t index = 0U; index + 1U < kNtcLut.size(); ++index) {
    const float high = static_cast<float>(kNtcLut[index].resistance_ohm);
    const float low = static_cast<float>(kNtcLut[index + 1U].resistance_ohm);
    if ((resistance <= high) && (resistance >= low)) {
      const float fraction = (high - resistance) / (high - low);
      const float temperature_tenth =
          static_cast<float>(kNtcLut[index].temperature_tenth_c) +
          fraction * static_cast<float>(
                         kNtcLut[index + 1U].temperature_tenth_c -
                         kNtcLut[index].temperature_tenth_c);
      temperature_c = temperature_tenth * 0.1F;
      return true;
    }
  }
  return false;
}

bool is_ntc(std::size_t channel) noexcept {
  return (channel == PACKCONTROLLER_ADC_TNTC1) ||
         (channel == PACKCONTROLLER_ADC_TNTC2) ||
         (channel == PACKCONTROLLER_ADC_TNTC3) ||
         (channel == PACKCONTROLLER_ADC_TNTC4) ||
         (channel == PACKCONTROLLER_ADC_TNTC5);
}

core::SignalQuality combine_quality(
    const MeasurementSnapshot& snapshot) noexcept {
  core::SignalQuality result = core::SignalQuality::kValid;
  for (const auto& channel : snapshot.channels) {
    if (channel.quality == core::SignalQuality::kFault) {
      return core::SignalQuality::kFault;
    }
    if (channel.quality == core::SignalQuality::kInvalid) {
      result = core::SignalQuality::kInvalid;
    }
  }
  return result;
}

}  // namespace

void MeasurementPipeline::process(
    const packcontroller_adc_block_t& block,
    packcontroller_adc_diagnostics_t diagnostics) noexcept {
  const std::uint16_t combined_errors =
      saturating_add(diagnostics.dma_error_count,
                     diagnostics.frame_error_count);
  pipeline_fault_active_ =
      !diagnostics.started || !block.coherent ||
      (initialized_ && (block.sequence != (previous_sequence_ + 1U))) ||
      (combined_errors != previous_error_count_) ||
      (diagnostics.dropped_block_count != previous_dropped_count_);

  snapshot_.sample_counter = block.sequence;
  snapshot_.timestamp_ms = block.timestamp_ms;
  snapshot_.dma_error_count = combined_errors;
  snapshot_.dropped_block_count = diagnostics.dropped_block_count;
  snapshot_.coherent = block.coherent && !pipeline_fault_active_;

  for (std::size_t channel = 0U;
       channel < PACKCONTROLLER_ADC_CHANNEL_COUNT; ++channel) {
    auto& output = snapshot_.channels[channel];
    output.raw_mean = block_mean(block, channel);
    output.physical = 0.0F;
    output.quality = raw_in_range(output.raw_mean)
                         ? core::SignalQuality::kValid
                         : core::SignalQuality::kFault;
  }

  const auto vref_raw =
      snapshot_.channels[index(PACKCONTROLLER_ADC_VREFINT)].raw_mean;
  const bool reference_valid =
      (calibration_.vrefint_calibration_raw > 0U) &&
      (calibration_.vrefint_calibration_mv > 0U) && (vref_raw > 0U) &&
      (vref_raw < 4095U);
  float vdda_v = 0.0F;
  if (reference_valid) {
    const std::uint32_t numerator =
        static_cast<std::uint32_t>(calibration_.vrefint_calibration_raw) *
        calibration_.vrefint_calibration_mv;
    vdda_v = static_cast<float>(numerator) /
             (static_cast<float>(vref_raw) * 1000.0F);
    snapshot_.channels[index(PACKCONTROLLER_ADC_VREFINT)].physical = vdda_v;
  } else {
    for (auto& channel : snapshot_.channels) {
      channel.quality = core::SignalQuality::kFault;
    }
  }

  if (reference_valid) {
    auto set_voltage = [&](packcontroller_adc_channel_t channel, float gain,
                           float offset_v) noexcept {
      auto& output = snapshot_.channels[index(channel)];
      if (output.quality == core::SignalQuality::kValid) {
        output.physical =
            adc_voltage(output.raw_mean, vdda_v) * gain + offset_v;
      }
    };
    set_voltage(PACKCONTROLLER_ADC_RLEAK1, 1.0F, 0.0F);
    set_voltage(PACKCONTROLLER_ADC_VVEHI, calibration_.hv_gain,
                calibration_.hv_offset_v[0U]);
    set_voltage(PACKCONTROLLER_ADC_RLEAK2, 1.0F, 0.0F);
    set_voltage(PACKCONTROLLER_ADC_VBATT, calibration_.vbatt_gain,
                calibration_.vbatt_offset_v);
    set_voltage(PACKCONTROLLER_ADC_VACCU, calibration_.hv_gain,
                calibration_.hv_offset_v[1U]);
    set_voltage(PACKCONTROLLER_ADC_VDCDC, calibration_.hv_gain,
                calibration_.hv_offset_v[2U]);

    for (std::size_t channel = 0U;
         channel < PACKCONTROLLER_ADC_CHANNEL_COUNT; ++channel) {
      if (!is_ntc(channel)) {
        continue;
      }
      auto& output = snapshot_.channels[channel];
      if ((output.quality != core::SignalQuality::kValid) ||
          !ntc_temperature(output.raw_mean, output.physical)) {
        output.quality = core::SignalQuality::kFault;
        output.physical = 0.0F;
      }
    }
  }

  if (pipeline_fault_active_) {
    for (auto& channel : snapshot_.channels) {
      channel.quality = core::SignalQuality::kFault;
    }
  }
  snapshot_.overall_quality = combine_quality(snapshot_);
  previous_sequence_ = block.sequence;
  previous_error_count_ = combined_errors;
  previous_dropped_count_ = diagnostics.dropped_block_count;
  initialized_ = true;
}

MeasurementSnapshot MeasurementPipeline::snapshot(
    std::uint32_t now_ms) const noexcept {
  MeasurementSnapshot result = snapshot_;
  if (initialized_ &&
      (core::elapsed(now_ms, result.timestamp_ms) > kAdcBlockTimeoutMs)) {
    result.overall_quality = core::SignalQuality::kStale;
    result.coherent = false;
    for (auto& channel : result.channels) {
      if (channel.quality == core::SignalQuality::kValid) {
        channel.quality = core::SignalQuality::kStale;
      }
    }
  }
  return result;
}

bool MeasurementPipeline::pipeline_invalid(std::uint32_t now_ms) const noexcept {
  return pipeline_fault_active_ ||
         (!initialized_ && (now_ms > kAdcBlockTimeoutMs)) ||
         (initialized_ &&
          (core::elapsed(now_ms, snapshot_.timestamp_ms) > kAdcBlockTimeoutMs));
}

bool MeasurementPipeline::reference_invalid(std::uint32_t now_ms) const noexcept {
  if (!initialized_) {
    return now_ms > kAdcBlockTimeoutMs;
  }
  const auto current = snapshot(now_ms);
  return current.channels[index(PACKCONTROLLER_ADC_VREFINT)].quality !=
         core::SignalQuality::kValid;
}

bool MeasurementPipeline::channel_invalid(
    packcontroller_adc_channel_t channel, std::uint32_t now_ms) const noexcept {
  if (!initialized_) {
    return now_ms > kAdcBlockTimeoutMs;
  }
  return snapshot(now_ms).channels[index(channel)].quality !=
         core::SignalQuality::kValid;
}

}  // namespace packcontroller::app
