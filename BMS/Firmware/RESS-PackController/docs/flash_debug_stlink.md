# PackController mit STLINK-V3MINIE flashen und debuggen

Diese Anleitung beschreibt den kontrollierten Bring-up der Inkremente 1 und 2.
Sie setzt voraus, dass die Firmware bereits mit dem Preset `target-debug`
gebaut wurde. CubeIDE darf zum grafischen Debuggen verwendet werden; gebaut
wird weiterhin ausschließlich mit CMake/Ninja und der GNU-Arm-Toolchain.

## Empfohlener Einstieg

| Aufgabe | Empfohlener Weg | Alternative |
|---|---|---|
| Firmware bauen | CMake-Preset `target-debug` | – |
| Erstes Flashen | STM32CubeProgrammer GUI | `STM32_Programmer_CLI.exe` |
| Quellcode-Debugging | externes ELF in STM32CubeIDE importieren | OpenOCD und GDB |

Für den ersten Boardkontakt ist die Kombination aus CubeProgrammer GUI und
CubeIDE am einfachsten. Sie bietet dieselbe grafische Bedienung wie ein
klassisches CubeIDE-Projekt, verwendet aber das von CMake gebaute ELF.

## 1. Firmware bauen

In PowerShell im Verzeichnis `software/RESS-PackController`:

```powershell
$BuildTools = (Resolve-Path '.\.venv\Scripts').Path
$ArmTools = 'C:\ST\STM32CubeIDE_1.13.2\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.11.3.rel1.win32_1.1.1.202309131626\tools\bin'
$Python = "$env:LOCALAPPDATA\Programs\Python\Python314\python.exe"
& $Python --version

$env:Path = "$BuildTools;$ArmTools;$env:Path"

cmake --preset target-debug -DPython3_EXECUTABLE="$Python"
cmake --build --preset target-debug
```

Die zu verwendende Datei ist:

```text
build/target-debug/generated/stm32cube/packcontroller.elf
```

Das ELF ist für Flashen und Debuggen vorzuziehen. Es enthält neben den
Flash-Adressen auch Symbole, Quellcodezuordnungen und Debug-Informationen.
Für eine ELF- oder HEX-Datei wird keine manuelle Startadresse eingetragen.
Nur eine rohe BIN-Datei würde die Adresse `0x08000000` benötigen.

Vor dem Anschließen kann der Build noch einmal geprüft werden:

```powershell
Get-Item build\target-debug\generated\stm32cube\packcontroller.elf
```

## 2. Hardware sicher anschließen

Für den ersten Bring-up gelten folgende Randbedingungen:

1. HV, Schütze, DCDC und Leistungspfade bleiben spannungsfrei.
2. Das Controllerboard wird aus einer strombegrenzten Kleinspannungsquelle
   versorgt.
3. Erst Boardversorgung einschalten, dann prüfen, ob am Debuganschluss die
   erwartete Logikversorgung anliegt.
4. STLINK-V3MINIE und Target benötigen eine gemeinsame Masse.
5. `T_NRST` muss verbunden sein, damit „Connect under reset“ zuverlässig
   funktioniert.
6. Der STLINK-V3MINIE versorgt das Target nicht. `T_VCC` ist ein Eingang zur
   Erkennung der Targetspannung und zur Pegelanpassung.

### STDC14-Signale

Der STLINK-V3MINIE besitzt einen STDC14-Anschluss. Vor dem Verbinden unbedingt
Pin-1-Markierung, Kabelorientierung und den tatsächlichen Boardstecker prüfen.
Die folgende Tabelle beschreibt die für dieses Projekt relevanten Signale:

| STDC14 | Probe-Signal | PackController-MCU | Zweck |
|---:|---|---|---|
| 3 | `T_VCC` | Debug-VREF/3,3 V | Pegelreferenz, keine Versorgung |
| 4 | `T_JTMS` | PA13 | JTAG TMS |
| 5, 7 | `GND` | GND | gemeinsame Masse |
| 6 | `T_JCLK` | PA14 | JTAG TCK |
| 8 | `T_JTDO` | PB3 | JTAG TDO |
| 10 | `T_JTDI` | PA15 | JTAG TDI |
| 11 | `GNDDETECT` | GND | Targeterkennung |
| 12 | `T_NRST` | MCU NRST | Hardware-Reset |

PB4 ist in der IOC als `SYS_JTRST` konfiguriert. Der Standard-STDC14-Anschluss
des STLINK-V3MINIE führt dieses Signal nicht heraus. Es darf nicht mit dem für
„Connect under reset“ benötigten MCU-Signal `NRST` verwechselt werden.

Der Ressourcenvertrag verwendet weiterhin 5-pin JTAG:

```text
PA13  JTMS
PA14  JTCK
PA15  JTDI
PB3   JTDO
PB4   nJTRST
```

Am aktuellen Aufbau wurde PB4 elektrisch getrennt, um die zuvor belastete
Resetbeschaltung einzugrenzen. Damit ist JTAG nicht vollständig, SWD über
PA13/PA14 funktioniert jedoch nachweislich zum Flashen und Debuggen. Die
folgenden Standardabläufe verwenden deshalb SWD. Die JTAG-Abnahme bleibt eine
offene Hardwareabweichung und wird dadurch nicht als erfüllt erklärt.

## 3. Erstes Flashen mit STM32CubeProgrammer GUI

### 3.1 Verbindung aufbauen

1. STM32CubeProgrammer starten.
2. Rechts oben als Schnittstelle **ST-LINK** auswählen.
3. Den STLINK-V3MINIE per USB verbinden.
4. Das Controllerboard aus der strombegrenzten Versorgung einschalten.
5. Im ST-LINK-Konfigurationsfeld einstellen:

   | Einstellung | Wert für den ersten Versuch |
   |---|---|
   | Serial number | angeschlossenen STLINK-V3MINIE auswählen |
   | Port | `SWD` |
   | Frequency | `1000 kHz` für Erstkontakt |
   | Mode | `Under Reset` / `Connect under reset` |
   | Reset mode | `Hardware reset` |
   | Access port | `0` |
   | Shared | deaktiviert |

6. Prüfen, ob eine plausible Targetspannung angezeigt wird. Bei einer
   3,3-V-Logikversorgung sollte sie ungefähr 3,3 V betragen.
7. **Connect** drücken.

Nach erfolgreicher Verbindung müssen mindestens MCU-Typ, Device-ID und
Flashgröße angezeigt werden. Erwartet wird ein STM32G483VETx mit 512 KiB
internem Flash. Erst danach programmieren.

### 3.2 ELF programmieren

1. Links **Erasing & Programming** öffnen.
2. Über **Browse** folgende Datei auswählen:

   ```text
   build/target-debug/generated/stm32cube/packcontroller.elf
   ```

3. **Verify programming** beziehungsweise **Verify after programming**
   aktivieren.
4. Für den allerersten Versuch **Run after programming** deaktiviert lassen.
   So bleibt der Übergang zum Debugger kontrolliert.
5. **Start Programming** drücken.
6. Auf beide Erfolgsmeldungen achten:

   - Programming complete/successful
   - Verification successful

7. Danach entweder **Disconnect** drücken oder direkt zu CubeIDE wechseln.
   CubeProgrammer muss vor einer exklusiven CubeIDE-Debugverbindung beendet
   oder getrennt sein.

Für dieses Inkrement sind kein Mass-Erase und keine Änderung der Option Bytes
notwendig. Insbesondere RDP, Boot-Konfiguration und Write Protection nicht
„auf Verdacht“ verändern.

## 4. Reproduzierbares Flashen per PowerShell

Die Kommandozeile führt denselben Ablauf ohne GUI aus.

### 4.1 Installation und Probe prüfen

```powershell
$Programmer = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
$Elf = (Resolve-Path 'build\target-debug\generated\stm32cube\packcontroller.elf').Path

& $Programmer --version
& $Programmer -l stlink-only
```

Bei mehreren Probes die angezeigte Seriennummer notieren und beim Verbinden
mit `sn=<Seriennummer>` ergänzen.

### 4.2 Erst nur verbinden

```powershell
& $Programmer -c port=SWD freq=1000 mode=UR reset=HWrst
```

`UR` bedeutet „under reset“, `HWrst` Hardware-Reset. Dieser Schritt schreibt
noch nichts in den Flash. Wenn er fehlschlägt, zuerst Verkabelung und
Targetversorgung korrigieren.

### 4.3 Programmieren, verifizieren und starten

```powershell
& $Programmer `
  -c port=SWD freq=1000 mode=UR reset=HWrst `
  -w $Elf `
  -v `
  -rst
```

Nach einer stabilen Verbindung kann `freq=4000` verwendet werden. Für den
ersten Bring-up ist die niedrigere Frequenz robuster. Bei mehreren Probes:

```powershell
& $Programmer `
  -c port=SWD sn=DEINE_STLINK_SERIENNUMMER freq=1000 mode=UR reset=HWrst `
  -w $Elf -v -rst
```

Der Befehl ist nur erfolgreich, wenn Programmierung und anschließender
Bytevergleich erfolgreich gemeldet werden.

## 5. Grafisch mit STM32CubeIDE debuggen

CubeIDE kann das von CMake gebaute ELF importieren. Es muss und darf dafür
nicht den PackController neu generieren.

### 5.1 Externes ELF importieren

1. CubeIDE starten.
2. Einen normalen CubeIDE-Workspace außerhalb des Git-Repositories wählen,
   beispielsweise:

   ```text
   E:\STM32CubeIDE-workspaces\PackControllerDebug
   ```

3. **File → Import…** öffnen.
4. **C/C++ → STM32 Cortex-M Executable** auswählen.
5. Als Executable das absolute Debug-ELF auswählen:

   ```text
   E:\Privat\Git\next-gen-ekart\software\RESS-PackController\build\target-debug\generated\stm32cube\packcontroller.elf
   ```

6. Als MCU `STM32G483VETx` beziehungsweise die exakt bestückte
   STM32G483VE-Variante wählen.
7. Import mit **Finish** abschließen.

CubeIDE legt dafür nur in seinem Workspace ein Hilfsprojekt an. Das
Repository bleibt ein CMake-Projekt. Nicht die IOC aus dem Repository in
CubeIDE öffnen und nicht über CubeIDE regenerieren.

### 5.2 Debug-Konfiguration einstellen

1. **Run → Debug Configurations…** öffnen.
2. Unter **STM32 C/C++ Application** die importierte Konfiguration wählen.
3. Im Tab **Main** kontrollieren:

   - C/C++ Application zeigt auf `packcontroller.elf` aus `target-debug`.
   - Nicht versehentlich das Release-ELF auswählen.

4. Im Tab **Debugger** einstellen:

   | Einstellung | Wert |
   |---|---|
   | Debug probe | ST-LINK |
   | Interface | SWD |
   | Frequency | zunächst 1000 kHz |
   | Reset behaviour | Connect under reset |
   | Reset type | Hardware reset |

5. Im Tab **Startup** kontrollieren:

   - Load executable/image aktiviert
   - Load symbols aktiviert
   - Breakpoint at `main` aktiviert
   - zunächst kein automatisches Resume nach `main`

6. **Apply**, danach **Debug** drücken.
7. Den Wechsel in die Debug Perspective bestätigen.

Beim Start lädt CubeIDE das ELF erneut. Nach jedem CMake-Neubuild die laufende
Debugsession beenden und neu starten, damit Flashinhalt und Symbole wieder
zusammenpassen. Das Hilfsprojekt selbst wird nicht mit CubeIDE gebaut.

## 6. Erster kontrollierter Debuglauf

Beim Breakpoint in `main()` zunächst nicht sofort frei laufen lassen.

### 6.1 Sinnvolle Breakpoints

In CubeIDE über die linke Randspalte oder im Breakpoints-Fenster setzen:

```text
main
Error_Handler
HardFault_Handler
NMI_Handler
```

Zusätzlich ist ein temporärer Breakpoint auf der Zeile mit
`HAL_TIM_Base_Start(&htim5)` hilfreich.

### 6.2 Initialisierung schrittweise prüfen

1. Bis hinter `SystemClock_Config()` steppen.
2. In **Expressions** hinzufügen:

   ```text
   SystemCoreClock
   ```

   Erwartet: `160000000`.

3. Bis hinter `HAL_TIM_Base_Start(&htim5)` laufen.
4. In **SFRs** beziehungsweise **Peripherals** `TIM5` öffnen.
5. `TIM5_CNT` mehrmals aktualisieren. Der Wert muss weiterzählen; eine
   Differenz von ungefähr 1 000 entspricht ungefähr 1 ms.
6. Kontrollieren, dass TIM2, TIM3 und TIM6 noch nicht laufen. Ihre `CEN`-Bits
   müssen null bleiben.
7. Danach bis zur Endlosschleife laufen und dort erneut anhalten.

### 6.3 Sichere Ausgangspegel prüfen

Vor dem Anschließen weiterer Lasten die Pegel mit Multimeter oder Oszilloskop
am Board verifizieren:

| Signal | MCU-Pin | Erwarteter Bootpegel |
|---|---|---|
| `DCDC_AIR_SWITCH` | PC0 | LOW |
| `RLeak2Supply` | PC3 | LOW |
| `LatchSC` | PC6 | LOW |
| `PCHRG_SWITCH` | PD0 | LOW |
| `AIR_P_Switch` | PD3 | LOW |
| `AIR_N_Switch` | PD4 | LOW |
| `WDBeat` | PE1 | LOW |
| `Heartbeat` | PE2 | LOW |
| `ErrorLED` | PE4 | LOW |
| `RLeak1Supply` | PF2 | LOW |
| `BuzzerPWM` | PA5 | LOW, Timer nicht gestartet |
| `FANPWM` | PE3 | LOW, Timer nicht gestartet |
| `WP` | PA7 | HIGH |
| `ERRQ_ext` | PD12 | HIGH |
| `ERRQ_res` | PD13 | HIGH |
| `nSleep` | PD15 | HIGH |

Auf FDCAN1 und FDCAN2 ist bis einschließlich Inkrement 2 noch kein Traffic zu erwarten. Die
Peripherie wird konfiguriert, aber nicht fachlich gestartet.

## 7. Fehlerdiagnose

### STLINK wird nicht gefunden

- Anderes USB-Datenkabel beziehungsweise anderen USB-Port testen.
- Windows-Gerätemanager auf ST-LINK-Geräte prüfen.
- CubeProgrammer, CubeIDE und OpenOCD schließen; nur ein Tool soll die Probe
  exklusiv verwenden.
- STLINK-Firmware über CubeProgrammer aktualisieren, falls das Tool dies
  ausdrücklich verlangt.

### Target voltage ist 0 V oder nicht plausibel

- Targetversorgung fehlt oder ist strombegrenzt abgeschaltet.
- `T_VCC`/Debug-VREF ist nicht verbunden.
- Gemeinsame Masse fehlt.
- Kabel ist verdreht oder Pin 1 ist falsch orientiert.

Nicht versuchen, das Board über `T_VCC` des STLINK-V3MINIE zu versorgen.

### „No target found“ oder JTAG-Verbindung bricht ab

1. JTAG-Frequenz auf 1000 kHz oder 500 kHz reduzieren.
2. `T_NRST` prüfen.
3. „Connect under reset“ plus „Hardware reset“ verwenden.
4. JTMS, JTCK, JTDI und JTDO einzeln durchmessen.
5. Externe Reset-/Watchdog-Beschaltung prüfen.
6. HSE mit 16 MHz prüfen, falls der Abbruch erst nach
   `SystemClock_Config()` auftritt.

Die IOC nicht auf SWD umstellen und keine Pinbelegung ändern, um ein
Verdrahtungsproblem zu kaschieren.

### JTAG meldet „Unknown device MCU“, SWD aber verbindet per Hotplug

Dieses Fehlerbild ist besonders aussagekräftig:

- SWD verwendet PA13/JTMS-SWDIO und PA14/JTCK-SWCLK. Eine erfolgreiche
  SWD-Hotplug-Verbindung bestätigt daher diese beiden Leitungen sowie
  Targetspannung und Masse grundsätzlich.
- JTAG benötigt zusätzlich PA15/JTDI und PB3/JTDO. Diese beiden Leitungen,
  ihre Richtung und ihre Zuordnung am Adapter zuerst prüfen.
- PB4/`nJTRST` muss im Ruhezustand HIGH sein. Ein dauerhaftes LOW hält den
  JTAG-TAP im Reset und kann als „Unknown device“ erscheinen.
- Der Standard-STDC14-Stecker führt `nJTRST` nicht heraus. Deshalb prüfen, wie
  PB4 auf dem konkreten Board beziehungsweise Adapter beschaltet ist. `nJTRST`
  ist nicht dasselbe Signal wie MCU-`NRST`.

Mit ausgeschaltetem Board zunächst Durchgang und Kurzschlüsse prüfen:

```text
STDC14 T_JTDI  -> MCU PA15
STDC14 T_JTDO  <- MCU PB3
MCU PB4/nJTRST -> im Betrieb HIGH
STDC14 T_NRST  -> MCU NRST
```

SWD darf zur Diagnose und zum Recovery verwendet werden, ohne die IOC zu
ändern. Der in Kapitel 18 geforderte stabile JTAG-Nachweis bleibt trotzdem
offen, bis die zusätzliche JTAG-Verdrahtung geklärt ist.

### SWD meldet „Device held under reset“

„Connect under reset“ zieht MCU-`NRST` beim Verbindungsaufbau absichtlich LOW.
Der Pegel muss anschließend wieder HIGH werden. Mit Multimeter oder Oszilloskop
direkt am MCU beziehungsweise am Debugstecker prüfen:

1. STLINK abgezogen, Board versorgt: `NRST` muss HIGH sein.
2. STLINK angeschlossen, noch nicht verbunden: `NRST` muss HIGH bleiben.
3. Während „Connect under reset“: kurzer LOW-Puls ist erwartet.
4. Nach dem Verbindungsversuch: dauerhaft LOW ist nicht erwartet.

Bleibt `NRST` LOW, nicht weiter löschen oder programmieren. Zuerst falsche
Steckerbelegung, Kurzschluss, Reset-Taster, Reset-Supervisor, zu große
Kapazität oder eine weitere Schaltung am Resetnetz untersuchen.

Wenn `NRST` elektrisch korrekt HIGH ist, zunächst SWD mit Normal-/Software-
Reset statt Under-Reset probieren:

```powershell
& $Programmer -c port=SWD freq=1000 mode=NORMAL reset=SWrst -halt -score
```

### Hotplug verbindet, Erase oder Download schlägt fehl

Hotplug verbindet ohne Reset und ohne den laufenden Core automatisch in einen
definierten Startzustand zu bringen. Deshalb vor jedem Schreibversuch zuerst
Core- und Schutzstatus prüfen. Die folgenden Befehle verändern weder Flash
noch Option Bytes:

```powershell
$Programmer = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'

& $Programmer -c port=SWD freq=1000 mode=HOTPLUG -score
& $Programmer -c port=SWD freq=1000 mode=HOTPLUG -ob displ
```

Die vollständige Ausgabe sichern. Insbesondere die Werte für RDP, WRP und
PCROP nicht verändern. `-rdu`, `-ob unlockchip` und Änderungen des RDP-Levels
können einen Mass-Erase auslösen oder Debugzugriff dauerhaft beeinflussen und
gehören nicht in einen ersten Recovery-Versuch.

Danach testen, ob der Core über Hotplug angehalten werden kann:

```powershell
& $Programmer -c port=SWD freq=1000 mode=HOTPLUG -halt -score
```

Nur wenn anschließend `Halted` gemeldet wird, das ELF ohne separaten
Mass-Erase programmieren. STM32CubeProgrammer löscht dabei nur die für das ELF
benötigten Flashbereiche:

```powershell
$Elf = (Resolve-Path 'build\target-debug\generated\stm32cube\packcontroller.elf').Path

& $Programmer `
  -c port=SWD freq=1000 mode=HOTPLUG `
  -halt `
  -w $Elf `
  -v `
  -rst `
  -vb 3 `
  -log packcontroller-flash.log
```

Scheitert dieser Befehl weiterhin, nicht mit Option Bytes oder Mass-Erase
experimentieren. Benötigt werden dann die genaue CLI-Ausgabe sowie die Ausgabe
von `-ob displ`, um Resetproblem, Schreibschutz und Readout Protection sauber
zu unterscheiden.

### Flashen funktioniert, Breakpoint in `main` aber nicht

- Debug- statt Release-ELF ausgewählt?
- Im Startup-Tab „Load executable“ und „Load symbols“ aktiviert?
- Debugsession nach dem letzten CMake-Build neu gestartet?
- Zeigt **C/C++ Application** wirklich auf das aktuelle ELF?
- Falls CubeIDE eine Quelldatei nicht findet, unter **Source Lookup** den
  Repositorypfad ergänzen.

### Firmware landet in `Error_Handler`

- Call Stack öffnen und die zuletzt aufgerufene `MX_*_Init`-Funktion notieren.
- Breakpoint auf `Error_Handler` gesetzt lassen.
- Besonders HSE/PLL, Targetspannung und Resetursache prüfen.
- Noch keine Ausgänge manuell setzen und keine Fehlerbehandlung umgehen.

### HardFault oder Lockup

Im SFR-/Expressions-Fenster sichern:

```text
SCB->CFSR
SCB->HFSR
SCB->BFAR
SCB->MMFAR
SCB->SHCSR
```

Zusätzlich Call Stack, PC, LR und SP notieren, bevor ein Reset ausgelöst wird.

## 8. Bring-up-Protokoll für Inkrement 1

Die formale Abnahme aus Kapitel 18 sollte mindestens folgende Angaben
enthalten:

```text
Datum:
Bearbeiter:
Board-Revision / Seriennummer:
Git-Commit:
STLINK-Seriennummer / Firmware:
Versorgungsspannung / Stromlimit:
Gemessene Targetspannung:
Debug-Port / Frequenz:
Erkannter MCU / Device-ID / Flashgröße:
Programming successful: ja/nein
Verification successful: ja/nein
Breakpoint main erreicht: ja/nein
SystemCoreClock = 160000000: ja/nein
TIM5 1-MHz-Zeitbasis geprüft: ja/nein
Alle sicheren Ausgangspegel geprüft: ja/nein
Abweichungen / Messmittel / Screenshots:
```

Der Softwarepfad wurde per SWD erfolgreich geflasht und debuggt. Der normative
JTAG-Nachweis aus Inkrement 1 bleibt bis zur geklärten beziehungsweise wieder
hergestellten PB4/nJTRST-Verbindung offen.

## 9. Bring-up und Debugging für Inkrement 2

Inkrement 2 fügt noch keine HV-, ADC-, TLE-, EEPROM- oder CAN-Funktion hinzu.
Alle Leistungsausgänge bleiben LOW. Gemessen werden nur Zeitbasis, Heartbeat,
WDBeat und die Reaktion des externen Watchdogs.

### 9.1 Normalbetrieb prüfen

Nach dem Flashen Firmware starten und vorzugsweise mit einem Oszilloskop messen:

| Signal | Pin | Erwartung |
|---|---|---|
| `Heartbeat` | PE2 | Toggle alle 1000 ms, vollständige Periode 2 s |
| `WDBeat` | PE1 | Toggle alle 100 ms, 5-Hz-Rechtecksignal |
| `ErrorLED` | PE4 | LOW, solange kein gespeicherter HardFault/Health-Verlust vorliegt |

In CubeIDE oder GDB zusätzlich beobachten:

```text
g_packcontroller_runtime_diagnostics
g_packcontroller_runtime_diagnostics.scheduler_healthy
g_packcontroller_runtime_diagnostics.watchdog_edges
g_packcontroller_runtime_diagnostics.heartbeat_edges
g_packcontroller_runtime_diagnostics.loop_count
g_packcontroller_runtime_diagnostics.idle_iterations
g_packcontroller_runtime_diagnostics.max_loop_gap_us
g_packcontroller_runtime_diagnostics.deadline_misses
g_packcontroller_runtime_diagnostics.consecutive_deadline_misses
g_packcontroller_runtime_diagnostics.overrun_limit_violations
g_packcontroller_runtime_diagnostics.skipped_releases
```

Nach etwas mehr als einer Sekunde müssen alle fünf `run_count`-Einträge größer
null sein. Die kritischen Einträge 0, 1 und 2 gehören zu den 1-/10-/20-ms-Tasks.
Im ungestörten Lauf bleiben deren Deadline- und Skip-Zähler null.
Ein einzelner Deadline-Overrun setzt nur ID 4. Ein sauberer Folgelauf derselben
Task setzt ihren Strike zurück; erst zwei direkt aufeinanderfolgende Overruns
einer kritischen Task verriegeln ID 3 und stoppen weitere WDBeat-Flanken.

### 9.2 Health-Gate kontrolliert auslösen

Dieser Test darf nur bei spannungsfreien HV-/DCDC-Leistungspfaden und bereits
LOW bestätigten Schaltausgängen stattfinden. Ein gesetzter Hardware-SC-Latch
erfordert anschließend bewusst einen Power-Cycle.

1. Breakpoint in `packcontroller_runtime_poll` setzen und anhalten.
2. Im Expressions-Fenster `g_packcontroller_debug_stall_scheduler` auf `1`
   setzen. Alternativ in GDB:

   ```text
   set variable g_packcontroller_debug_stall_scheduler = 1
   ```

3. Ausführung fortsetzen. Die Hauptschleife läuft weiter, führt aber keine
   Scheduler-Tasks mehr aus.
4. Prüfen, dass sowohl PE1/`WDBeat` als auch PE2/`Heartbeat` keine weiteren
   Flanken erzeugen.
5. Nach dem Timeout des externen Watchdogs muss der Hardwarepfad den Shutdown
   Circuit öffnen und `SC_Latched` setzen. Timeout und Pegel am realen
   Watchdog entsprechend dessen Datenblatt protokollieren.
6. Board vollständig ausschalten, gespeicherte Energie abbauen lassen und
   erst danach wieder einschalten. Ein CAN- oder Software-Reset darf den
   Hardware-Latch nicht löschen.

Das bloße Anhalten des Cores im Debugger stoppt ebenfalls WDBeat und kann den
Watchdog auslösen. Für reproduzierbare Messungen ist deshalb die Variable oben
vorzuziehen: Der Core läuft weiter und der Stillstand ist eindeutig absichtlich.

### 9.3 HardFault Record untersuchen

Der Assembly-Einstieg wählt MSP oder PSP, berücksichtigt einen eventuell
gestapelten FPU-Kontext und sichert R0-R3, R12, LR, PC, xPSR sowie die
Cortex-Faultstatusregister. Der Record liegt in `.noinit`:

```text
g_packcontroller_hardfault_record
```

Nach einem echten HardFault werden Schütz-/DCDC-Ausgänge bestmöglich direkt
LOW geschrieben, WDBeat LOW gesetzt und ErrorLED HIGH gesetzt. Nach dem Reset
erkennt die Firmware nur einen Record mit gültigem Magic, Version und CRC; in
diesem Fall bleibt das Watchdog-Health-Gate gesperrt. Den Fault für die erste
Hardwareprüfung nicht künstlich durch einen ungültigen PC erzeugen – zunächst
reichen Hosttest und ELF-/Symbolprüfung.

### 9.4 Protokoll für Inkrement 2

```text
Datum / Bearbeiter:
Board-Revision / Seriennummer:
Git-Commit:
Debug-Port / Frequenz: SWD / ... kHz
Programming + Verify erfolgreich: ja/nein
Heartbeat PE2, Toggle 1000 ms: ja/nein
WDBeat PE1, Toggle 100 ms: ja/nein
Scheduler critical Deadline-Misses/Skips = 0: ja/nein
Stallvariable gesetzt und WDBeat gestoppt: ja/nein
Watchdog-Timeout gemessen:
Shutdown Circuit geöffnet: ja/nein
SC_Latched gesetzt: ja/nein
Power-Cycle zum Rücksetzen erforderlich: ja/nein
Abweichungen / Oszilloskopbilder:
```

## Referenzen

- [ST UM2910: STLINK-V3MINIE User Manual](https://www.st.com/resource/en/user_manual/um2910-stlinkv3minie-evaluation-board-stmicroelectronics.pdf)
- [ST UM2237: STM32CubeProgrammer Software Description](https://www.st.com/resource/en/user_manual/dm00403500-stm32cubeprogrammer-software-description-stmicroelectronics.pdf)
- [ST UM2609: STM32CubeIDE User Guide](https://www.st.com/resource/en/user_manual/dm00629856-stm32cubeide-user-guide-stmicroelectronics.pdf), Abschnitt „Import STM32 Cortex-M executable“
