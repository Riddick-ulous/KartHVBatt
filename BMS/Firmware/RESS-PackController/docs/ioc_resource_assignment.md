# PackController Board- und IOC-Ressourcen — Codex-Übergabestand

Diese Datei ist die normative Quelle für Pinbelegung, GPIO-Defaults, Alternate Functions, Clock, Timer, ADC, DMA und NVIC. Die Softwarearchitektur ist für Verhalten und Zustandsautomaten führend. `PackController.ioc` wird aus beiden Dokumenten abgeleitet; ihr aktueller rudimentärer Stand ist nicht führend.

## Toolchain

| Bereich | Festlegung |
|---|---|
| Target-Compiler | GNU Arm Embedded: `arm-none-eabi-gcc`, `g++`, `as` |
| Sprachstandard | C11, C++17, GNU-Assembler `.S` |
| Build | CMake + Ninja; keine IAR-/EWARM-Abhängigkeit |
| MCU-Flags | Cortex-M4F, Thumb, FPv4-SP-D16, Hard-Float |
| CubeMX | Codegenerator für HAL/LL-Initialisierung; erzeugter Code wird reviewt, nicht als Architekturquelle verwendet |
| Generator-Baseline | vorhandene IOC: STM32CubeMX 6.9.2, STM32CubeG4 1.5.2; Upgrade nur als separater reviewbarer Change |
| CubeMX-Ausgabe | `STM32CubeIDE`/GNU-kompatible Startup- und Linkerdateien; der eigentliche Build bleibt bei CMake/Ninja |
| Programmierung/Debug | 5-pin JTAG |

Die IOC-Einstellung `ProjectManager.TargetToolchain=EWARM V8.50` wird beim nächsten CubeMX-Lauf auf `STM32CubeIDE` umgestellt. IDE-Projektdateien sind nicht der Buildvertrag; CMake/Ninja verwendet die erzeugten GNU-Startup-, Linker- und STM32Cube-Quellen.

## Clock Tree

| Größe | Einstellung |
|---|---:|
| HSE | 16 MHz Crystal an PF0/PF1 |
| PLL | `M=4, N=80, R=2, Q=4` |
| SYSCLK/HCLK/APB1/APB2 | 160 MHz |
| FDCAN kernel | PLLQ = 80 MHz |
| ADC kernel | synchron HCLK/4 = 40 MHz |
| Regulator | Range 1 Boost; Flash-Latency durch CubeMX |

Clock Security System aktivieren. HSE/PLL-Ausfall führt zu `CLOCK_FAILURE`; der WD-Health-Gate wird nicht mehr bedient.

## Timer

| Timer | Funktion | Pin/AF | Taktbasis | Konfiguration |
|---|---|---|---:|---|
| TIM1 CH2/TI2 | IMD PWM Input | PE11 / AF2 | 100 kHz | CH2 direct/rising, CH1 indirect/falling, Slave-Reset; `PSC=1599`, `ARR=65535`, CC-Interrupt; 500-ms-Timeout in Software |
| TIM2 CH1 | Buzzer | PA5 / AF1 | 1 MHz | `PSC=159`, `ARR=249`, `CCR1=125`: 4 kHz, 50 % |
| TIM3 CH2 | Fan/Servo-PWM | PE3 / AF2 | 1 MHz | `PSC=159`, `ARR=19999`: 50 Hz; `CCR2=1000…2000 µs`, Default 2000 µs |
| TIM5 | monotone Zeitbasis | intern | 1 MHz | `PSC=159`, `ARR=0xFFFFFFFF`, free-running, kein IRQ |
| TIM6 | ADC-Akquisition | intern | 1 MHz | `PSC=159`, `ARR=999`, Update als TRGO: 1 kHz, kein IRQ |

TIM1 ist 16 Bit; PWM-Input-Reset liefert Periode auf CH2 und High-Zeit auf CH1. 100 kHz hält 10-Hz-Perioden und den 500-ms-Signal-Timeout ohne Überlaufmehrdeutigkeit. TIM6_TRGO ist für ADC1…ADC5 des STM32G483 zulässig.

## ADC-Sequenzen

Alle ADCs: unabhängiger Modus, 12 Bit, Single-ended, Scan, externe steigende Flanke von TIM6_TRGO, DMA continuous request, Overrun overwrite + Fehlerzähler. VREF+ ist die reale 3,3-V-Referenz; VREFINT wird zur VDDA-Plausibilisierung mitgeführt. Die AMC0311-HV-Kanäle verwenden ebenfalls 3,3 V als Referenz.

| ADC | Rank | Signal | Pin/Kanal | Sampling |
|---|---:|---|---|---:|
| ADC1 | 1 | `RLeak1` | PA0 / IN1 | 247,5 cycles |
| ADC1 | 2 | `VVEHI` | PA2 / IN3 | 92,5 cycles |
| ADC1 | 3 | `TNTC4` | PB14 / IN5 | 92,5 cycles |
| ADC1 | 4 | `VREFINT` | interner Kanal | 247,5 cycles |
| ADC2 | 1 | `RLeak2` | PA1 / IN2 | 247,5 cycles |
| ADC2 | 2 | `TNTC1` | PA6 / IN3 | 92,5 cycles |
| ADC2 | 3 | `TNTC5` | PC4 / IN5 | 92,5 cycles |
| ADC3 | 1 | `VBatt` | PB1 / IN1 | 92,5 cycles |
| ADC3 | 2 | `VACCU` | PE9 / IN2 | 92,5 cycles |
| ADC3 | 3 | `VDCDC` | PE13 / IN3 | 92,5 cycles |
| ADC4 | 1 | `TNTC3` | PE14 / IN1 | 92,5 cycles |
| ADC5 | 1 | `TNTC2` | PE8 / IN6 | 92,5 cycles |

Die vorhandene IOC enthält derzeit nur je einen Rank und ordnet PE8 korrekt ADC5 zu. Pro ADC liegt ein zirkulärer Puffer über 20 komplette Sequenzen vor (`ADC1: 80`, `ADC2/3: 60`, `ADC4/5: 20` Halfwords); Half/Full-Transfer liefern jeweils zehn 1-kHz-Samples für das nichtüberlappende 100-Hz-Mittel.

## DMA/DMAMUX

| Kanal | Request | Richtung | Modus/Priorität |
|---|---|---|---|
| DMA1 CH1 | ADC1 | Peripheral→Memory, Halfword | Circular, Very High |
| DMA1 CH2 | ADC2 | Peripheral→Memory, Halfword | Circular, Very High |
| DMA1 CH3 | ADC3 | Peripheral→Memory, Halfword | Circular, Very High |
| DMA1 CH4 | ADC4 | Peripheral→Memory, Halfword | Circular, High |
| DMA1 CH5 | ADC5 | Peripheral→Memory, Halfword | Circular, High |
| DMA2 CH1 | USART1_RX | Peripheral→Memory, Byte | Normal, Very High |
| DMA2 CH2 | USART1_TX | Memory→Peripheral, Byte | Normal, High |
| DMA2 CH3 | USART2_RX | Peripheral→Memory, Byte | Normal, Very High |
| DMA2 CH4 | USART2_TX | Memory→Peripheral, Byte | Normal, High |

USART1/2 laufen mit 2 Mbit/s, 8N1; RX-Abschluss über UART-IDLE plus DMA-Transferstand. EEPROM/I²C2 bleibt interrupt- oder pollingbasiert und benötigt keinen DMA-Kanal.

## Alternate Functions und Busparameter

| Funktion | Pins | AF / Parameter |
|---|---|---|
| FDCAN1 | PA11 RX, PA12 TX | AF9; 1 Mbit/s nominal, 4 Mbit/s data, BRS |
| FDCAN2 | PB12 RX, PB13 TX | AF9; identische Bitzeiten |
| USART1 TLE-HS | PE0 TX, PC5 RX | AF7; 2 Mbit/s, 8N1 |
| USART2 TLE-LS | PD5 TX, PA3 RX | AF7; 2 Mbit/s, 8N1 |
| I²C2 EEPROM | PA8 SDA, PA9 SCL | AF4, 400 kHz, Open Drain |
| TIM1 IMD | PE11 CH2/TI2 | AF2; PWM Input |
| TIM2 Buzzer | PA5 CH1 | AF1; 4 kHz, 50 % |
| TIM3 Fan | PE3 CH2 | AF2; 50 Hz, 1…2 ms |
| HSE | PF0 OSC_IN, PF1 OSC_OUT | 16-MHz-Crystal |
| JTAG | PA13 JTMS, PA14 JTCK, PA15 JTDI, PB3 JTDO, PB4 nJTRST | AF0, 5-pin JTAG |

Die Verwendung von PE2 als `Heartbeat` verhindert ETM-Trace, nicht jedoch das vorgesehene 5-pin JTAG.

Der 24LC256 verwendet die 7-Bit-Adresse `0x50`; A0, A1 und A2 liegen im
freigegebenen Boardstand auf GND. HAL-Aufrufe verwenden entsprechend die
linksverschobene Busadresse `0xA0`. Die physische Pagegröße beträgt 64 Byte,
die Wortadresse ist 16 Bit breit. `WP` auf PA7 bleibt außerhalb eines
tatsächlichen Page-Write-Transfers HIGH.

FDCAN bei 80 MHz:

| Phase | Prescaler | TSEG1 | TSEG2 | SJW | Samplepoint |
|---|---:|---:|---:|---:|---:|
| nominal 1 Mbit/s | 4 | 15 | 4 | 4 | 80 % |
| data 4 Mbit/s | 2 | 7 | 2 | 2 | 80 % |

Für beide Controller: ISO CAN-FD, automatische Retransmission, Standard-ID-Filter ab `0x100`, RX FIFO0 und TX FIFO/Queue mit 64-Byte-Elementen. FDCAN1 und FDCAN2 benötigen jeweils beide Interruptlinien.

## Digitale GPIO

Ausgangswerte werden per BSRR gesetzt, bevor der Pin als Output beziehungsweise Alternate Function aktiviert wird.

| Signal | Pin | Bootwert / Betrieb |
|---|---|---|
| `DCDC_AIR_SWITCH` | PC0 | LOW |
| `RLeak2Supply` | PC3 | LOW/Hi-Z; nur im 1-Hz-Messzyklus HIGH |
| `LatchSC` | PC6 | LOW; nur explizite Power-Cycle-Faultaktion darf setzen |
| `WP` | PA7 | HIGH; nur während EEPROM-Write LOW |
| `PCHRG_SWITCH` | PD0 | LOW |
| `AIR_P_Switch` | PD3 | LOW |
| `AIR_N_Switch` | PD4 | LOW |
| `WDBeat` | PE1 | LOW; Scheduler-Health-Gate toggelt alle 100 ms, kein Hardware-PWM |
| `Heartbeat` | PE2 | LOW; Toggle im 1-Hz-Task |
| `FANPWM` | PE3 | LOW bis TIM3-Start, danach 100 % = 2000 µs |
| `ErrorLED` | PE4 | LOW; bei STM-/HV-Hardfault oder SAFE latched HIGH |
| `BuzzerPWM` | PA5 | LOW bis TIM2-Start; PWM nur während Beep-Sequenz |
| `RLeak1Supply` | PF2 | LOW/Hi-Z; nur im 1-Hz-Messzyklus HIGH |

| Signal | Pin | Polung / Auswertung |
|---|---|---|
| `DCDC_AIR_ACTUAL` | PC1 | active-high, 100 ms bestätigt |
| `nPOR_State` | PC7 | active-low benannt; muss im Betrieb HIGH sein, 100 ms bestätigt |
| `SC_Latched` | PC8 | active-high, 100 ms bestätigt |
| `TSAL_GRN_ON` | PC15 | active-high, 100 ms bestätigt |
| `nDangerV` | PA4 | gefährliche Spannung = LOW; 100 ms bestätigt |
| `AIR_N_Intended` | PB5 | active-high, 100 ms bestätigt |
| `AIR_N_Actual` | PB6 | active-high, 100 ms bestätigt |
| `nAIR_Error` | PB7 | Fehler = LOW; 100 ms bestätigt |
| `nPRCHG_DONE` | PD1 | Precharge fertig = LOW; 100 ms bestätigt |
| `PCHRG_ACTUAL` | PD2 | active-high, 100 ms bestätigt |
| `AIR_P_Intended` | PD6 | active-high, 100 ms bestätigt |
| `AIR_P_Actual` | PD7 | active-high, 100 ms bestätigt |
| `IMDOK` | PE10 | IMD-Plausibilisierung, 100 ms bestätigt |
| `IMDSCClosed` | PE12 | IMD-Plausibilisierung, 100 ms bestätigt |

Rohpegel werden zusätzlich diagnostiziert; Stateflows verwenden ausschließlich bestätigte Zustände.

## TLE9015 ERR-Signale

| MCU | Firmware-Name | Richtung | aktiv | Normalzustand |
|---|---|---|---|---|
| PD10 | `ERR_loc_out` | TLE→MCU | HIGH-Puls bei lokal/iso-UART ausgelöstem Fehler | LOW |
| PD11 | `ERR_ext_out` | TLE→MCU | HIGH bei über `ERRQ_ext` ausgelöstem Fehler | LOW |
| PD12 | `ERRQ_ext` | MCU→TLE | LOW löst externen, gelatchten ERRQ aus | HIGH |
| PD13 | `ERRQ_res` | MCU→TLE | LOW setzt ERRQ-Latch zurück | HIGH |
| PD14 | `ERRQ` | TLE→MCU, Open Drain | LOW gelatchter Fehler | HIGH über externen Pull-up |
| PD15 | `nSleep` | MCU→TLE | fallende Flanke/LOW erzwingt Sleep | HIGH |

`ERRQ_Ioc_out` in der IOC ist in `ERR_loc_out` umzubenennen. Für die Hardwarefreigabe sind VIO=3,3 V, der PD14-Pull-up auf 3,3 V und das Fehlen externer Inverter im finalen Schaltplan zu bestätigen.

## Nachzuführende IOC-Punkte

| Bereich | aktueller rudimentärer Stand | normative Einstellung |
|---|---|---|
| Toolchain | EWARM V8.50 | GNU Arm Embedded |
| HSE / Systemtakt | 8 / 16 MHz | 16 / 160 MHz; FDCAN 80 MHz |
| FDCAN1/2 | 200 kbit/s | CAN-FD+BRS, 1/4 Mbit/s |
| ADC | je Instanz nur ein Rank | obige fünf Sequenzen, TIM6-TRGO, DMA circular |
| PE11 | GPIO Input | TIM1 CH2/TI2 PWM Input |
| PC15 | GPIO Output | `TSAL_GRN_ON` Input |
| GPIO-Namen | `AIR_Error`, `ERRQ_Ioc_out`, PC7 ohne Label | `nAIR_Error`, `ERR_loc_out`, `nPOR_State` |
| Timer/DMA | TIM2/TIM3 rudimentär; Rest fehlt | TIM1/2/3/5/6 und DMA1/2 gemäß Tabellen |
| Interrupts | FDCAN2/UART/DMA/TIM1 unvollständig, meist Priorität 0 | NVIC gemäß nachfolgender Tabelle |

Nach der Übernahme wird die IOC einmal mit CubeMX regeneriert. Der Diff muss Clock, GPIO-Bootwerte, AF, DMA-Requests und IRQ-Prioritäten vollständig abbilden; Abweichungen werden in diesem Dokument korrigiert, nicht rückwärts aus der IOC in die Architektur übernommen.

## NVIC-Zuordnung

| Priorität | Quellen |
|---:|---|
| 3 | TIM1 Capture, FDCAN1/2 IT0 |
| 4 | USART1/2 und DMA2 RX |
| 5 | ADC-DMA Half/Full, FDCAN1/2 IT1 |
| 6 | DMA2 TX, I²C2 Error/Event |
| 15 | SysTick; ISR setzt nur Tick/Flags |

ADC-Peripheral-IRQs werden nur für Analog-Watchdog/Overrun verwendet; die Datenerfassung läuft ausschließlich über DMA.
