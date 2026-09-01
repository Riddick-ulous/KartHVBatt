# PackController FaultId-Matrix — Codex-Übergabestand

`FaultId` ist `uint8_t`. IDs sind stabil; Umbenennungen ändern die Zahl nicht. Die DBC überträgt je 128 Bits für `active` und `latched`, daher bleiben `87…127` reserviert.

## Reaktions- und Resetcode

| Code | Bedeutung |
|---|---|
| `W` | Warning/Diagnose auf CAN, kein Zustandswechsel |
| `I` | Eintritt in `HV_ON` beziehungsweise DCDC-Freigabe sperren |
| `L` | betroffene Leistungsgrenze auf 0, Integration/Lernen pausieren |
| `B` | Balancing sofort ausschalten |
| `S500` | `FAULT_CONTROLLED_CRITICAL`: `HV_SAFE_PENDING`, Ausgänge nach 500 ms LOW |
| `H` | `FAULT_HV_HARDFAULT`: Ausgänge bereits LOW; `ErrorLED` und Hardfault-Buzzer aktiv |
| `HWD` | `FAULT_STM_HARDFAULT`: sichere Ausgänge bestmöglich, `WDBeat` stoppen; externer Watchdog öffnet/latcht den SC |

`AUTO` löscht nach stabiler Fehlerfreiheit, `CAN` nur durch gültige `SafeResetReq`-Flanke nach Fehlerfreiheit, `POWER` nur durch Power-Cycle. Bei `I → S500 in HV/DCDC` ist derselbe Fault vor dem Einschalten ein Inhibit und bei Verlust im aktiven Pfad kritisch.

Im ausschließlich per `BOARD_BRINGUP` verfügbaren `DEV_COMMISSIONING` bleiben alle Faults unverändert active/latched und auf CAN sichtbar. Nur bei frischer Developer-Session wird die Inhibit-/`S500`-Wirkung der IDs `10, 57, 63, 65, 66, 68, 71, 72, 73, 74, 80, 81, 84, 85` maskiert; alle anderen Reaktionen bleiben aktiv. `DEV_OUTPUT_TEST` besitzt die engere, in der Softwarearchitektur definierte unmittelbare Ausgangsabschaltung.

## IDs

| ID | `FaultId` | Erkennung / Entprellung | Reaktion | Reset |
|---:|---|---|---|---|
| 0 | `FAULT_NONE` | kein Fehler | – | `AUTO` |
| 1 | `STM_HARDFAULT` | Cortex HardFault | `HWD` | `POWER` |
| 2 | `CLOCK_FAILURE` | HSE/CSS/PLL-Ausfall | `HWD` | `POWER` |
| 3 | `SCHEDULER_HEALTH_LOSS` | Health-Gate verfehlt | `HWD` | `POWER` |
| 4 | `SCHEDULER_TASK_OVERRUN` | Task-Deadline oder Laufzeitbudget überschritten; ein sauberer Folgelauf setzt den tasklokalen Strike zurück | `W`; zwei direkt aufeinanderfolgende Overruns derselben kritischen 1-/10-/20-ms-Task setzen zusätzlich ID 3 | `AUTO` |
| 5 | `WATCHDOG_RESET_DETECTED` | Resetursache Watchdog | `W`; `SC_Latched` separat über ID 26 | `POWER` |
| 6 | `UNEXPECTED_RESET_CAUSE` | nicht freigegebene Resetursache | `W` | `POWER` |
| 7 | `PLATFORM_INIT_FAILED` | Pflichtperipherie nicht initialisierbar | `I` | `POWER` |
| 8 | `CONFIG_INVALID` | Schema/Wertebereich ungültig | `I` | `POWER` |
| 9 | `CELL_PROFILE_INVALID` | Profil passt nicht zu 162S2P | `I` | `POWER` |
| 10 | `IMD_TYPE_UNSET` | kein gültiger EEPROM-Typ | `I` | `POWER` |
| 11 | `EEPROM_COMMUNICATION` | I²C/ACK-Fehler | `W`; gültige Defaults bleiben verwendbar | `AUTO` |
| 12 | `EEPROM_RECORD_CRC` | Record-CRC ungültig | `W`; nächster gültiger Record/Defaults | `AUTO` |
| 13 | `EEPROM_WRITE_VERIFY` | Readback/CRC nach Befehl falsch | `W` | `CAN` |
| 14 | `EEPROM_SELFTEST_FAILED` | expliziter Selbsttest fehlgeschlagen | `W` | `CAN` |
| 15 | `NVM_SCHEMA_MISMATCH` | unbekanntes Pflichtschema | `I` | `POWER` |
| 16 | `CAN1_BUS_OFF` | FDCAN1 Bus-Off | `W`; CAN2 übernimmt, falls frisch | `AUTO` |
| 17 | `CAN2_BUS_OFF` | FDCAN2 Bus-Off | `W`; CAN1 bleibt primär | `AUTO` |
| 18 | `CAN1_COMMAND_STALE` | kein Counter-Fortschritt für 500 ms | `W`; CAN2 übernimmt, falls frisch | `AUTO` |
| 19 | `CAN2_COMMAND_STALE` | kein Counter-Fortschritt für 500 ms | `W`; CAN1 bleibt primär | `AUTO` |
| 20 | `CAN_COMMAND_LOSS` | beide Befehlsquellen 500 ms stale | `S500` | `CAN` |
| 21 | `CAN_COUNTER_DISCONTINUITY` | Sprung/Duplikat, aber Message frisch | `W`, Dropzähler erhöhen | `AUTO` |
| 22 | `CAN_RX_OVERFLOW` | Software-RX-Ring voll | `W`; bei Commandverlust zusätzlich ID 20 | `AUTO` |
| 23 | `CAN_TX_OVERFLOW` | Software-TX-Ring voll | `W` | `AUTO` |
| 24 | `AIR_ERROR_ACTIVE` | `nAIR_Error` 100 ms LOW | `I`; bei HV-On-Request oder in HV/DCDC `S500` | `CAN` |
| 25 | `POR_STATE_INVALID` | `nPOR_State` 100 ms LOW | `I → S500 in HV/DCDC` | `CAN` |
| 26 | `SC_LATCHED` | `SC_Latched` 100 ms HIGH | `H`; Hardware-SC ist bereits offen | `POWER` |
| 27 | `AIR_N_INTENDED_MISMATCH` | `Intended != Switch`, kombiniert 200 ms | `S500` | `CAN` |
| 28 | `AIR_P_INTENDED_MISMATCH` | `Intended != Switch`, kombiniert 200 ms | `S500` | `CAN` |
| 29 | `PCHARGE_ACTUAL_TIMEOUT` | Actual nicht bestätigt innerhalb 3 s | `S500` | `CAN` |
| 30 | `AIR_P_CLOSE_TIMEOUT` | Actual nicht bestätigt innerhalb 200 ms nach Befehl | `S500` | `CAN` |
| 31 | `PRECHARGE_DONE_TIMEOUT` | `nPRCHG_DONE` nicht LOW innerhalb 3 s | `S500` | `CAN` |
| 32 | `PRECHARGE_VOLTAGE_TIMEOUT` | `VVEHI != VACCU ±10 %` nach 3 s | `S500` | `CAN` |
| 33 | `AIR_N_STUCK_CLOSED` | Actual nach Open-Befehl nicht innerhalb 200 ms LOW | `H` | `CAN` |
| 34 | `PCHARGE_STUCK_CLOSED` | Actual nach Open-Befehl nicht innerhalb 200 ms LOW | `H` | `CAN` |
| 35 | `AIR_P_STUCK_CLOSED` | Actual nach Open-Befehl nicht innerhalb 200 ms LOW | `H` | `CAN` |
| 36 | `DCDC_ACTUAL_TIMEOUT` | Actual nicht innerhalb 200 ms bestätigt | `S500` | `CAN` |
| 37 | `DCDC_VOLTAGE_TIMEOUT` | `VDCDC != VACCU ±10 %` nach 3 s | `S500`, DCDC-Fault | `CAN` |
| 38 | `DCDC_STUCK_CLOSED` | Actual nach Disable nicht innerhalb 200 ms LOW | `H` | `CAN` |
| 39 | `PACK_VOLTAGE_PLAUSIBILITY` | `Vpack` gegen `VACCU` 200 ms außerhalb `max(5 %,10 V)` | `I → S500 in HV/DCDC`, `L` | `CAN` |
| 40 | `DANGER_V_FALSE_NEGATIVE` | `VVEHI > 60 V` und `nDangerV` 100 ms HIGH | `S500` | `CAN` |
| 41 | `DANGER_V_FALSE_POSITIVE` | `VVEHI < 40 V` und `nDangerV` 100 ms LOW | `I` vor HV, sonst `W` | `AUTO` |
| 42 | `TSAL_UNSAFE_GREEN` | SW erwartet ROT, `TSAL_GRN_ON` 100 ms HIGH | `S500` | `CAN` |
| 43 | `TSAL_CONSERVATIVE_RED` | SW erwartet GRÜN, Hardware meldet ROT | `I`, `W` | `AUTO` |
| 44 | `CONTACTOR_SEQUENCE_ILLEGAL` | unerlaubte Sollwertkombination | `S500` | `CAN` |
| 45 | `ACTUATOR_OUTPUT_CONFLICT` | mehrere Owner/inkonsistenter Arbiter-Request | `S500` | `CAN` |
| 46 | `HV_MEASUREMENT_STALE` | VACCU/VVEHI/Feedback nicht aktuell | `I → S500 in HV/DCDC` | `CAN` |
| 47 | `SAFE_OPEN_FEEDBACK_FAILED` | nach SAFE mindestens ein Actual HIGH | `H` | `CAN` |
| 48 | `ADC_PIPELINE_INVALID` | DMA-Overrun, Framebruch oder Timeout | `I → S500 in HV/DCDC` | `CAN` |
| 49 | `ADC_REFERENCE_INVALID` | VDDA/VREF-Plausibilität ungültig | `I → S500 in HV/DCDC` | `CAN` |
| 50 | `VACCU_MEAS_INVALID` | Kanal out-of-range/stale | `I → S500 in HV/DCDC` | `CAN` |
| 51 | `VVEHI_MEAS_INVALID` | Kanal out-of-range/stale | `I → S500 in HV/DCDC` | `CAN` |
| 52 | `VDCDC_MEAS_INVALID` | Kanal out-of-range/stale | DCDC sperren; bei aktivem DCDC `S500` | `CAN` |
| 53 | `VBATT_MEAS_INVALID` | Clamp/out-of-range/stale | `W` | `AUTO` |
| 54 | `LEAKAGE_WARNING` | `R_SENSOR < 3 MΩ`, zwei 1-Hz-Messungen | `W` | `AUTO` |
| 55 | `LEAKAGE_DETECTED` | `R_SENSOR < 1 MΩ`, zwei 1-Hz-Messungen | `W` | `AUTO` |
| 56 | `LEAKAGE_SEVERE` | `R_SENSOR < 300 kΩ`, zwei 1-Hz-Messungen | `S500` | `CAN` |
| 57 | `IMD_NOT_READY` | Startup/kein gültiger RF | `I → S500 bei Verlust in HV/DCDC` | `CAN` |
| 58 | `IMD_CONFIG_MISMATCH` | Typ/Polarität unplausibel | `I → S500 bei Verlust in HV/DCDC` | `CAN` |
| 59 | `IMD_ISOLATION_CRITICAL` | `RF < iso_critical`, Default 300 kΩ | `S500` | `CAN` |
| 60 | `IMD_SPEED_START_BAD` | 30 Hz, Duty 90…95 % | `S500` | `CAN` |
| 61 | `IMD_DEVICE_ERROR` | 40 Hz, Duty 47,5…52,5 % | `S500` | `CAN` |
| 62 | `IMD_EARTH_FAULT` | 50 Hz, Duty 47,5…52,5 % | `S500` | `CAN` |
| 63 | `IMD_SIGNAL_INVALID` | PWM-Timeout/Frequenz/Duty ungültig | `I → S500 bei Verlust in HV/DCDC` | `CAN` |
| 64 | `IMD_UNDERVOLTAGE` | 20-Hz-Zustand | `W`; nur gültigen RF verwenden | `AUTO` |
| 65 | `TLE_TRANSCEIVER_COMM` | TLE9015 Boot-/UART-Test fehlgeschlagen | `I → S500 bei Verlust in HV/DCDC` | `CAN` |
| 66 | `TLE_SLAVE_COUNT` | nicht `kTleSlaveCount` eindeutige Slaves | `I → S500 bei Verlust in HV/DCDC` | `CAN` |
| 67 | `TLE_RING_DEGRADED` | Ring gebrochen, zwei Linien vollständig | `W`, `B` | `AUTO` |
| 68 | `TLE_STACK_COMM_LOSS` | Slave fehlt/doppelt/stale | `I → S500 in HV/DCDC`, `B` | `CAN` |
| 69 | `TLE_REFERENCE_WARNING` | Referenz >±2,5 % über zwei MUX-Zyklen | `W`, `B` betroffener Slave | `AUTO` |
| 70 | `TLE_REFERENCE_FAULT` | Referenz >±5 % über zwei MUX-Zyklen | `I`, `B`; Coverage entscheidet SAFE | `CAN` |
| 71 | `TLE_CELL_DATA_STALE` | kein kohärenter 50-Hz-Frame | `I → S500 in HV/DCDC`, `L`, `B` | `CAN` |
| 72 | `TLE_TEMP_DATA_STALE` | NTC nicht mindestens 2 Hz aktualisiert | `W`; Coverage über ID 73 | `AUTO` |
| 73 | `NTC_COVERAGE_CRITICAL` | >1/3 global oder je Slave ungültig | `S500`, `L`, `B` | `CAN` |
| 74 | `NTC_COLD_INHIBIT` | mindestens `ceil(kNtcCount/3)` gültige 1-s-Mittel <−10 °C | nur `HV_ON` sperren, `W`; DCDC bleibt möglich | `AUTO` |
| 75 | `CELL_UNDERVOLTAGE` | eine Zellgruppe 1 s <2,500 V | `S500`, `L`, `B` | `CAN` |
| 76 | `CELL_OVERVOLTAGE` | eine Zellgruppe 1 s ≥4,250 V | `S500`, `L`, `B` | `CAN` |
| 77 | `CELL_OVERTEMPERATURE` | ein gültiges 1-s-Mittel ≥81 °C | `S500`, `L`, `B` | `CAN` |
| 78 | `CELL_VOLTAGE_SPREAD` | parametrierte Diagnosegrenze überschritten | `W` | `AUTO` |
| 79 | `BALANCING_DIAGNOSTIC` | TLE-Balancingstatus unplausibel | `W`, `B` | `CAN` |
| 80 | `PACK_CURRENT_INVALID` | `PackCurrentStatus` >500 ms oder Quality ungültig | `I`, `L`; im aktiven HV kein SAFE | `AUTO` |
| 81 | `INVERTER_POWER_INVALID` | Inverterleistung stale/Quality ungültig | `W`; 15-s-Überwachung pausiert | `AUTO` |
| 82 | `POWER_LIMIT_VIOLATION` | rollendes 15-s-Energieintegral überschritten | `W` | `AUTO` |
| 83 | `POWER_PLAUSIBILITY` | 200-ms-Mittel `P_inv` gegen `Vpack·Ipack` >`max(10 %,5 kW)` | `W`; Compliance ungültig | `AUTO` |
| 84 | `SOC_FALLBACK_LOW` | kein gültiger SOC-Checkpoint/Anker | `W`; 2-%-Entladelimit, Laden 0 | `AUTO` |
| 85 | `SOC_QUALITY_INVALID` | Integrationsgap oder Pflichtdaten ungültig | `W`, `L` | `AUTO` |
| 86 | `DEVELOPER_SESSION_LOSS` | `DEV_COMMISSIONING`-Keepalive >500 ms oder expliziter Exit bei aktivem HV/DCDC | `S500` | `AUTO` |

`87…127` sind reserviert. `Vcell_1s ≥ 4,200 V` ist Ladeschluss mit `PChargeMax=0` und ausdrücklich kein Fault.
