#include "packcontroller/platform/fdcan.h"

#include <stddef.h>
#include <string.h>

#include "main.h"
#include "pack_controller.h"
#include "packcontroller/platform/io.h"
#include "packcontroller/platform/runtime.h"

extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;

enum {
  RX_RING_CAPACITY = 16,
  TX_EVENT_RING_CAPACITY = 4,
  TX_PERIODIC_RING_CAPACITY = 8,
  FDCAN_TDC_OFFSET_FROM_DATA_TIMING = 14,
  FDCAN_TDC_FILTER = 0
};

typedef struct {
  packcontroller_can_frame_t frames[RX_RING_CAPACITY];
  volatile uint8_t head;
  volatile uint8_t tail;
} rx_ring_t;

typedef struct {
  packcontroller_can_frame_t frames[TX_EVENT_RING_CAPACITY];
  volatile uint8_t head;
  volatile uint8_t tail;
} tx_event_ring_t;

typedef struct {
  packcontroller_can_frame_t frames[TX_PERIODIC_RING_CAPACITY];
  volatile uint8_t head;
  volatile uint8_t tail;
} tx_periodic_ring_t;

typedef struct {
  rx_ring_t rx;
  tx_event_ring_t tx_event;
  tx_periodic_ring_t tx_periodic;
  volatile packcontroller_can_bus_diagnostics_t diagnostics;
} bus_context_t;

static bus_context_t bus_contexts[2];

static uint16_t saturating_increment_u16(uint16_t value)
{
  return value == UINT16_MAX ? value : (uint16_t)(value + 1U);
}

static uint8_t saturating_increment_u8(uint8_t value)
{
  return value == UINT8_MAX ? value : (uint8_t)(value + 1U);
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

static size_t context_index_from_handle(const FDCAN_HandleTypeDef *hfdcan)
{
  return hfdcan->Instance == FDCAN2 ? 1U : 0U;
}

static FDCAN_HandleTypeDef *handle_from_index(size_t index)
{
  return index == 1U ? &hfdcan2 : &hfdcan1;
}

static bool rx_push(bus_context_t *context,
                    const packcontroller_can_frame_t *frame)
{
  const uint8_t next = (uint8_t)((context->rx.head + 1U) % RX_RING_CAPACITY);
  if (next == context->rx.tail)
  {
    context->diagnostics.rx_dropped =
        saturating_increment_u16(context->diagnostics.rx_dropped);
    return false;
  }
  context->rx.frames[context->rx.head] = *frame;
  __DMB();
  context->rx.head = next;
  return true;
}

static bool rx_pop(bus_context_t *context, packcontroller_can_frame_t *frame)
{
  if (context->rx.tail == context->rx.head)
  {
    return false;
  }
  *frame = context->rx.frames[context->rx.tail];
  context->rx.tail =
      (uint8_t)((context->rx.tail + 1U) % RX_RING_CAPACITY);
  return true;
}

static bool tx_event_push(tx_event_ring_t *ring,
                          const packcontroller_can_frame_t *frame)
{
  const uint8_t next =
      (uint8_t)((ring->head + 1U) % TX_EVENT_RING_CAPACITY);
  if (next == ring->tail)
  {
    return false;
  }
  ring->frames[ring->head] = *frame;
  ring->head = next;
  return true;
}

static bool tx_periodic_push_or_replace(
    tx_periodic_ring_t *ring, const packcontroller_can_frame_t *frame)
{
  uint8_t cursor = ring->tail;
  while (cursor != ring->head)
  {
    if (ring->frames[cursor].identifier == frame->identifier)
    {
      ring->frames[cursor] = *frame;
      return true;
    }
    cursor = (uint8_t)((cursor + 1U) % TX_PERIODIC_RING_CAPACITY);
  }

  const uint8_t next =
      (uint8_t)((ring->head + 1U) % TX_PERIODIC_RING_CAPACITY);
  if (next == ring->tail)
  {
    return false;
  }
  ring->frames[ring->head] = *frame;
  ring->head = next;
  return true;
}

static bool tx_peek(bus_context_t *context,
                    packcontroller_can_frame_t **frame,
                    bool *from_event_ring)
{
  if (context->tx_event.tail != context->tx_event.head)
  {
    *frame = &context->tx_event.frames[context->tx_event.tail];
    *from_event_ring = true;
    return true;
  }
  if (context->tx_periodic.tail != context->tx_periodic.head)
  {
    *frame = &context->tx_periodic.frames[context->tx_periodic.tail];
    *from_event_ring = false;
    return true;
  }
  return false;
}

static void tx_pop(bus_context_t *context, bool from_event_ring)
{
  if (from_event_ring)
  {
    context->tx_event.tail = (uint8_t)(
        (context->tx_event.tail + 1U) % TX_EVENT_RING_CAPACITY);
  }
  else
  {
    context->tx_periodic.tail = (uint8_t)(
        (context->tx_periodic.tail + 1U) % TX_PERIODIC_RING_CAPACITY);
  }
}

static bool length_to_dlc(uint8_t length, uint32_t *dlc)
{
  static const uint32_t dlcs[16] = {
      FDCAN_DLC_BYTES_0,  FDCAN_DLC_BYTES_1,  FDCAN_DLC_BYTES_2,
      FDCAN_DLC_BYTES_3,  FDCAN_DLC_BYTES_4,  FDCAN_DLC_BYTES_5,
      FDCAN_DLC_BYTES_6,  FDCAN_DLC_BYTES_7,  FDCAN_DLC_BYTES_8,
      FDCAN_DLC_BYTES_12, FDCAN_DLC_BYTES_16, FDCAN_DLC_BYTES_20,
      FDCAN_DLC_BYTES_24, FDCAN_DLC_BYTES_32, FDCAN_DLC_BYTES_48,
      FDCAN_DLC_BYTES_64};
  static const uint8_t lengths[16] = {0U,  1U,  2U,  3U,  4U,  5U,
                                      6U,  7U,  8U,  12U, 16U, 20U,
                                      24U, 32U, 48U, 64U};
  size_t index;
  for (index = 0U; index < 16U; ++index)
  {
    if (length == lengths[index])
    {
      *dlc = dlcs[index];
      return true;
    }
  }
  return false;
}

static uint8_t dlc_to_length(uint32_t dlc)
{
  static const uint8_t lengths[16] = {0U,  1U,  2U,  3U,  4U,  5U,
                                      6U,  7U,  8U,  12U, 16U, 20U,
                                      24U, 32U, 48U, 64U};
  return lengths[dlc & 0x0FU];
}

static void drain_tx(size_t index)
{
  bus_context_t *const context = &bus_contexts[index];
  FDCAN_HandleTypeDef *const hfdcan = handle_from_index(index);
  while (HAL_FDCAN_GetTxFifoFreeLevel(hfdcan) > 0U)
  {
    packcontroller_can_frame_t *frame;
    bool from_event_ring;
    FDCAN_TxHeaderTypeDef header;
    uint32_t dlc;
    if (!tx_peek(context, &frame, &from_event_ring))
    {
      return;
    }
    if (!length_to_dlc(frame->length, &dlc))
    {
      context->diagnostics.tx_dropped =
          saturating_increment_u16(context->diagnostics.tx_dropped);
      tx_pop(context, from_event_ring);
      continue;
    }
    memset(&header, 0, sizeof(header));
    header.Identifier = frame->identifier;
    header.IdType = FDCAN_STANDARD_ID;
    header.TxFrameType = FDCAN_DATA_FRAME;
    header.DataLength = dlc;
    header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    header.BitRateSwitch = FDCAN_BRS_ON;
    header.FDFormat = FDCAN_FD_CAN;
    header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    if (HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &header, frame->data) !=
        HAL_OK)
    {
      return;
    }
    tx_pop(context, from_event_ring);
  }
}

static bool configure_and_start(FDCAN_HandleTypeDef *hfdcan)
{
  FDCAN_FilterTypeDef filter;
  const uint32_t line1_interrupts =
      FDCAN_IT_BUS_OFF | FDCAN_IT_ERROR_PASSIVE;
  const uint32_t notifications =
      FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO0_MESSAGE_LOST |
      FDCAN_IT_TX_COMPLETE | line1_interrupts;
  memset(&filter, 0, sizeof(filter));
  filter.IdType = FDCAN_STANDARD_ID;
  filter.FilterIndex = 0U;
  filter.FilterType = FDCAN_FILTER_DUAL;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1 = PACK_CONTROLLER_VCU_BMS_CONTROL_FRAME_ID;
  filter.FilterID2 = PACK_CONTROLLER_BMS_SERVICE_REQUEST_FRAME_ID;

  if ((HAL_FDCAN_ConfigFilter(hfdcan, &filter) != HAL_OK) ||
      (HAL_FDCAN_ConfigGlobalFilter(hfdcan, FDCAN_REJECT, FDCAN_REJECT,
                                    FDCAN_REJECT_REMOTE,
                                    FDCAN_REJECT_REMOTE) != HAL_OK) ||
      (HAL_FDCAN_ConfigTxDelayCompensation(
           hfdcan, FDCAN_TDC_OFFSET_FROM_DATA_TIMING, FDCAN_TDC_FILTER) !=
       HAL_OK) ||
      (HAL_FDCAN_EnableTxDelayCompensation(hfdcan) != HAL_OK) ||
      (HAL_FDCAN_ConfigInterruptLines(hfdcan, line1_interrupts,
                                      FDCAN_INTERRUPT_LINE1) != HAL_OK) ||
      (HAL_FDCAN_ActivateNotification(hfdcan, notifications,
                                      0xFFFFFFFFU) != HAL_OK) ||
      (HAL_FDCAN_Start(hfdcan) != HAL_OK))
  {
    return false;
  }
  return true;
}

bool packcontroller_platform_can_init(void)
{
  memset(bus_contexts, 0, sizeof(bus_contexts));
  if (!configure_and_start(&hfdcan1))
  {
    return false;
  }
  bus_contexts[0].diagnostics.started = true;
  if (!configure_and_start(&hfdcan2))
  {
    (void)HAL_FDCAN_Stop(&hfdcan1);
    bus_contexts[0].diagnostics.started = false;
    return false;
  }
  bus_contexts[1].diagnostics.started = true;
  return true;
}

bool packcontroller_platform_can_receive(packcontroller_can_frame_t *frame)
{
  uint32_t primask;
  bool received;
  if (frame == NULL)
  {
    return false;
  }
  primask = enter_critical();
  received = rx_pop(&bus_contexts[0], frame);
  if (!received)
  {
    received = rx_pop(&bus_contexts[1], frame);
  }
  leave_critical(primask);
  return received;
}

bool packcontroller_platform_can_transmit(
    const packcontroller_can_frame_t *frame)
{
  size_t index;
  bus_context_t *context;
  uint32_t primask;
  bool queued;
  if ((frame == NULL) ||
      ((frame->bus != (uint8_t)PACKCONTROLLER_CAN_BUS_1) &&
       (frame->bus != (uint8_t)PACKCONTROLLER_CAN_BUS_2)) ||
      frame->is_extended || !frame->is_fd || !frame->bit_rate_switch)
  {
    return false;
  }
  index = frame->bus == (uint8_t)PACKCONTROLLER_CAN_BUS_2 ? 1U : 0U;
  context = &bus_contexts[index];
  primask = enter_critical();
  queued = frame->high_priority
               ? tx_event_push(&context->tx_event, frame)
               : tx_periodic_push_or_replace(&context->tx_periodic, frame);
  if (!queued)
  {
    context->diagnostics.tx_dropped =
        saturating_increment_u16(context->diagnostics.tx_dropped);
  }
  else
  {
    drain_tx(index);
  }
  leave_critical(primask);
  return queued;
}

void packcontroller_platform_can_service(void)
{
  size_t index;
  uint32_t primask = enter_critical();
  for (index = 0U; index < 2U; ++index)
  {
    FDCAN_ProtocolStatusTypeDef status;
    FDCAN_HandleTypeDef *const hfdcan = handle_from_index(index);
    if (HAL_FDCAN_GetProtocolStatus(hfdcan, &status) == HAL_OK)
    {
      bus_contexts[index].diagnostics.error_passive =
          status.ErrorPassive != 0U;
      if ((status.BusOff != 0U) || bus_contexts[index].diagnostics.bus_off)
      {
        CLEAR_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_INIT);
      }
      bus_contexts[index].diagnostics.bus_off = status.BusOff != 0U;
    }
    drain_tx(index);
  }
  leave_critical(primask);
}

packcontroller_can_bus_diagnostics_t packcontroller_platform_can_diagnostics(
    packcontroller_can_bus_t bus)
{
  packcontroller_can_bus_diagnostics_t result;
  const size_t index = bus == PACKCONTROLLER_CAN_BUS_2 ? 1U : 0U;
  const uint32_t primask = enter_critical();
  result = bus_contexts[index].diagnostics;
  leave_critical(primask);
  return result;
}

packcontroller_safety_inputs_t packcontroller_platform_read_safety_inputs(void)
{
  packcontroller_safety_inputs_t inputs;
  inputs.danger_voltage_clear_n =
      (nDangerV_GPIO_Port->IDR & nDangerV_Pin) != 0U;
  inputs.por_state_n = (nPOR_State_GPIO_Port->IDR & nPOR_State_Pin) != 0U;
  inputs.sc_latched = (SC_Latched_GPIO_Port->IDR & SC_Latched_Pin) != 0U;
  inputs.precharge_actual =
      (PCHRG_ACTUAL_GPIO_Port->IDR & PCHRG_ACTUAL_Pin) != 0U;
  inputs.air_p_actual =
      (AIR_P_Actual_GPIO_Port->IDR & AIR_P_Actual_Pin) != 0U;
  inputs.air_n_actual =
      (AIR_N_Actual_GPIO_Port->IDR & AIR_N_Actual_Pin) != 0U;
  inputs.dcdc_actual =
      (DCDC_AIR_ACTUAL_GPIO_Port->IDR & DCDC_AIR_ACTUAL_Pin) != 0U;
  return inputs;
}

void packcontroller_platform_commit_switch_outputs(
    packcontroller_switch_outputs_t outputs)
{
  uint32_t set_d = 0U;
  uint32_t reset_d = 0U;
  if (outputs.air_n)
  {
    set_d |= AIR_N_Switch_Pin;
  }
  else
  {
    reset_d |= AIR_N_Switch_Pin;
  }
  if (outputs.precharge)
  {
    set_d |= PCHRG_SWITCH_Pin;
  }
  else
  {
    reset_d |= PCHRG_SWITCH_Pin;
  }
  if (outputs.air_p)
  {
    set_d |= AIR_P_Switch_Pin;
  }
  else
  {
    reset_d |= AIR_P_Switch_Pin;
  }
  GPIOD->BSRR = set_d | (reset_d << 16U);
  GPIOC->BSRR = outputs.dcdc ? DCDC_AIR_SWITCH_Pin
                             : ((uint32_t)DCDC_AIR_SWITCH_Pin << 16U);
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t rx_fifo0_interrupts)
{
  const size_t index = context_index_from_handle(hfdcan);
  bus_context_t *const context = &bus_contexts[index];
  if ((rx_fifo0_interrupts & FDCAN_IT_RX_FIFO0_MESSAGE_LOST) != 0U)
  {
    context->diagnostics.rx_dropped =
        saturating_increment_u16(context->diagnostics.rx_dropped);
  }
  while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0U)
  {
    FDCAN_RxHeaderTypeDef header;
    packcontroller_can_frame_t frame;
    memset(&header, 0, sizeof(header));
    memset(&frame, 0, sizeof(frame));
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &header, frame.data) !=
        HAL_OK)
    {
      context->diagnostics.rx_dropped =
          saturating_increment_u16(context->diagnostics.rx_dropped);
      return;
    }
    frame.identifier = header.Identifier;
    frame.timestamp_us = packcontroller_platform_time_us();
    frame.length = dlc_to_length(header.DataLength);
    frame.bus = (uint8_t)(index + 1U);
    frame.is_fd = header.FDFormat == FDCAN_FD_CAN;
    frame.bit_rate_switch = header.BitRateSwitch == FDCAN_BRS_ON;
    frame.is_extended = header.IdType == FDCAN_EXTENDED_ID;
    (void)rx_push(context, &frame);
  }
}

void HAL_FDCAN_TxBufferCompleteCallback(FDCAN_HandleTypeDef *hfdcan,
                                        uint32_t buffer_indexes)
{
  (void)buffer_indexes;
  drain_tx(context_index_from_handle(hfdcan));
}

void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hfdcan,
                                   uint32_t error_status_interrupts)
{
  bus_context_t *const context =
      &bus_contexts[context_index_from_handle(hfdcan)];
  if ((error_status_interrupts & FDCAN_IT_BUS_OFF) != 0U)
  {
    if (!context->diagnostics.bus_off)
    {
      context->diagnostics.bus_off_count =
          saturating_increment_u8(context->diagnostics.bus_off_count);
    }
    context->diagnostics.bus_off = true;
  }
  if ((error_status_interrupts & FDCAN_IT_ERROR_PASSIVE) != 0U)
  {
    FDCAN_ProtocolStatusTypeDef status;
    if (HAL_FDCAN_GetProtocolStatus(hfdcan, &status) == HAL_OK)
    {
      context->diagnostics.error_passive = status.ErrorPassive != 0U;
    }
  }
}
