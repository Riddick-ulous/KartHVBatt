# PackController – Codex-Übergabestand v0.12

Scope: ausschließlich `software/RESS-PackController` im Monorepository.

| Datei | Zweck |
|---|---|
| `AGENTS.md` | verbindliche Scope-, Safety-, Build- und Testregeln für Codex |
| `docs/software_architecture.md` | konsolidierte Implementierungsgrundlage v0.12 |
| `docs/fault_matrix.md` | stabile `FaultId` 0…86 mit Erkennung, Reaktion und Reset |
| `can/pack_controller.dbc` | Vector-kompatible CAN-FD-Bitbelegung für zehn thematisch gebündelte IDs |
| `docs/ioc_resource_assignment.md` | normative Pin-, GPIO-, Clock-, Timer-, ADC-, DMA-, AF-, ERRQ- und NVIC-Zuordnung |
| `docs/hil_validation.md` | Abnahme von Fan-PWM und Balancing-Thermik |
| `docs/flash_debug_stlink.md` | Schritt-für-Schritt-Anleitung für STLINK-V3MINIE, CubeProgrammer und CubeIDE |
| `docs/can_bringup_increment3.md` | CAN-FD-, Failover- und Developer-Aufbaunachweis für Inkrement 3 |
| `docs/adc_bringup_increment4.md` | ADC-/DMA-/VREFINT-/CAN-Aufbaunachweis für Inkrement 4 |
| `docs/eeprom_bringup_increment5.md` | EEPROM-, A/B-Config- und Testpage-Aufbaunachweis für Inkrement 5 |
| `tools/validate_contract.py` | DBC-/Fault-/Paging-Check; in CI zusätzlich striktes `cantools`-Parsing |
| `tools/generate_dbc.py` | reproduzierbare DBC-C-Codegenerierung und Generated-Drift-Check |
| `requirements-ci.txt` | gepinnte Python-Abhängigkeit für den Contract-Check |
| `.github/workflows/packcontroller.yml` | pfadgefiltertes Contract-Gate im Repository-Root |

Die Toolchain ist auf GNU Arm Embedded mit CMake/Ninja festgelegt. Der `PackController.ioc` und die Quellen unter `generated/stm32cube/PackController` wurden für Inkrement 1 aus dem normativen Ressourcenplan mit CubeMX 6.9.2 und STM32CubeG4 1.5.2 erzeugt. Führend bleiben Architektur, Fault-Matrix, DBC und Ressourcenplan.

## Übergabe-Status

Der Softwareumfang von Inkrement 5 ergänzt die bestehende Runtime um den
interruptgestützten 24LC256-Treiber, 64-Byte-Page-Splitting, schrittweises
ACK-Polling, WP-Steuerung und Readback. `SystemConfig` wird explizit
serialisiert und CRC32-geschützt in zwei 512-Byte-A/B-Slots gespeichert; ein
Commit-Marker wird zuletzt geschrieben, sodass ein unterbrochener Commit auf
den älteren gültigen Slot zurückfällt. `CONFIG_READ/STAGE/COMMIT` und der
ausschließlich explizite Testpage-Selbsttest sind über `BMS_ServiceRequest`
verfügbar. Die ADC-/CAN-Funktionalität aus Inkrement 4 bleibt unverändert.
TLE, Leakage-Sequencer und normale HV-/DCDC-Stateflows bleiben ihren späteren
Inkrementen vorbehalten.

Der feste Developer-Build-Key ist `BRT`, numerisch `0x00425254`. Er ist nur im
`BOARD_BRINGUP`-Profil wirksam und dient ausschließlich gegen versehentliche
Aktivierung; er ist kein Security-Mechanismus. `PACK_162S2P` lehnt sämtliche
Developer-Servicebefehle unabhängig vom Key ab.

Bestätigt:

- CubeMX 6.9.2 / STM32CubeG4 1.5.2 bleiben die erste Generator-Baseline; Upgrade nur separat.
- Der PackController-Scope darf für den pfadgefilterten Workflow `.github/workflows/packcontroller.yml` verlassen werden.

Buildstack:

| Zweck | Festlegung |
|---|---|
| Host-Tests | GoogleTest über CTest; Abhängigkeit auf eine feste Version pinnen |
| DBC-Codegen | `cantools generate_c_source --use-float --database-name pack_controller`; Version pinnen und Generated-Drift in CI prüfen |
| Target | GNU Arm Embedded; CubeMX erzeugt GNU-kompatible Quellen, CMake/Ninja baut sie |
| CI | Contract-Check, DBC-Codegen-Drift, Host-Build/-Tests und Target-Cross-Build |

Nicht blockierende Freigaben vor dem jeweils betroffenen Inkrement:

- DBC-Knotennamen `VCU`, `PackCurrentSensor` und `Inverter` gegen die Fahrzeug-DBC abgleichen;
- TLE-VIO, `ERRQ`-Pull-up und ERRQ-Polungen vor ERRQ-Inbetriebnahme am finalen Schaltplan bestätigen;
- Fan-Kennlinie und Balancing-Thermik am Aufbau freigeben, bevor die jeweiligen Produktionsparameter aktiviert werden.

`ServiceTarget` ist nur der Untertyp generischer Konfigurationsbefehle: `CONFIG_STAGE + IMD_CONFIG` adressiert beispielsweise die IMD-Konfiguration. Das Feld betrifft weder normale CAN-Requests noch die Developer-Ausgangssteuerung und blockiert den Implementierungsstart nicht.

## Host-Abnahme

`cantools==41.3.0` benötigt Python 3.10 oder neuer. Unter Windows wird die
global installierte Python-Version explizit an CMake übergeben; auf dem
aktuellen Entwicklungsrechner ist dies Python 3.14.7.

```powershell
$Python = "$env:LOCALAPPDATA\Programs\Python\Python314\python.exe"
& $Python --version
& $Python -m pip install -r requirements-ci.txt
& $Python tools/validate_contract.py `
  --dbc can/pack_controller.dbc `
  --fault-matrix docs/fault_matrix.md `
  --require-cantools
& $Python tools/generate_dbc.py --check

cmake --preset host-debug -DPython3_EXECUTABLE="$Python"
```

```bash
python3 -m pip install -r requirements-ci.txt
python3 tools/validate_contract.py \
  --dbc can/pack_controller.dbc \
  --fault-matrix docs/fault_matrix.md \
  --require-cantools
python3 tools/validate_ioc.py
python3 tools/generate_dbc.py --check
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```

Damit werden DBC und Fault-Matrix, die IOC-/Cube-Verträge, der reproduzierbare DBC-Code sowie die Host-Tests geprüft. Die CI führt dieselben Gates aus und baut zusätzlich das Target.

## Target bauen

Der GNU-Arm-Binärpfad muss in `PATH` liegen. Unter Windows mit der lokal installierten STM32CubeIDE-Toolchain beispielsweise:

```powershell
$BuildTools = (Resolve-Path '.\.venv\Scripts').Path
$ArmBin = 'C:\ST\STM32CubeIDE_1.13.2\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.11.3.rel1.win32_1.1.1.202309131626\tools\bin'
$Python = "$env:LOCALAPPDATA\Programs\Python\Python314\python.exe"
$env:Path = "$BuildTools;$ArmBin;$env:Path"

cmake --preset target-debug -DPython3_EXECUTABLE="$Python"
cmake --build --preset target-debug

cmake --preset target-release -DPython3_EXECUTABLE="$Python"
cmake --build --preset target-release
```

Die Debug-Artefakte liegen anschließend unter `build/target-debug/generated/stm32cube/` als `packcontroller.elf`, `packcontroller.hex` und `packcontroller.bin`. Release-Artefakte liegen entsprechend unter `build/target-release/generated/stm32cube/`.

## STM32Cube regenerieren

Die Regeneration ist absichtlich ein separater, reviewpflichtiger Schritt. Sie setzt exakt CubeMX 6.9.2 und das in der IOC festgelegte STM32CubeG4 1.5.2 voraus:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/generate_stm32cube.ps1
python tools/validate_ioc.py
git diff -- generated/stm32cube/PackController PackController.ioc
```

Das Skript erwartet das Paket standardmäßig unter `%USERPROFILE%\STM32Cube\Repository\STM32Cube_FW_G4_V1.5.2`. Für einen abweichenden Ablageort dient `-FirmwarePackagePath`. CubeMX-Code bleibt ausschließlich unter `generated/stm32cube`. Die `USER CODE`-Blöcke stellen die sicheren Bootwerte her und starten TIM5; CubeMX-Regeneration und fachliche Änderungen gehören weiterhin in getrennte Commits.

## Mit STLINK-V3MINIE flashen

Eine ausführliche Anleitung mit STDC14-Belegung, CubeProgrammer-GUI,
CubeIDE-Import des externen ELF, Fehlerdiagnose und Bring-up-Protokoll steht in
[`docs/flash_debug_stlink.md`](docs/flash_debug_stlink.md). Der folgende Ablauf
ist nur der CLI-Kurzweg.

Vor dem Anschließen HV und Leistungspfade spannungsfrei halten und das Board nur
aus einer strombegrenzten Kleinspannungsversorgung speisen. Am aktuellen
Aufbau ist SWD über PA13/PA14 der bestätigte Flash-/Debugpfad. Der normative
Ressourcenplan sieht weiterhin 5-pin JTAG vor; dessen Hardware-Nachweis bleibt
wegen des getrennten PB4/nJTRST ein separat zu schließender Punkt.

```powershell
$Programmer = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
$Elf = (Resolve-Path 'build\target-debug\generated\stm32cube\packcontroller.elf').Path

& $Programmer -l stlink
& $Programmer -c port=SWD freq=1000 mode=UR reset=HWrst -w $Elf -v -rst
```

Bei instabiler Verbindung zuerst `freq=1000` versuchen. `mode=UR reset=HWrst` verbindet unter Hardware-Reset und hilft, falls eine vorherige Firmware die Debugpins oder den Takt stört. Das Flashen löscht beziehungsweise überschreibt den verwendeten MCU-Flashbereich.

## Erste Debug-Schritte

OpenOCD aus STM32CubeIDE kann den STLINK-V3MINIE als GDB-Server verwenden:

```powershell
$OpenOcd = 'C:\ST\STM32CubeIDE_1.13.2\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.3.0.202305091550\tools\bin\openocd.exe'
$OpenOcdScripts = 'C:\ST\STM32CubeIDE_1.13.2\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.debug.openocd_2.1.0.202306221132\resources\openocd\st_scripts'

& $OpenOcd -s $OpenOcdScripts -f interface/stlink.cfg `
  -c 'transport select hla_swd' -f target/stm32g4x.cfg
```

In einem zweiten Terminal:

```powershell
arm-none-eabi-gdb build/target-debug/generated/stm32cube/packcontroller.elf
```

```text
target extended-remote localhost:3333
monitor reset halt
load
break main
break Error_Handler
break HardFault_Handler
break NMI_Handler
continue
```

Beim ersten Halt prüfen:

1. `SystemCoreClock == 160000000` und keine Clock-/CSS-Exception.
2. `TIM5->CNT` zählt mit 1 MHz weiter.
3. Alle HV-, DCDC-, Lüfter-, Buzzer-, Wake- und Discharge-Ausgänge bleiben auf ihrem dokumentierten sicheren Pegel.
4. Auf FDCAN1 und FDCAN2 werden `BMS_Status` und `BMS_Analog` alle 20 ms sowie
   `BMS_SafetyDiag` alle 100 ms als CAN-FD+BRS gesendet; TLE,
   Leakage-Sequencer und die normalen HV-/DCDC-Stateflows sind noch nicht
   fachlich aktiv.
5. `Heartbeat` auf PE2 wechselt alle 1000 ms und `WDBeat` auf PE1 nur bei gesundem Scheduler alle 100 ms den Pegel.
6. `g_packcontroller_runtime_diagnostics` zeigt Tasklaufzeiten, Latenzen, Deadline-Misses, übersprungene Releases und Fault-Bitmaps.
7. Bei `HardFault_Handler` wird der Stackframe zusammen mit CFSR/HFSR/DFSR/AFSR/MMFAR/BFAR/SHCSR in `g_packcontroller_hardfault_record` gesichert.

Die vollständige Inkrement-2-Abnahme einschließlich des kontrollierten
Scheduler-Stalls steht in [`docs/flash_debug_stlink.md`](docs/flash_debug_stlink.md).
Der CAN-FD-, Failover- und Developer-Test für Inkrement 3 steht in
[`docs/can_bringup_increment3.md`](docs/can_bringup_increment3.md).
Der Analog-Aufbaunachweis für Inkrement 4 steht in
[`docs/adc_bringup_increment4.md`](docs/adc_bringup_increment4.md).
Der EEPROM-/A/B-Konfigurations- und Testpage-Nachweis für Inkrement 5 steht in
[`docs/eeprom_bringup_increment5.md`](docs/eeprom_bringup_increment5.md).
