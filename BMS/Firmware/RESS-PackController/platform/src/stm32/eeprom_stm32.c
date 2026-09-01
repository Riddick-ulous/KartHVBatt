#include "packcontroller/platform/eeprom.h"

#include <limits.h>
#include <stddef.h>

#include "main.h"

extern I2C_HandleTypeDef hi2c2;

typedef enum {
  EEPROM_OPERATION_NONE = 0,
  EEPROM_OPERATION_READ,
  EEPROM_OPERATION_WRITE,
  EEPROM_OPERATION_ACK_POLL,
} eeprom_operation_t;

static volatile packcontroller_eeprom_io_status_t g_status =
    PACKCONTROLLER_EEPROM_IO_IDLE;
static volatile eeprom_operation_t g_operation = EEPROM_OPERATION_NONE;
static packcontroller_eeprom_diagnostics_t g_diagnostics = {
    0U, 0U, 0U, 0U, true};
static uint8_t g_ack_probe_byte = 0U;

static uint16_t saturating_increment(uint16_t value)
{
  return value == UINT16_MAX ? UINT16_MAX : (uint16_t)(value + 1U);
}

static bool range_valid(uint16_t address, uint16_t length)
{
  return (length > 0U) &&
         (((uint32_t)address + (uint32_t)length) <=
          PACKCONTROLLER_EEPROM_SIZE_BYTES);
}

bool packcontroller_platform_eeprom_init(void)
{
  packcontroller_platform_eeprom_set_write_protected(true);
  g_status = PACKCONTROLLER_EEPROM_IO_IDLE;
  g_operation = EEPROM_OPERATION_NONE;
  return hi2c2.Instance == I2C2;
}

bool packcontroller_platform_eeprom_start_read(uint16_t address,
                                               uint8_t *data,
                                               uint16_t length)
{
  if ((g_status == PACKCONTROLLER_EEPROM_IO_BUSY) || (data == NULL) ||
      !range_valid(address, length)) {
    return false;
  }
  g_operation = EEPROM_OPERATION_READ;
  g_status = PACKCONTROLLER_EEPROM_IO_BUSY;
  if (HAL_I2C_Mem_Read_IT(&hi2c2,
                          (uint16_t)(PACKCONTROLLER_EEPROM_I2C_ADDRESS_7BIT
                                     << 1U),
                          address, I2C_MEMADD_SIZE_16BIT, data, length) !=
      HAL_OK) {
    g_operation = EEPROM_OPERATION_NONE;
    g_status = PACKCONTROLLER_EEPROM_IO_ERROR;
    g_diagnostics.communication_error_count =
        saturating_increment(g_diagnostics.communication_error_count);
    return false;
  }
  return true;
}

bool packcontroller_platform_eeprom_start_page_write(uint16_t address,
                                                     const uint8_t *data,
                                                     uint8_t length)
{
  const uint16_t page_offset =
      (uint16_t)(address % PACKCONTROLLER_EEPROM_PAGE_SIZE);
  if ((g_status == PACKCONTROLLER_EEPROM_IO_BUSY) || (data == NULL) ||
      !range_valid(address, length) || (length == 0U) ||
      (length > PACKCONTROLLER_EEPROM_PAGE_SIZE) ||
      ((uint16_t)(page_offset + length) >
       PACKCONTROLLER_EEPROM_PAGE_SIZE)) {
    return false;
  }
  g_operation = EEPROM_OPERATION_WRITE;
  g_status = PACKCONTROLLER_EEPROM_IO_BUSY;
  if (HAL_I2C_Mem_Write_IT(&hi2c2,
                           (uint16_t)(PACKCONTROLLER_EEPROM_I2C_ADDRESS_7BIT
                                      << 1U),
                           address, I2C_MEMADD_SIZE_16BIT,
                           (uint8_t *)(uintptr_t)data, length) != HAL_OK) {
    g_operation = EEPROM_OPERATION_NONE;
    g_status = PACKCONTROLLER_EEPROM_IO_ERROR;
    g_diagnostics.communication_error_count =
        saturating_increment(g_diagnostics.communication_error_count);
    return false;
  }
  return true;
}

bool packcontroller_platform_eeprom_start_ack_poll(void)
{
  if (g_status == PACKCONTROLLER_EEPROM_IO_BUSY) {
    return false;
  }
  g_diagnostics.ack_poll_count =
      saturating_increment(g_diagnostics.ack_poll_count);
  g_operation = EEPROM_OPERATION_ACK_POLL;
  g_status = PACKCONTROLLER_EEPROM_IO_BUSY;
  if (HAL_I2C_Master_Transmit_IT(
          &hi2c2,
          (uint16_t)(PACKCONTROLLER_EEPROM_I2C_ADDRESS_7BIT << 1U),
          &g_ack_probe_byte, 0U) != HAL_OK) {
    g_operation = EEPROM_OPERATION_NONE;
    g_status = PACKCONTROLLER_EEPROM_IO_ERROR;
    g_diagnostics.communication_error_count =
        saturating_increment(g_diagnostics.communication_error_count);
    return false;
  }
  return true;
}

packcontroller_eeprom_io_status_t packcontroller_platform_eeprom_status(void)
{
  return g_status;
}

void packcontroller_platform_eeprom_clear_result(void)
{
  if (g_status != PACKCONTROLLER_EEPROM_IO_BUSY) {
    g_status = PACKCONTROLLER_EEPROM_IO_IDLE;
    g_operation = EEPROM_OPERATION_NONE;
  }
}

void packcontroller_platform_eeprom_set_write_protected(bool enabled)
{
  WP_GPIO_Port->BSRR = enabled ? WP_Pin : ((uint32_t)WP_Pin << 16U);
  g_diagnostics.write_protected = enabled;
}

packcontroller_eeprom_diagnostics_t
packcontroller_platform_eeprom_diagnostics(void)
{
  return g_diagnostics;
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if ((hi2c != NULL) && (hi2c->Instance == I2C2) &&
      (g_operation == EEPROM_OPERATION_READ)) {
    g_diagnostics.completed_read_count =
        saturating_increment(g_diagnostics.completed_read_count);
    g_operation = EEPROM_OPERATION_NONE;
    g_status = PACKCONTROLLER_EEPROM_IO_COMPLETE;
  }
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if ((hi2c != NULL) && (hi2c->Instance == I2C2) &&
      (g_operation == EEPROM_OPERATION_WRITE)) {
    g_diagnostics.completed_write_count =
        saturating_increment(g_diagnostics.completed_write_count);
    g_operation = EEPROM_OPERATION_NONE;
    g_status = PACKCONTROLLER_EEPROM_IO_COMPLETE;
  }
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if ((hi2c != NULL) && (hi2c->Instance == I2C2) &&
      (g_operation == EEPROM_OPERATION_ACK_POLL)) {
    g_operation = EEPROM_OPERATION_NONE;
    g_status = PACKCONTROLLER_EEPROM_IO_COMPLETE;
  }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
  if ((hi2c != NULL) && (hi2c->Instance == I2C2) &&
      (g_status == PACKCONTROLLER_EEPROM_IO_BUSY)) {
    const bool expected_ack_nack =
        (g_operation == EEPROM_OPERATION_ACK_POLL) &&
        ((HAL_I2C_GetError(hi2c) & HAL_I2C_ERROR_AF) != 0U);
    g_operation = EEPROM_OPERATION_NONE;
    if (expected_ack_nack) {
      g_status = PACKCONTROLLER_EEPROM_IO_NOT_READY;
    } else {
      g_diagnostics.communication_error_count =
          saturating_increment(g_diagnostics.communication_error_count);
      g_status = PACKCONTROLLER_EEPROM_IO_ERROR;
    }
  }
}
