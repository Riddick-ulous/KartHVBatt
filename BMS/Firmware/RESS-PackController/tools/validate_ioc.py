#!/usr/bin/env python3
"""Validate Increment-1 IOC and generated STM32Cube contracts."""

from __future__ import annotations

import re
import sys
from pathlib import Path


REPOSITORY = Path(__file__).resolve().parents[1]
IOC = REPOSITORY / "PackController.ioc"
CUBE = REPOSITORY / "generated" / "stm32cube" / "PackController"


def read_ioc(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        key, separator, value = line.partition("=")
        if not separator:
            raise ValueError(f"invalid IOC line: {line}")
        values[key] = value
    return values


def main() -> int:
    errors: list[str] = []
    values = read_ioc(IOC)

    expected = {
        "MxCube.Version": "6.9.2",
        "ProjectManager.FirmwarePackage": "STM32Cube FW_G4 V1.5.2",
        "ProjectManager.TargetToolchain": "STM32CubeIDE",
        "ProjectManager.LibraryCopy": "1",
        "ProjectManager.HeapSize": "0x0",
        "RCC.HSE_VALUE": "16000000",
        "RCC.PLLM": "RCC_PLLM_DIV4",
        "RCC.PLLN": "80",
        "RCC.PLLQ": "RCC_PLLQ_DIV4",
        "RCC.PLLR": "RCC_PLLR_DIV2",
        "RCC.SYSCLKFreq_VALUE": "160000000",
        "RCC.HCLKFreq_Value": "160000000",
        "RCC.FDCANCLockSelection": "RCC_FDCANCLKSOURCE_PLL",
        "RCC.FDCANFreq_Value": "80000000",
        "RCC.EnbaleCSS": "true",
        "TIM1.Prescaler": "1599",
        "TIM1.Period": "65535",
        "TIM1.TIM_SlaveMode": "TIM_SLAVEMODE_RESET",
        "TIM2.Prescaler": "159",
        "TIM2.Period": "249",
        "TIM2.PulseNoDither_1": "125",
        "TIM3.Prescaler": "159",
        "TIM3.Period": "19999",
        "TIM3.PulseNoDither_2": "2000",
        "TIM5.Prescaler": "159",
        "TIM5.Period": "4294967295",
        "TIM6.Prescaler": "159",
        "TIM6.Period": "999",
        "TIM6.TIM_MasterOutputTrigger": "TIM_TRGO_UPDATE",
        "USART1.BaudRate": "2000000",
        "USART2.BaudRate": "2000000",
        "I2C2.Speed": "400",
        "PA7.PinState": "GPIO_PIN_SET",
        "PD12.PinState": "GPIO_PIN_SET",
        "PD13.PinState": "GPIO_PIN_SET",
        "PD15.PinState": "GPIO_PIN_SET",
        "PC15-OSC32_OUT.Signal": "GPIO_Input",
        "PC15-OSC32_OUT.GPIO_Label": "TSAL_GRN_ON",
        "PB7.GPIO_Label": "nAIR_Error",
        "PC7.GPIO_Label": "nPOR_State",
        "PD10.GPIO_Label": "ERR_loc_out",
        "PE11.Signal": "S_TIM1_CH2",
        "NVIC.TIM1_CC_IRQn": "true\\:3\\:0\\:false\\:false\\:true\\:true\\:true\\:true",
        "NVIC.FDCAN1_IT0_IRQn": "true\\:3\\:0\\:false\\:false\\:true\\:true\\:true\\:true",
        "NVIC.FDCAN2_IT0_IRQn": "true\\:3\\:0\\:false\\:false\\:true\\:true\\:true\\:true",
        "NVIC.FDCAN1_IT1_IRQn": "true\\:5\\:0\\:false\\:false\\:true\\:true\\:true\\:true",
        "NVIC.FDCAN2_IT1_IRQn": "true\\:5\\:0\\:false\\:false\\:true\\:true\\:true\\:true",
        "NVIC.SysTick_IRQn": "true\\:15\\:0\\:false\\:false\\:true\\:false\\:true\\:false",
    }

    for adc, conversions in ((1, 4), (2, 3), (3, 3), (4, 1), (5, 1)):
        expected[f"ADC{adc}.ClockPrescaler"] = "ADC_CLOCK_SYNC_PCLK_DIV4"
        expected[f"ADC{adc}.ExternalTrigConv"] = "ADC_EXTERNALTRIG_T6_TRGO"
        expected[f"ADC{adc}.ExternalTrigConvEdge"] = "ADC_EXTERNALTRIGCONVEDGE_RISING"
        expected[f"ADC{adc}.DMAContinuousRequests"] = "ENABLE"
        expected[f"ADC{adc}.Overrun"] = "ADC_OVR_DATA_OVERWRITTEN"
        if conversions > 1:
            expected[f"ADC{adc}.NbrOfConversion"] = str(conversions)

    adc_channels = {
        "ADC1": ("ADC_CHANNEL_1", "ADC_CHANNEL_3", "ADC_CHANNEL_5", "ADC_CHANNEL_VREFINT"),
        "ADC2": ("ADC_CHANNEL_2", "ADC_CHANNEL_3", "ADC_CHANNEL_5"),
        "ADC3": ("ADC_CHANNEL_1", "ADC_CHANNEL_2", "ADC_CHANNEL_3"),
        "ADC4": ("ADC_CHANNEL_1",),
        "ADC5": ("ADC_CHANNEL_6",),
    }
    for adc, channels in adc_channels.items():
        for index, channel in enumerate(channels):
            expected[f"{adc}.Channel-{index}\\#ChannelRegularConversion"] = channel

    for fdcan in ("FDCAN1", "FDCAN2"):
        expected.update(
            {
                f"{fdcan}.FrameFormat": "FDCAN_FRAME_FD_BRS",
                f"{fdcan}.AutoRetransmission": "ENABLE",
                f"{fdcan}.NominalPrescaler": "4",
                f"{fdcan}.NominalTimeSeg1": "15",
                f"{fdcan}.NominalTimeSeg2": "4",
                f"{fdcan}.NominalSyncJumpWidth": "4",
                f"{fdcan}.DataPrescaler": "2",
                f"{fdcan}.DataTimeSeg1": "7",
                f"{fdcan}.DataTimeSeg2": "2",
                f"{fdcan}.DataSyncJumpWidth": "2",
                f"{fdcan}.StdFiltersNbr": "1",
                f"{fdcan}.TxFifoQueueMode": "FDCAN_TX_FIFO_OPERATION",
            }
        )

    dma = {
        "ADC1.0": ("DMA1_Channel1", "DMA_CIRCULAR", "DMA_PRIORITY_VERY_HIGH"),
        "ADC2.1": ("DMA1_Channel2", "DMA_CIRCULAR", "DMA_PRIORITY_VERY_HIGH"),
        "ADC3.2": ("DMA1_Channel3", "DMA_CIRCULAR", "DMA_PRIORITY_VERY_HIGH"),
        "ADC4.3": ("DMA1_Channel4", "DMA_CIRCULAR", "DMA_PRIORITY_HIGH"),
        "ADC5.4": ("DMA1_Channel5", "DMA_CIRCULAR", "DMA_PRIORITY_HIGH"),
        "USART1_RX.5": ("DMA2_Channel1", "DMA_NORMAL", "DMA_PRIORITY_VERY_HIGH"),
        "USART1_TX.6": ("DMA2_Channel2", "DMA_NORMAL", "DMA_PRIORITY_HIGH"),
        "USART2_RX.7": ("DMA2_Channel3", "DMA_NORMAL", "DMA_PRIORITY_VERY_HIGH"),
        "USART2_TX.8": ("DMA2_Channel4", "DMA_NORMAL", "DMA_PRIORITY_HIGH"),
    }
    for request, (instance, mode, priority) in dma.items():
        expected[f"Dma.{request}.Instance"] = instance
        expected[f"Dma.{request}.Mode"] = mode
        expected[f"Dma.{request}.Priority"] = priority

    for key, wanted in expected.items():
        actual = values.get(key)
        if actual != wanted:
            errors.append(f"{key}: expected {wanted!r}, got {actual!r}")

    generated_ioc = CUBE / "PackController.ioc"
    if not generated_ioc.is_file() or IOC.read_bytes() != generated_ioc.read_bytes():
        errors.append("generated IOC copy differs from PackController.ioc")

    generated_files = {
        "main": CUBE / "Core" / "Src" / "main.c",
        "msp": CUBE / "Core" / "Src" / "stm32g4xx_hal_msp.c",
        "linker": CUBE / "STM32CubeIDE" / "STM32G483VETX_FLASH.ld",
        "startup": CUBE / "Core" / "Startup" / "startup_stm32g483vetx.s",
    }
    for name, path in generated_files.items():
        if not path.is_file():
            errors.append(f"missing generated {name}: {path.relative_to(REPOSITORY)}")

    if all(path.is_file() for path in generated_files.values()):
        main_c = generated_files["main"].read_text(encoding="utf-8")
        msp_c = generated_files["msp"].read_text(encoding="utf-8")
        linker = generated_files["linker"].read_text(encoding="utf-8")
        required_main = (
            "HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST)",
            "HAL_RCC_EnableCSS()",
            "hadc1.Init.NbrOfConversion = 4",
            "hadc2.Init.NbrOfConversion = 3",
            "hadc3.Init.NbrOfConversion = 3",
            "sConfig.Channel = ADC_CHANNEL_VREFINT",
            "hfdcan1.Init.FrameFormat = FDCAN_FRAME_FD_BRS",
            "hfdcan2.Init.FrameFormat = FDCAN_FRAME_FD_BRS",
            "sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE",
            "HAL_GPIO_WritePin(GPIOE, Heartbeat_Pin|ErrorLED_Pin|WDBeat_Pin, GPIO_PIN_RESET)",
            "HAL_GPIO_WritePin(GPIOC, DCDC_AIR_SWITCH_Pin|RLeak2Supply_Pin|LatchSC_Pin, GPIO_PIN_RESET)",
            "HAL_GPIO_WritePin(RLeak1Supply_GPIO_Port, RLeak1Supply_Pin, GPIO_PIN_RESET)",
            "HAL_GPIO_WritePin(WP_GPIO_Port, WP_Pin, GPIO_PIN_SET)",
            "HAL_GPIO_WritePin(GPIOD, ERRQ_ext_Pin|ERRQ_res_Pin|nSleep_Pin, GPIO_PIN_SET)",
            "HAL_GPIO_WritePin(GPIOD, PCHRG_SWITCH_Pin|AIR_P_Switch_Pin|AIR_N_Switch_Pin, GPIO_PIN_RESET)",
            "HAL_GPIO_WritePin(BuzzerPWM_GPIO_Port, BuzzerPWM_Pin, GPIO_PIN_RESET)",
            "HAL_GPIO_WritePin(FANPWM_GPIO_Port, FANPWM_Pin, GPIO_PIN_RESET)",
            "HAL_TIM_Base_Start(&htim5)",
        )
        for token in required_main:
            if token not in main_c:
                errors.append(f"generated main.c lacks {token}")
        required_msp = (
            "RCC_FDCANCLKSOURCE_PLL",
            "GPIO_AF9_FDCAN1",
            "GPIO_AF9_FDCAN2",
            "GPIO_AF2_TIM1",
            "GPIO_AF1_TIM2",
            "GPIO_AF2_TIM3",
            "DMA_REQUEST_USART1_RX",
            "DMA_REQUEST_USART2_RX",
        )
        for token in required_msp:
            if token not in msp_c:
                errors.append(f"generated stm32g4xx_hal_msp.c lacks {token}")
        if "_Min_Heap_Size = 0x0" not in linker:
            errors.append("generated FLASH linker script reserves a heap")

    adc_platform = REPOSITORY / "platform" / "src" / "stm32" / "adc_dma_stm32.c"
    if not adc_platform.is_file():
        errors.append("missing STM32 ADC DMA platform adapter")
    else:
        adc_source = adc_platform.read_text(encoding="utf-8")
        required_adc_platform = (
            "ADC1_DMA_COUNT = 80",
            "ADC2_DMA_COUNT = 60",
            "ADC3_DMA_COUNT = 60",
            "ADC4_DMA_COUNT = 20",
            "ADC5_DMA_COUNT = 20",
            "HAL_ADCEx_Calibration_Start(&hadc1",
            "HAL_ADCEx_Calibration_Start(&hadc5",
            "HAL_ADC_Start_DMA(&hadc1",
            "HAL_ADC_Start_DMA(&hadc5",
            "HAL_TIM_Base_Start(&htim6)",
            "HAL_ADC_ConvHalfCpltCallback",
            "HAL_ADC_ConvCpltCallback",
            "HAL_ADC_ErrorCallback",
        )
        for token in required_adc_platform:
            if token not in adc_source:
                errors.append(f"ADC DMA platform adapter lacks {token}")

    for forbidden_generated in (
        CUBE / "Core" / "Src" / "sysmem.c",
        CUBE / ".mxproject",
    ):
        if forbidden_generated.exists():
            errors.append(
                "path-dependent or heap-enabled generated file present: "
                f"{forbidden_generated.relative_to(REPOSITORY)}"
            )

    for directory in ("app", "services", "drivers", "platform"):
        for source in (REPOSITORY / directory).rglob("*"):
            if source.suffix.lower() not in {".c", ".h", ".cc", ".cpp", ".hpp"}:
                continue
            text = source.read_text(encoding="utf-8")
            if re.search(r'#\s*include\s*[<"]stm32[^>"]*[>"]', text, re.IGNORECASE):
                errors.append(f"STM32 header outside generated boundary: {source.relative_to(REPOSITORY)}")

    if errors:
        print("Increment-1 IOC contract validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print("OK: Increment-1 IOC and STM32Cube contracts are consistent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
