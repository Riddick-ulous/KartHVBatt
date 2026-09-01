# Inkrement 3: CAN-FD- und Developer-Bring-up

Diese Anleitung ergänzt die allgemeine Flash- und Debug-Anleitung um den
Aufbaunachweis für Inkrement 3. Sie verändert weder DBC noch Ressourcenplan.

## 1. Sicherheitsgrenze

Für die ersten CAN- und Ausgangstests bleiben HV, Zwischenkreis und
Leistungspfade spannungsfrei. Die vier Schaltausgänge werden zuerst mit
abgesteckten Schützen beziehungsweise einer ungefährlichen Prüflast gemessen.
`DEV_OUTPUT_TEST` umgeht absichtlich die normale Schützreihenfolge und ist kein
Betriebsmodus.

Vor einem Ausgangstest müssen folgende Leitungen am MCU-Pin gemessen werden:

| Signal | erforderlicher Pegel für mindestens 100 ms |
|---|---|
| `nDangerV` PA4 | HIGH |
| `nPOR_State` PC7 | HIGH |
| `SC_Latched` PC8 | LOW |

Der Scheduler muss gesund sein und es darf kein Critical-Fault anstehen. Ein
Verlust eines dieser Gates setzt AIR_N, PCHRG, AIR_P und DCDC im nächsten
1-ms-Task LOW. Der Output Arbiter prüft dieselben Gates noch einmal unmittelbar
vor dem einzigen GPIO-Commit.

## 2. Bauen und flashen

Die vollständige Windows-Einrichtung von CMake, Ninja, GNU Arm und STLINK steht
in [flash_debug_stlink.md](flash_debug_stlink.md). Nach deren Einrichtung:

```powershell
$Python = "$env:LOCALAPPDATA\Programs\Python\Python314\python.exe"

cmake --preset target-debug -DPython3_EXECUTABLE="$Python"
cmake --build --preset target-debug

$Programmer = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
$Elf = (Resolve-Path 'build\target-debug\generated\stm32cube\packcontroller.elf').Path
& $Programmer -c port=SWD freq=1000 mode=UR reset=HWrst -w $Elf -v -rst
```

Das ELF enthält Symbole. Für Breakpoints und Variablenansicht deshalb das ELF,
nicht die BIN-Datei, in CubeIDE oder GDB öffnen.

## 3. CAN-Hardware anschließen

Beide Busse benötigen einen CAN-FD-fähigen Transceiver und ein CAN-FD-fähiges
Interface. Die beiden Busse sind elektrisch getrennt und dürfen zum
Failover-Test nicht einfach zusammengeschaltet werden.

| Bus | MCU-Pins | Bitraten |
|---|---|---|
| FDCAN1 / MAIN | PA11 RX, PA12 TX | nominal 1 Mbit/s, data 4 Mbit/s |
| FDCAN2 / BACKUP | PB12 RX, PB13 TX | nominal 1 Mbit/s, data 4 Mbit/s |

An jedem Bus:

1. CAN_H, CAN_L und eine gemeinsame Logikmasse verbinden.
2. Im spannungslosen Zustand ungefähr 60 Ohm zwischen CAN_H und CAN_L prüfen,
   wenn an beiden Enden je 120 Ohm terminiert sind.
3. Interface auf ISO CAN-FD, Standard-IDs, BRS, 1/4 Mbit/s und 80-%-
   Samplepoint konfigurieren.
4. Erst danach Board und Interface einschalten.

Unter Linux ist eine typische SocketCAN-Konfiguration:

```bash
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 1000000 sample-point 0.8 \
  dbitrate 4000000 dsample-point 0.8 fd on restart-ms 100
sudo ip link set can0 up
candump -tz can0
```

## 4. Erwarteter Grundtraffic

Direkt nach dem Start sendet der PackController den Inhalt auf beiden Bussen
gespiegelt, aber mit getrennten Alive Countern:

| ID | Frame | Zyklus | DLC |
|---:|---|---:|---:|
| `0x110` | `BMS_Status` | 20 ms | 64 Byte |
| `0x111` | `BMS_SafetyDiag` | 100 ms | 64 Byte |

Beide Frames sind Standard-CAN-FD-Frames mit BRS. `StatusCrcEnabled` ist 0 und
die reservierten CRC-Bytes sind 0. Ohne gültige `VCU_BMS_Control`-Quelle wird
nach der 500-ms-Startfrist `CAN1_COMMAND_STALE`, `CAN2_COMMAND_STALE` und
`CAN_COMMAND_LOSS` sichtbar. Das ist der erwartete sichere Zustand.

Als erste Prüfung:

1. Auf beiden Bussen 0x110 und 0x111 beobachten.
2. Prüfen, dass die jeweiligen TX-Alive-Counter unabhängig modulo 16 laufen.
3. `AuthoritativeCanBus == CAN_SOURCE_NONE` ohne Control-Frame erwarten.
4. Fault-Bits 18, 19 und 20 nach mehr als 500 ms ohne Control-Alive prüfen.

## 5. Control-Alive und Failover prüfen

`VCU_BMS_Control` hat ID `0x100`, DLC 16 und muss als CAN-FD+BRS-Frame gesendet
werden. Der Sender erhöht `ControlAliveCounter` je Bus und Message modulo 16.
CRC bleibt zunächst 0; die CRC-Prüfung ist noch deaktiviert.

Testfolge:

1. Auf CAN1 alle 100 ms 0x100 mit Counter 0, 1, 2, ... senden.
2. In 0x110 `AuthoritativeCanBus == CAN_SOURCE_MAIN` prüfen.
3. Auf CAN2 gleichzeitig einen unabhängig laufenden Counter senden. Requests
   beider Busse dürfen sich nicht vermischen; CAN1 bleibt autoritativ.
4. CAN1-Frames stoppen und CAN2 weiterlaufen lassen. Nach mehr als 500 ms muss
   die Quelle auf `CAN_SOURCE_BACKUP` wechseln.
5. CAN1 wieder mit fortschreitendem Counter senden. Die Quelle darf erst nach
   500 ms stabiler CAN1-Kommunikation zu MAIN zurückkehren.
6. Einen Counter duplizieren: Der Frame darf den letzten Request nicht
   ersetzen. Einen Counter überspringen: Der aktuelle Frame wird übernommen,
   der Dropzähler steigt um `delta - 1`.

Fault 20 `CAN_COMMAND_LOSS` ist CAN-rücksetzbar und bleibt nach einem bereits
eingetretenen Command-Loss gelatcht. Die vollständige Safe-Reset-Prüfung gehört
zum späteren HV-Stateflow. Für den Inkrement-3-Aufbaunachweis deshalb den
100-ms-Control-Generator zuerst starten und den PackController anschließend
resetten; dann trifft der erste gültige Control-Frame innerhalb der
500-ms-Startfrist ein und Fault 20 wird nicht neu gesetzt.

Die Anwendung dekodiert nie in der ISR. Die RX-ISR kopiert Frame, Bus und
Zeitstempel in einen festen Ring; der 1-ms-Task dekodiert maximal den vollständig
gefüllten Software-Ring. Event-TX besitzt einen separaten Vorrangring,
periodische Statusframes verwenden „latest value wins“.

`SchedulerLoopLast` ist der zuletzt gemessene Abstand zwischen zwei
Mainloop-Starts. `SchedulerLoopMax` ist das Maximum seit Reset. Ein konstanter
Maximalwert ist daher normal, während `SchedulerLoopLast` die aktuelle Varianz
zeigt. Beide Signale sind Loop-Abstände und keine isolierten Task-Laufzeiten.

Die Diagnose-Bitmaps sind Bitfelder und absichtlich keine Enums. Beispiel:
`DigitalRawBitmap = 768 = 0x300` setzt Bit 8 (`nDangerV`) und Bit 9
(`nPOR_State`). `FaultActiveBitmapLo = 1835008 = 0x1C0000` setzt die Fault-IDs
18 (`CAN1_COMMAND_STALE`), 19 (`CAN2_COMMAND_STALE`) und 20
(`CAN_COMMAND_LOSS`). `PrimaryFaultId` besitzt dagegen eine vollständige
DBC-Value-Table und wird in CANalyzer symbolisch angezeigt.

### Symbolische Werte in CANalyzer

Die DBC enthaelt fuer alle normativ diskreten Signale `VAL_`-Value-Tables und
fuer jedes der 222 Signale einen `CM_ SG_`-Kommentar. In CANalyzer wird dadurch
nicht die Byte-Darstellung in der normalen Trace-Datenspalte ersetzt. Fuer eine
symbolische Anzeige die Message im Trace expandieren oder das Daten-/Signal-
Fenster verwenden und dort die symbolische beziehungsweise Value-Description-
Darstellung waehlen. Nach einer DBC-Aenderung die Messung stoppen und die
Datenbasis neu laden; im CANdb++-Editor muss beispielsweise fuer `HvState`
der Wert `2 = HV_NOT_READY` sichtbar sein.

`Can1DroppedFrames` und `Can2DroppedFrames` sind saettigende Summen aus
Hardware-/Software-RX-Ringverlusten sowie Control- und Service-Alive-
Duplikaten oder -Luecken. Ein langsam steigender Zaehler bei stabilem Bus ist
daher meist ein Generatorproblem. Der Alive-Counter muss unmittelbar mit jeder
Messageaussendung genau einmal modulo 16 fortgeschrieben werden. Ein separat
getaktetes Signal-Update kann an Taktgrenzen Duplikate oder Spruenge erzeugen.

## 6. Developer-Ausgänge

Der feste Build-Key lautet `BRT`. In `ServiceValue1` wird er als numerischer
32-Bit-Wert `0x00425254` übertragen (`B=0x42`, `R=0x52`, `T=0x54`). CAN-Tools
sollen den Signalwert verwenden; die Little-Endian-Darstellung der vier
Payload-Bytes darf nicht als abweichender String-Key interpretiert werden.

Der Test läuft über `BMS_ServiceRequest` 0x120:

1. Auf demselben autoritativen Bus weiterhin 0x100 mit frischem Alive senden.
2. 0x120 `DEV_MODE_ENTER` senden: `ServiceValue0=1` (`DEV_OUTPUT_TEST`) und
   `ServiceValue1=0x00425254` (`BRT`).
3. `DEV_MODE_ENTER` spätestens alle 500 ms mit neuem
   `ServiceRequestAliveCounter` wiederholen. `DEV_OUTPUT_SET` erneuert den
   Keepalive ausdrücklich nicht.
4. 0x120 `DEV_OUTPUT_SET` mit `ServiceValue0` als Maske senden:

   | Bit | Ausgang |
   |---:|---|
   | 0 | AIR_N / PD4 |
   | 1 | PCHRG / PD0 |
   | 2 | AIR_P / PD3 |
   | 3 | DCDC / PC0 |

5. Jeden Ausgang einzeln einschalten, Pegel messen und wieder ausschalten.
6. Für jedes Gate separat den unsicheren Pegel erzeugen. Alle vier Ausgänge
   müssen spätestens im nächsten 1-ms-Zyklus LOW sein.
7. Keepalive für mehr als 500 ms stoppen und dieselbe Abschaltung prüfen.
8. Mit `DEV_MODE_EXIT` beenden und nochmals alle Ausgänge LOW messen.

Jede Serviceantwort erscheint ereignisgetrieben als 0x121 mit derselben
`ServiceSequence`. `PACK_162S2P` lehnt sämtliche Developer-Befehle unabhängig
vom Key mit `SERVICE_DENIED_STATE` ab.

## 7. Debugvariablen und Breakpoints

Sinnvolle erste Breakpoints:

```text
packcontroller_platform_can_init
HAL_FDCAN_RxFifo0Callback
packcontroller::services::CanService::receive
packcontroller::app::DeveloperSession::handle
packcontroller_platform_commit_switch_outputs
Error_Handler
HardFault_Handler
```

Zusätzlich beobachten:

- `g_packcontroller_runtime_diagnostics`
- `bus_contexts[0].diagnostics` und `bus_contexts[1].diagnostics`
- `can_service.source_`, beide `control_alive_` und `source_switch_count_`
- `developer_session.mode_`, `last_keepalive_ms_` und `output_mask_`
- `committed_outputs`

Die privaten C++-Member können je nach GDB-Darstellung über die Objektansicht
aufgeklappt werden. Ein Halt länger als die Watchdog-Frist ist auf dem realen
Board erwartungsgemäß sicherheitswirksam; beim schrittweisen Debuggen daher mit
offenen beziehungsweise abgesteckten Leistungsausgängen arbeiten.

## 8. Noch erforderlicher Aufbaunachweis

Vor Abschluss von Inkrement 3 protokollieren:

- 0x110/0x111 auf FDCAN1 und FDCAN2 bei 1/4 Mbit/s;
- MAIN/BACKUP-Wechsel und 500-ms-Rückkehrhysterese;
- Bus-Off/Recovery und Dropzähler je Bus;
- TDC-Offset am realen Transceiver und Bus vermessen und den aus den
  Bittimings abgeleiteten Startwert 14 bestätigen oder separat korrigieren;
- alle vier Ausgänge einzeln sowie jedes unmittelbare Abschaltgate prüfen.
