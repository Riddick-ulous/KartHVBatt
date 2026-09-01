# Inkrement 4: ADC-/DMA-/CAN-Aufbaunachweis

Dieses Dokument führt durch den ersten Hardwaretest der direkten
Board-Analogsignale. Die Leistungspfade und HV bleiben dabei abgeschaltet.
`BMS_Analog` ist reine Diagnose; die Nachricht schaltet keine Ausgänge und
ersetzt keine spätere Plausibilisierung im HV-/DCDC-Stateflow.

## 1. Was implementiert ist

- TIM6 erzeugt mit 1 kHz den gemeinsamen ADC-Trigger.
- ADC1 bis ADC5 laufen kalibriert, 12 Bit, single-ended und DMA-circular.
- Jeder DMA-Puffer enthält 20 vollständige ADC-Sequenzen. Half- und
  Full-Transfer ergeben jeweils einen nicht überlappenden 10-Sample-Block.
- Die ISR kopiert nur Rohdaten in einen festen Blockring. Mittelwert,
  Umrechnung, Quality und Fault-Auswertung laufen im 10-ms-Task.
- VREFINT korrigiert die tatsächliche VDDA anhand der STM32-Factory-Kalibrierung.
- Die fünf NTC-Kanäle verwenden die 34 Punkte der Eaton-LUT aus der
  Architektur. Die drei HV-Kanäle verwenden Gain 280,167; VBatt 5,54545.
- `BMS_Analog` wird als 64-Byte-CAN-FD+BRS-Frame alle 20 ms auf CAN1 und CAN2
  gesendet. Der Alive-Counter ist je Bus unabhängig.
- Es gibt keinen Heap, keine blockierende ADC-Wartezeit und keine
  Messwertumrechnung in Interrupts.

Die Leakage-Kanäle zeigen in diesem Inkrement nur die ADC-Sense-Spannung.
Anregung und Widerstandsberechnung gehören zu Inkrement 8.

## 2. Kanalreihenfolge

Die folgende Reihenfolge gilt gleichzeitig für den festen Rohblock, die
Runtime-Diagnosearrays und die Raw-/Quality-Felder in `BMS_Analog`.

| Index | ADC/Rank | Board-Netz | Pin | CAN-Physikwert |
|---:|---|---|---|---|
| 0 | ADC1/1 | RLeak1 | PA0 | `RLeak1SenseVoltage` |
| 1 | ADC1/2 | VVEHI | PA2 | `VVehiAnalog` |
| 2 | ADC1/3 | TNTC4 | PB14 | `Tntc4Temperature` |
| 3 | ADC1/4 | VREFINT | intern | `Vdda` |
| 4 | ADC2/1 | RLeak2 | PA1 | `RLeak2SenseVoltage` |
| 5 | ADC2/2 | TNTC1 | PA6 | `Tntc1Temperature` |
| 6 | ADC2/3 | TNTC5 | PC4 | `Tntc5Temperature` |
| 7 | ADC3/1 | VBatt | PB1 | `VBattAnalog` |
| 8 | ADC3/2 | VACCU | PE9 | `VAccuAnalog` |
| 9 | ADC3/3 | VDCDC | PE13 | `VDcdcAnalog` |
| 10 | ADC4/1 | TNTC3 | PE14 | `Tntc3Temperature` |
| 11 | ADC5/1 | TNTC2 | PE8 | `Tntc2Temperature` |

## 3. Bauen und per STLINK-V3MINIE flashen

Falls `cmake` in PowerShell nicht gefunden wird, verwende die mit dem Projekt
installierte Version explizit. Der GNU-Arm-Pfad entspricht der vorhandenen
CubeIDE-1.13.2-Installation und muss bei einer anderen CubeIDE-Version
angepasst werden.

```powershell
$CMake = (Resolve-Path '.\.venv\Scripts\cmake.exe').Path
$Python = "$env:LOCALAPPDATA\Programs\Python\Python314\python.exe"
$ArmBin = 'C:\ST\STM32CubeIDE_1.13.2\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.11.3.rel1.win32_1.1.1.202309131626\tools\bin'
$env:Path = "$ArmBin;$env:Path"

& $CMake --preset target-debug -DPython3_EXECUTABLE="$Python"
& $CMake --build --preset target-debug
```

Board nur aus einer strombegrenzten Kleinspannungsversorgung speisen. HV und
Leistungspfade bleiben getrennt. Am aktuellen Aufbau ist SWD der bestätigte
Debugpfad; PB4/nJTRST wurde für diesen Aufbau elektrisch getrennt.

```powershell
$Programmer = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
$Elf = (Resolve-Path 'build\target-debug\generated\stm32cube\packcontroller.elf').Path

& $Programmer -l stlink
& $Programmer -c port=SWD freq=1000 mode=UR reset=HWrst -w $Elf -v -rst
```

Der komplette Flash-/GDB-Ablauf und die Fehlerbilder stehen in
[`flash_debug_stlink.md`](flash_debug_stlink.md).

## 4. CANalyzer einrichten

1. Aktualisierte `can/pack_controller.dbc` in die CANalyzer-Konfiguration laden
   oder die vorhandene Datenbankverknüpfung aktualisieren.
2. CAN-FD mit 1 Mbit/s nominal und 4 Mbit/s Datenphase sowie BRS aktivieren.
3. Standard-ID `0x112`, Name `BMS_Analog`, DLC 64 beobachten.
4. Auf beiden Bussen eine Zykluszeit von ungefähr 20 ms prüfen.
5. `AnalogAliveCounter` muss pro Bus 0 bis 15 durchlaufen.
6. `AnalogProtocolVersion` muss `ANALOG_PROTOCOL_V0`,
   `AnalogFrameCoherent` muss `ANALOG_FRAME_COHERENT` anzeigen.
7. `AnalogQuality` und jede Kanal-Quality müssen bei gesundem Aufbau
   `SIGNAL_VALID` anzeigen.

Die Quality-Werte bedeuten: `0 = INVALID`, `1 = VALID`, `2 = STALE` und
`3 = FAULT`. VACCU, VVEHI, VDCDC und VBatt werden ausschließlich hier
übertragen; `BMS_Status` enthält keine Kopie dieser Messwerte mehr.

Die DBC enthält Kommentare und Value-Tables für alle Quality-/Statusfelder.
Falls CANalyzer nur Zahlen zeigt, die alte DBC aus der Konfiguration entfernen,
die neue Datei erneut zuordnen und die Measurement-/Trace-Fenster aktualisieren.

## 5. Erwartete Werte und sichere Stimuli

Ohne HV dürfen VACCU, VVEHI und VDCDC nahe 0 V liegen. Zum Skalierungstest nur
eine galvanisch sichere, strombegrenzte Kleinspannung auf der ADC-Seite oder
über einen geeigneten Simulator einspeisen; niemals die MCU-Pins außerhalb
0 bis VDDA treiben.

Rechenanker bei `Vdda = 3,000 V`:

```text
ADC voltage = raw / 4095 * Vdda
HV voltage  = ADC voltage * 280.167
VBatt       = ADC voltage * 5.54545
```

- Raw 2048 am HV-ADC entspricht ungefähr 420,4 V im skalierten Diagnosewert.
- Raw 1024 am VBatt-ADC entspricht ungefähr 4,16 V.
- Ein 10-kΩ-NTC beziehungsweise ein 10-kΩ-Ersatzwiderstand ergibt etwa
  Raw 2048 und 25 °C.
- `Vdda` sollte zur gemessenen 3,3-V-Analogversorgung passen. Eine deutliche
  Abweichung zuerst direkt am Board gegen GND nachmessen.

Der genaue Beginn der VBatt-Zenerklemmung und der zulässige kalibrierte
VBatt-Bereich sind laut Architektur am realen Board zu vermessen. Inkrement 4
erkennt ADC-Sättigung und Stale-/Pipelinefehler; eine niedrigere
boardkalibrierte Clamp-Grenze wird erst nach diesem Messergebnis festgelegt.

## 6. Erste Debug-Schritte

In CubeIDE kann das externe ELF wie in `flash_debug_stlink.md` beschrieben
verwendet werden. Sinnvolle Haltepunkte sind:

```text
packcontroller_platform_adc_init
HAL_ADC_ErrorCallback
packcontroller::app::MeasurementPipeline::process
packcontroller::app::task_10ms
Error_Handler
HardFault_Handler
```

Im Live-Expressions-/Watch-Fenster:

```text
g_packcontroller_runtime_diagnostics.adc_raw
g_packcontroller_runtime_diagnostics.adc_physical
g_packcontroller_runtime_diagnostics.adc_quality
g_packcontroller_runtime_diagnostics.adc_sample_counter
g_packcontroller_runtime_diagnostics.adc_timestamp_ms
g_packcontroller_runtime_diagnostics.adc_dma_error_count
g_packcontroller_runtime_diagnostics.adc_dropped_block_count
g_packcontroller_runtime_diagnostics.adc_overall_quality
g_packcontroller_runtime_diagnostics.adc_coherent
```

Erwartung:

- `adc_sample_counter` steigt mit 100 Hz.
- `adc_timestamp_ms` steigt in 10-ms-Schritten.
- `BMS_Status.RuntimeSeconds` steigt einmal pro Sekunde und beginnt nach jedem
  MCU-Reset wieder bei 0.
- `adc_coherent == 1`, `adc_overall_quality == 1` (`SIGNAL_VALID`).
- DMA- und Drop-Zähler bleiben 0.
- Die Rohwerte variieren typischerweise um wenige Counts; vollkommen starre
  Werte sind bei festem Eingang möglich, sollten aber gegen einen bewusst
  leicht veränderten sicheren Stimulus geprüft werden.

## 7. Fehlernachweis

- NTC kurz nach GND beziehungsweise offen simulieren: zugehörige Quality wird
  `SIGNAL_FAULT`; keine reale HV anschließen.
- ADC-Trigger oder DMA nur im Debugversuch anhalten: nach mehr als 20 ms werden
  Werte `SIGNAL_STALE`, `ADC_PIPELINE_INVALID` (Fault 48) wird gesetzt.
- Ein unplausibler VREFINT-Pfad setzt `ADC_REFERENCE_INVALID` (Fault 49) und
  markiert konservativ alle abhängigen Physikwerte als fehlerhaft.
- VACCU/VVEHI/VDCDC/VBatt verwenden Faults 50 bis 53 gemäß Fault-Matrix.
- Faults 48 bis 52 sind CAN-resettable, Fault 53 auto-clearing. Fault-Reset und
  die zustandsabhängige S500-Reaktion werden mit dem späteren Stateflow
  vollständig angebunden; bis dahin bleiben die Ausgänge ohnehin im sicheren
  HV-NOT-READY-Zustand.

Der Aufbaunachweis ist bestanden, wenn alle elf externen Kanäle samt Rawwert,
Physikwert und Quality plausibel auf beiden CAN-Bussen sichtbar sind,
`Vdda` zur Messung passt und die Fehlerzähler im ungestörten Betrieb bei 0
bleiben.

## 8. Resetdiagnose

`BMS_Status.RuntimeSeconds` beginnt nach jedem MCU-Reset wieder bei 0. Auch
`BMS_Analog.AnalogSampleTimestamp` beginnt erneut bei 0 ms. Ein gleichzeitiger
Rücksprung beider Signale bestätigt einen Neustart und nicht nur eine Änderung
der Error-LED.

Bei einem Schedulerproblem zuerst diese Safety-Signale prüfen:

- Fault-Bit 3: `SCHEDULER_HEALTH_LOSS`, WDBeat wird gestoppt.
- Fault-Bit 4: `SCHEDULER_TASK_OVERRUN`.
- `SchedulerHealthy` und `WatchdogFeedEnabled` müssen beide 1 bleiben.
- Fault-Bit 19 ist lediglich `CAN2_COMMAND_STALE`, wenn nur CAN1 benutzt wird.

Ein analysiertes 40,6-s-Log zeigte elf Neustarts. Vor jedem Neustart war
Fault-Bit 3 gesetzt und die Watchdog-Freigabe 0. Ursache war eine Fälligkeits-
prüfung mit unterschiedlichen Zeitwerten innerhalb eines Scheduler-Durchlaufs:
An einer Release-Grenze konnte die 100-ms-Task fällig werden, nachdem die
schnelleren Tasks bereits als noch nicht fällig geprüft worden waren. Der
Scheduler verwendet deshalb nun einen gemeinsamen Dispatch-Zeit-Snapshot.




Ja – die Ausgänge lassen sich bereits ansteuern, allerdings ausschließlich über DEV_OUTPUT_TEST. Die normale automatische Schützsequenz über HVOnRequest ist noch nicht implementiert; sie kommt erst mit Inkrement 9.
Verfügbare Ausgänge
Maske	Funktion	MCU-Pin
0x01	AIR_N	PD4
0x02	Precharge	PD0
0x04	AIR_P	PD3
0x08	DCDC-Schütz	PC0
0x00	alles aus	–


Bitte die Schützspulen nicht direkt mit den MCU-Pins verbinden, sondern ausschließlich über die vorgesehenen Treiberstufen gemäß Schaltplan.
Empfohlenes Vorgehen
Zunächst ohne HV und möglichst mit strombegrenzter Schützversorgung testen:
1. Alle vier Ausgänge bei ausgeschaltetem Controller auf LOW prüfen.
2. Erst nur Treiberausgänge beziehungsweise Ersatzlasten testen.
3. Danach jeweils nur ein Schütz anschließen.
4. Noch keine HV-Busbars oder leistungsführenden Verbindungen aufbauen.
5. Vor jedem Umverdrahten DEV_OUTPUT_SET = 0 senden und Versorgung abschalten.
Vor der Freigabe sollten über CAN folgende Werte vorliegen:
- SchedulerHealthy = 1
- WatchdogFeedEnabled = 1
- CriticalErrorActive = 0
- PorState_n = 1
- ScLatched = 0
- DangerVoltage = 0
- AuthoritativeCanBus = CAN_SOURCE_MAIN beziehungsweise der verwendete Bus
Dann:
1. VCU_BMS_Control (0x100) mit laufendem Alive-Counter weitersenden.
2. BMS_ServiceRequest (0x120) senden:
   - ServiceCommand = DEV_MODE_ENTER (7)
   - ServiceValue0 = 1 (DEV_OUTPUT_TEST)
   - ServiceValue1 = 0x00425254 (BRT)
3. DEV_MODE_ENTER spätestens alle 500 ms mit neuem ServiceRequestAliveCounter wiederholen.
4. Einzelnen Ausgang mit DEV_OUTPUT_SET (8) und obiger Maske aktivieren.
5. Anschließend ServiceValue0 = 0 senden.
6. Mit DEV_MODE_EXIT (9) beenden.
DEV_OUTPUT_SET erneuert den 500-ms-Keepalive nicht. Bei Keepalive-Verlust oder einem Safety-Gate werden alle Ausgänge innerhalb des nächsten 1-ms-Zyklus abgeschaltet.
Wichtig: Die vollständige Überwachung von Schützfeedback, Precharge-Zeit, Spannungsangleichung und normaler Schaltreihenfolge ist noch nicht vorhanden. Deshalb ist das aktuell ein reiner Niederspannungs-/I/O-Bring-up-Test.
Feedbacks
Falls bereits verdrahtet, kannst du parallel beobachten:
- AIR_N Intended: PB5
- AIR_N Actual: PB6
- Precharge Actual: PD2
- AIR_P Intended: PD6
- AIR_P Actual: PD7
- DCDC Actual: PC1
- nAIR_Error: PB7
Der Developer-Modus prüft jedoch noch nicht die vollständige normale Schützdiagnose. Nicht auf eine automatische Abschaltung bei falschem Actual-Feedback verlassen.
NTC-Test
Für einen ersten Test eignet sich ein 10-kΩ-NTC beziehungsweise ein 10-kΩ-Ersatzwiderstand:
NTC	Pin
TNTC1	PA6
TNTC2	PE8
TNTC3	PE14
TNTC4	PB14
TNTC5	PC4


Bei 10 kΩ und etwa 25 °C sollten ungefähr Raw = 2048, etwa 25 °C und Quality = SIGNAL_VALID erscheinen. Die übrigen offenen NTC-Kanäle bleiben auf SIGNAL_FAULT; deshalb kann die zusammengefasste Analog-Quality weiterhin FAULT anzeigen. Das blockiert DEV_OUTPUT_TEST derzeit nicht.