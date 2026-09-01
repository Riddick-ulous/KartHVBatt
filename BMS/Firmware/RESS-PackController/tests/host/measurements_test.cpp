#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "packcontroller/app/measurements.hpp"

namespace packcontroller::app {
namespace {

constexpr MeasurementCalibration kCalibration{1500U, 3000U};

packcontroller_adc_block_t valid_block(std::uint32_t sequence = 1U,
                                       std::uint32_t timestamp_ms = 10U) {
  packcontroller_adc_block_t block{};
  for (std::size_t sample = 0U;
       sample < PACKCONTROLLER_ADC_SAMPLES_PER_BLOCK; ++sample) {
    for (std::size_t channel = 0U;
         channel < PACKCONTROLLER_ADC_CHANNEL_COUNT; ++channel) {
      block.raw[sample][channel] = 2048U;
    }
    block.raw[sample][PACKCONTROLLER_ADC_VREFINT] = 1500U;
  }
  block.sequence = sequence;
  block.timestamp_ms = timestamp_ms;
  block.coherent = true;
  return block;
}

packcontroller_adc_diagnostics_t valid_diagnostics() {
  packcontroller_adc_diagnostics_t diagnostics{};
  diagnostics.started = true;
  return diagnostics;
}

TEST(MeasurementPipeline, AllowsOneBlockStartupWindowBeforeInvalidating) {
  MeasurementPipeline pipeline{kCalibration};
  EXPECT_FALSE(pipeline.pipeline_invalid(kAdcBlockTimeoutMs));
  EXPECT_FALSE(pipeline.reference_invalid(kAdcBlockTimeoutMs));
  EXPECT_FALSE(
      pipeline.channel_invalid(PACKCONTROLLER_ADC_VACCU,
                               kAdcBlockTimeoutMs));
  EXPECT_TRUE(pipeline.pipeline_invalid(kAdcBlockTimeoutMs + 1U));
  EXPECT_TRUE(pipeline.reference_invalid(kAdcBlockTimeoutMs + 1U));
  EXPECT_TRUE(
      pipeline.channel_invalid(PACKCONTROLLER_ADC_VACCU,
                               kAdcBlockTimeoutMs + 1U));
}

TEST(MeasurementPipeline, ComputesRoundedBlockMeanAndPhysicalScaling) {
  MeasurementPipeline pipeline{kCalibration};
  auto block = valid_block();
  for (std::size_t sample = 0U;
       sample < PACKCONTROLLER_ADC_SAMPLES_PER_BLOCK; ++sample) {
    block.raw[sample][PACKCONTROLLER_ADC_RLEAK1] =
        static_cast<std::uint16_t>(1000U + sample);
  }
  block.raw[0U][PACKCONTROLLER_ADC_VBATT] = 1024U;
  for (std::size_t sample = 1U;
       sample < PACKCONTROLLER_ADC_SAMPLES_PER_BLOCK; ++sample) {
    block.raw[sample][PACKCONTROLLER_ADC_VBATT] = 1024U;
  }

  pipeline.process(block, valid_diagnostics());
  const auto snapshot = pipeline.snapshot(10U);

  EXPECT_TRUE(snapshot.coherent);
  EXPECT_EQ(snapshot.overall_quality, core::SignalQuality::kValid);
  EXPECT_EQ(snapshot.channels[PACKCONTROLLER_ADC_RLEAK1].raw_mean, 1005U);
  EXPECT_NEAR(snapshot.channels[PACKCONTROLLER_ADC_VREFINT].physical,
              3.0F, 0.001F);
  EXPECT_NEAR(snapshot.channels[PACKCONTROLLER_ADC_RLEAK1].physical,
              (1005.0F / 4095.0F) * 3.0F, 0.001F);
  EXPECT_NEAR(snapshot.channels[PACKCONTROLLER_ADC_VVEHI].physical,
              (2048.0F / 4095.0F) * 3.0F * 280.167F, 0.1F);
  EXPECT_NEAR(snapshot.channels[PACKCONTROLLER_ADC_VBATT].physical,
              (1024.0F / 4095.0F) * 3.0F * 5.54545F, 0.01F);
}

TEST(MeasurementPipeline, InterpolatesEatonNtcNearTwentyFiveDegrees) {
  MeasurementPipeline pipeline{kCalibration};
  pipeline.process(valid_block(), valid_diagnostics());
  const auto snapshot = pipeline.snapshot(10U);

  for (const auto channel : {PACKCONTROLLER_ADC_TNTC1,
                             PACKCONTROLLER_ADC_TNTC2,
                             PACKCONTROLLER_ADC_TNTC3,
                             PACKCONTROLLER_ADC_TNTC4,
                             PACKCONTROLLER_ADC_TNTC5}) {
    EXPECT_EQ(snapshot.channels[channel].quality,
              core::SignalQuality::kValid);
    EXPECT_NEAR(snapshot.channels[channel].physical, 25.0F, 0.1F);
  }
}

TEST(MeasurementPipeline, RejectsInvalidReferenceAndNtcEndpoints) {
  MeasurementPipeline pipeline{kCalibration};
  auto block = valid_block();
  for (std::size_t sample = 0U;
       sample < PACKCONTROLLER_ADC_SAMPLES_PER_BLOCK; ++sample) {
    block.raw[sample][PACKCONTROLLER_ADC_VREFINT] = 0U;
    block.raw[sample][PACKCONTROLLER_ADC_TNTC1] = 4095U;
  }
  pipeline.process(block, valid_diagnostics());

  const auto snapshot = pipeline.snapshot(10U);
  EXPECT_TRUE(pipeline.reference_invalid(10U));
  EXPECT_EQ(snapshot.overall_quality, core::SignalQuality::kFault);
  for (const auto& channel : snapshot.channels) {
    EXPECT_EQ(channel.quality, core::SignalQuality::kFault);
  }
}

TEST(MeasurementPipeline, MarksAdcFullScaleSaturationAsFault) {
  MeasurementPipeline pipeline{kCalibration};
  auto block = valid_block();
  for (std::size_t sample = 0U;
       sample < PACKCONTROLLER_ADC_SAMPLES_PER_BLOCK; ++sample) {
    block.raw[sample][PACKCONTROLLER_ADC_VBATT] = 4095U;
  }
  pipeline.process(block, valid_diagnostics());

  const auto snapshot = pipeline.snapshot(10U);
  EXPECT_EQ(snapshot.channels[PACKCONTROLLER_ADC_VBATT].quality,
            core::SignalQuality::kFault);
  EXPECT_TRUE(
      pipeline.channel_invalid(PACKCONTROLLER_ADC_VBATT, 10U));
}

TEST(MeasurementPipeline, MarksStaleDataInsteadOfKeepingItValid) {
  MeasurementPipeline pipeline{kCalibration};
  pipeline.process(valid_block(1U, 100U), valid_diagnostics());

  const auto fresh = pipeline.snapshot(120U);
  EXPECT_EQ(fresh.overall_quality, core::SignalQuality::kValid);
  const auto stale = pipeline.snapshot(121U);
  EXPECT_FALSE(stale.coherent);
  EXPECT_EQ(stale.overall_quality, core::SignalQuality::kStale);
  EXPECT_EQ(stale.channels[PACKCONTROLLER_ADC_VACCU].quality,
            core::SignalQuality::kStale);
  EXPECT_TRUE(pipeline.pipeline_invalid(121U));
}

TEST(MeasurementPipeline, DetectsAndRecoversFromFrameAndCounterErrors) {
  MeasurementPipeline pipeline{kCalibration};
  auto broken = valid_block(1U, 10U);
  broken.coherent = false;
  pipeline.process(broken, valid_diagnostics());
  EXPECT_TRUE(pipeline.pipeline_invalid(10U));
  EXPECT_EQ(pipeline.snapshot(10U).overall_quality,
            core::SignalQuality::kFault);

  pipeline.process(valid_block(2U, 20U), valid_diagnostics());
  EXPECT_FALSE(pipeline.pipeline_invalid(20U));
  EXPECT_EQ(pipeline.snapshot(20U).overall_quality,
            core::SignalQuality::kValid);

  auto diagnostics = valid_diagnostics();
  diagnostics.dma_error_count = 1U;
  pipeline.process(valid_block(3U, 30U), diagnostics);
  EXPECT_TRUE(pipeline.pipeline_invalid(30U));
  EXPECT_EQ(pipeline.snapshot(30U).dma_error_count, 1U);

  pipeline.process(valid_block(4U, 40U), diagnostics);
  EXPECT_FALSE(pipeline.pipeline_invalid(40U));
}

}  // namespace
}  // namespace packcontroller::app
