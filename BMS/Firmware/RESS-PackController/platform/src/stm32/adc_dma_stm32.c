#include "packcontroller/platform/adc.h"

#include <stddef.h>
#include <string.h>

#include "main.h"

enum {
  ADC_INSTANCE_COUNT = 5,
  ADC_ALL_READY_MASK = 0x1F,
  ADC_QUEUE_CAPACITY = 4,
  ADC1_RANK_COUNT = 4,
  ADC2_RANK_COUNT = 3,
  ADC3_RANK_COUNT = 3,
  ADC4_RANK_COUNT = 1,
  ADC5_RANK_COUNT = 1,
  ADC1_DMA_COUNT = 80,
  ADC2_DMA_COUNT = 60,
  ADC3_DMA_COUNT = 60,
  ADC4_DMA_COUNT = 20,
  ADC5_DMA_COUNT = 20,
  ADC_VREFINT_CALIBRATION_MV = 3000
};

#define ADC_VREFINT_CALIBRATION_ADDRESS (0x1FFF75AAUL)

typedef struct {
  packcontroller_adc_block_t block;
  uint32_t generation;
  uint8_t ready_mask;
} adc_assembly_t;

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;
extern ADC_HandleTypeDef hadc4;
extern ADC_HandleTypeDef hadc5;
extern TIM_HandleTypeDef htim6;

static uint16_t adc1_dma[ADC1_DMA_COUNT];
static uint16_t adc2_dma[ADC2_DMA_COUNT];
static uint16_t adc3_dma[ADC3_DMA_COUNT];
static uint16_t adc4_dma[ADC4_DMA_COUNT];
static uint16_t adc5_dma[ADC5_DMA_COUNT];

static adc_assembly_t assemblies[2];
static uint32_t completion_count[ADC_INSTANCE_COUNT];
static packcontroller_adc_block_t queue[ADC_QUEUE_CAPACITY];
static volatile uint8_t queue_head;
static volatile uint8_t queue_tail;
static volatile packcontroller_adc_diagnostics_t diagnostics;

static uint16_t saturating_increment_u16(uint16_t value)
{
  return value == UINT16_MAX ? value : (uint16_t)(value + 1U);
}

static uint32_t enter_critical(void)
{
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

static void leave_critical(uint32_t primask)
{
  if (primask == 0U)
  {
    __enable_irq();
  }
}

static bool queue_push(const packcontroller_adc_block_t *block)
{
  const uint8_t next = (uint8_t)((queue_head + 1U) % ADC_QUEUE_CAPACITY);
  if (next == queue_tail)
  {
    diagnostics.dropped_block_count =
        saturating_increment_u16(diagnostics.dropped_block_count);
    return false;
  }
  queue[queue_head] = *block;
  queue_head = next;
  return true;
}

static int adc_index(const ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1)
  {
    return 0;
  }
  if (hadc->Instance == ADC2)
  {
    return 1;
  }
  if (hadc->Instance == ADC3)
  {
    return 2;
  }
  if (hadc->Instance == ADC4)
  {
    return 3;
  }
  if (hadc->Instance == ADC5)
  {
    return 4;
  }
  return -1;
}

static void copy_ranked_samples(adc_assembly_t *assembly,
                                const uint16_t *source,
                                size_t rank_count,
                                const packcontroller_adc_channel_t *channels)
{
  size_t sample;
  size_t rank;
  for (sample = 0U; sample < PACKCONTROLLER_ADC_SAMPLES_PER_BLOCK; ++sample)
  {
    for (rank = 0U; rank < rank_count; ++rank)
    {
      assembly->block.raw[sample][channels[rank]] =
          source[(sample * rank_count) + rank];
    }
  }
}

static void copy_adc_samples(adc_assembly_t *assembly,
                             int index,
                             uint8_t half)
{
  static const packcontroller_adc_channel_t adc1_channels[ADC1_RANK_COUNT] = {
      PACKCONTROLLER_ADC_RLEAK1, PACKCONTROLLER_ADC_VVEHI,
      PACKCONTROLLER_ADC_TNTC4, PACKCONTROLLER_ADC_VREFINT};
  static const packcontroller_adc_channel_t adc2_channels[ADC2_RANK_COUNT] = {
      PACKCONTROLLER_ADC_RLEAK2, PACKCONTROLLER_ADC_TNTC1,
      PACKCONTROLLER_ADC_TNTC5};
  static const packcontroller_adc_channel_t adc3_channels[ADC3_RANK_COUNT] = {
      PACKCONTROLLER_ADC_VBATT, PACKCONTROLLER_ADC_VACCU,
      PACKCONTROLLER_ADC_VDCDC};
  static const packcontroller_adc_channel_t adc4_channels[ADC4_RANK_COUNT] = {
      PACKCONTROLLER_ADC_TNTC3};
  static const packcontroller_adc_channel_t adc5_channels[ADC5_RANK_COUNT] = {
      PACKCONTROLLER_ADC_TNTC2};

  switch (index)
  {
    case 0:
      copy_ranked_samples(assembly, &adc1_dma[(size_t)half * 40U],
                          ADC1_RANK_COUNT, adc1_channels);
      break;
    case 1:
      copy_ranked_samples(assembly, &adc2_dma[(size_t)half * 30U],
                          ADC2_RANK_COUNT, adc2_channels);
      break;
    case 2:
      copy_ranked_samples(assembly, &adc3_dma[(size_t)half * 30U],
                          ADC3_RANK_COUNT, adc3_channels);
      break;
    case 3:
      copy_ranked_samples(assembly, &adc4_dma[(size_t)half * 10U],
                          ADC4_RANK_COUNT, adc4_channels);
      break;
    case 4:
      copy_ranked_samples(assembly, &adc5_dma[(size_t)half * 10U],
                          ADC5_RANK_COUNT, adc5_channels);
      break;
    default:
      break;
  }
}

static void adc_completed(ADC_HandleTypeDef *hadc, uint8_t half)
{
  const int index = adc_index(hadc);
  adc_assembly_t *assembly;
  uint32_t generation;
  uint8_t bit;
  if (index < 0)
  {
    return;
  }

  generation = ++completion_count[(size_t)index];
  if (((generation - 1U) & 1U) != (uint32_t)half)
  {
    diagnostics.frame_error_count =
        saturating_increment_u16(diagnostics.frame_error_count);
  }

  assembly = &assemblies[half];
  bit = (uint8_t)(1U << (uint32_t)index);
  if ((assembly->ready_mask == 0U) || (assembly->generation != generation))
  {
    if (assembly->ready_mask != 0U)
    {
      diagnostics.frame_error_count =
          saturating_increment_u16(diagnostics.frame_error_count);
    }
    assembly->ready_mask = 0U;
    assembly->generation = generation;
    assembly->block.coherent = false;
  }
  if ((assembly->ready_mask & bit) != 0U)
  {
    diagnostics.frame_error_count =
        saturating_increment_u16(diagnostics.frame_error_count);
    assembly->ready_mask = 0U;
  }

  copy_adc_samples(assembly, index, half);
  assembly->ready_mask = (uint8_t)(assembly->ready_mask | bit);
  if (assembly->ready_mask == ADC_ALL_READY_MASK)
  {
    assembly->block.sequence = generation;
    assembly->block.timestamp_ms = TIM5->CNT / 1000U;
    assembly->block.coherent = true;
    (void)queue_push(&assembly->block);
    assembly->ready_mask = 0U;
  }
}

static void stop_adc_acquisition(void)
{
  (void)HAL_TIM_Base_Stop(&htim6);
  (void)HAL_ADC_Stop_DMA(&hadc1);
  (void)HAL_ADC_Stop_DMA(&hadc2);
  (void)HAL_ADC_Stop_DMA(&hadc3);
  (void)HAL_ADC_Stop_DMA(&hadc4);
  (void)HAL_ADC_Stop_DMA(&hadc5);
  diagnostics.started = false;
}

bool packcontroller_platform_adc_init(void)
{
  memset(assemblies, 0, sizeof(assemblies));
  memset(completion_count, 0, sizeof(completion_count));
  queue_head = 0U;
  queue_tail = 0U;
  diagnostics.dma_error_count = 0U;
  diagnostics.dropped_block_count = 0U;
  diagnostics.frame_error_count = 0U;
  diagnostics.started = false;

  if ((HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) ||
      (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK) ||
      (HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED) != HAL_OK) ||
      (HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED) != HAL_OK) ||
      (HAL_ADCEx_Calibration_Start(&hadc5, ADC_SINGLE_ENDED) != HAL_OK))
  {
    return false;
  }

  if ((HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc1_dma, ADC1_DMA_COUNT) != HAL_OK) ||
      (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_dma, ADC2_DMA_COUNT) != HAL_OK) ||
      (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adc3_dma, ADC3_DMA_COUNT) != HAL_OK) ||
      (HAL_ADC_Start_DMA(&hadc4, (uint32_t *)adc4_dma, ADC4_DMA_COUNT) != HAL_OK) ||
      (HAL_ADC_Start_DMA(&hadc5, (uint32_t *)adc5_dma, ADC5_DMA_COUNT) != HAL_OK) ||
      (HAL_TIM_Base_Start(&htim6) != HAL_OK))
  {
    stop_adc_acquisition();
    return false;
  }

  diagnostics.started = true;
  return true;
}

bool packcontroller_platform_adc_receive(packcontroller_adc_block_t *block)
{
  uint32_t primask;
  if (block == NULL)
  {
    return false;
  }
  primask = enter_critical();
  if (queue_tail == queue_head)
  {
    leave_critical(primask);
    return false;
  }
  *block = queue[queue_tail];
  queue_tail = (uint8_t)((queue_tail + 1U) % ADC_QUEUE_CAPACITY);
  leave_critical(primask);
  return true;
}

packcontroller_adc_diagnostics_t packcontroller_platform_adc_diagnostics(void)
{
  packcontroller_adc_diagnostics_t result;
  const uint32_t primask = enter_critical();
  result.dma_error_count = diagnostics.dma_error_count;
  result.dropped_block_count = diagnostics.dropped_block_count;
  result.frame_error_count = diagnostics.frame_error_count;
  result.started = diagnostics.started;
  leave_critical(primask);
  return result;
}

uint16_t packcontroller_platform_adc_vref_calibration_raw(void)
{
  const volatile uint16_t *const calibration =
      (const volatile uint16_t *)ADC_VREFINT_CALIBRATION_ADDRESS;
  return *calibration;
}

uint16_t packcontroller_platform_adc_vref_calibration_mv(void)
{
  return (uint16_t)ADC_VREFINT_CALIBRATION_MV;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  adc_completed(hadc, 0U);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  adc_completed(hadc, 1U);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
  (void)hadc;
  diagnostics.dma_error_count =
      saturating_increment_u16(diagnostics.dma_error_count);
  assemblies[0].ready_mask = 0U;
  assemblies[1].ready_mask = 0U;
}
