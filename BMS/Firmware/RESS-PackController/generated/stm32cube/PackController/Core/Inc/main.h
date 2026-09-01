/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define Heartbeat_Pin GPIO_PIN_2
#define Heartbeat_GPIO_Port GPIOE
#define FANPWM_Pin GPIO_PIN_3
#define FANPWM_GPIO_Port GPIOE
#define ErrorLED_Pin GPIO_PIN_4
#define ErrorLED_GPIO_Port GPIOE
#define TSAL_GRN_ON_Pin GPIO_PIN_15
#define TSAL_GRN_ON_GPIO_Port GPIOC
#define DCDC_AIR_SWITCH_Pin GPIO_PIN_0
#define DCDC_AIR_SWITCH_GPIO_Port GPIOC
#define DCDC_AIR_ACTUAL_Pin GPIO_PIN_1
#define DCDC_AIR_ACTUAL_GPIO_Port GPIOC
#define RLeak2Supply_Pin GPIO_PIN_3
#define RLeak2Supply_GPIO_Port GPIOC
#define RLeak1Supply_Pin GPIO_PIN_2
#define RLeak1Supply_GPIO_Port GPIOF
#define RLeak1_Pin GPIO_PIN_0
#define RLeak1_GPIO_Port GPIOA
#define RLeak2_Pin GPIO_PIN_1
#define RLeak2_GPIO_Port GPIOA
#define VVEHI_Pin GPIO_PIN_2
#define VVEHI_GPIO_Port GPIOA
#define nDangerV_Pin GPIO_PIN_4
#define nDangerV_GPIO_Port GPIOA
#define BuzzerPWM_Pin GPIO_PIN_5
#define BuzzerPWM_GPIO_Port GPIOA
#define TNTC1_Pin GPIO_PIN_6
#define TNTC1_GPIO_Port GPIOA
#define WP_Pin GPIO_PIN_7
#define WP_GPIO_Port GPIOA
#define TNTC5_Pin GPIO_PIN_4
#define TNTC5_GPIO_Port GPIOC
#define VBatt_Pin GPIO_PIN_1
#define VBatt_GPIO_Port GPIOB
#define TNTC2_Pin GPIO_PIN_8
#define TNTC2_GPIO_Port GPIOE
#define VACCU_Pin GPIO_PIN_9
#define VACCU_GPIO_Port GPIOE
#define IMDOK_Pin GPIO_PIN_10
#define IMDOK_GPIO_Port GPIOE
#define IMD_State_Pin GPIO_PIN_11
#define IMD_State_GPIO_Port GPIOE
#define IMDSCClosed_Pin GPIO_PIN_12
#define IMDSCClosed_GPIO_Port GPIOE
#define VDCDC_Pin GPIO_PIN_13
#define VDCDC_GPIO_Port GPIOE
#define TNTC3_Pin GPIO_PIN_14
#define TNTC3_GPIO_Port GPIOE
#define TNTC4_Pin GPIO_PIN_14
#define TNTC4_GPIO_Port GPIOB
#define ERR_loc_out_Pin GPIO_PIN_10
#define ERR_loc_out_GPIO_Port GPIOD
#define ERR_ext_out_Pin GPIO_PIN_11
#define ERR_ext_out_GPIO_Port GPIOD
#define ERRQ_ext_Pin GPIO_PIN_12
#define ERRQ_ext_GPIO_Port GPIOD
#define ERRQ_res_Pin GPIO_PIN_13
#define ERRQ_res_GPIO_Port GPIOD
#define ERRQ_Pin GPIO_PIN_14
#define ERRQ_GPIO_Port GPIOD
#define nSleep_Pin GPIO_PIN_15
#define nSleep_GPIO_Port GPIOD
#define LatchSC_Pin GPIO_PIN_6
#define LatchSC_GPIO_Port GPIOC
#define nPOR_State_Pin GPIO_PIN_7
#define nPOR_State_GPIO_Port GPIOC
#define SC_Latched_Pin GPIO_PIN_8
#define SC_Latched_GPIO_Port GPIOC
#define PCHRG_SWITCH_Pin GPIO_PIN_0
#define PCHRG_SWITCH_GPIO_Port GPIOD
#define nPRCHG_DONE_Pin GPIO_PIN_1
#define nPRCHG_DONE_GPIO_Port GPIOD
#define PCHRG_ACTUAL_Pin GPIO_PIN_2
#define PCHRG_ACTUAL_GPIO_Port GPIOD
#define AIR_P_Switch_Pin GPIO_PIN_3
#define AIR_P_Switch_GPIO_Port GPIOD
#define AIR_N_Switch_Pin GPIO_PIN_4
#define AIR_N_Switch_GPIO_Port GPIOD
#define AIR_P_Intended_Pin GPIO_PIN_6
#define AIR_P_Intended_GPIO_Port GPIOD
#define AIR_P_Actual_Pin GPIO_PIN_7
#define AIR_P_Actual_GPIO_Port GPIOD
#define AIR_N_Intended_Pin GPIO_PIN_5
#define AIR_N_Intended_GPIO_Port GPIOB
#define AIR_N_Actual_Pin GPIO_PIN_6
#define AIR_N_Actual_GPIO_Port GPIOB
#define nAIR_Error_Pin GPIO_PIN_7
#define nAIR_Error_GPIO_Port GPIOB
#define WDBeat_Pin GPIO_PIN_1
#define WDBeat_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
