# EEPROM-Bring-up – Inkrement 5

Diese Prüfung nimmt ausschließlich den 24LC256-Niederspannungspfad in Betrieb.
HV und Leistungspfade bleiben spannungsfrei. Vor jedem Schreibtest müssen alle
vier Schütz-/DCDC-Ausgangsrequests LOW und die Rückmeldungen mindestens 100 ms
offen sein. `DEV_OUTPUT_TEST` muss beendet sein.

## 1. Implementierter Vertrag

- I²C2: PA8 SDA, PA9 SCL, 400 kHz, 7-Bit-Adresse `0x50`
- WP: PA7, HIGH im Idle sowie während Reads und ACK-Polling
- 64-Byte-Pages; Transfers über Pagegrenzen werden automatisch geteilt
- keine feste Write-Delay-Schleife; nach jeder Page schrittweises ACK-Polling
- jeder asynchrone Einzeltransfer ist auf 50 Aufrufe des 1-ms-Service begrenzt
- jeder Write wird vollständig zurückgelesen und byteweise verifiziert
- `SystemConfig`: Slot A `0x0000…0x01FF`, Slot B `0x0200…0x03FF`
- Testpage: ausschließlich `0x7FC0…0x7FFF`
- beim Boot ausschließlich A/B-Reads; ein leerer EEPROM wird nicht beschrieben

## 2. ServiceTargets

| Wert | Enum | Payloadbereich |
|---:|---|---|
| 0 | `NVM_TARGET_NONE` | kein Konfigurationsziel |
| 1 | `SYSTEM_CONFIG` | komplette 128-Byte-Payload |
| 2 | `CELL_PROFILE_CONFIG` | Profil und feste 162S2P-Topologie |
| 3 | `HV_CONFIG` | HV-/Precharge-Parameter |
| 4 | `CHARGE_CONFIG` | Lade- und Lernparameter |
| 5 | `IMD_CONFIG` | IMD-Typ und Decoderparameter |
| 6 | `LEAKAGE_CONFIG` | Leakage-Sequenz und Grenzen |
| 7 | `ADC_CALIBRATION` | Analog-Gains, Offsets und Referenzen |

`ServiceSubIndex` ist ein Byte-Offset innerhalb des Targets. `CONFIG_STAGE`
verwendet `ServicePayloadLength=1…12`; die Payloadbytes liegen little-endian in
`ServiceValue0`, danach `ServiceValue1` und `ServiceValue2`.

Das Schema 1 der 128-Byte-`SystemConfig` ist explizit serialisiert:

| Offset | Typ | Feld |
|---:|---|---|
| 0 | u16 | Schema |
| 2 | u16 | CellProfileId |
| 4 | u16 | SeriesCells |
| 6 | u8 | ParallelCells |
| 8, 10, 12 | u16 | PrechargeTimeout, FeedbackConfirm, FeedbackTimeout in ms |
| 14, 16 | u16 | PrechargeRatioMin/Max in Promille |
| 20 | u32 | PrechargeMin in mV |
| 24…38 | 8 × u16 | Charge-Spannungs-, Strom- und Spread-Parameter |
| 40 | u32 | FullTime in ms |
| 44, 46 | u16 | LearnLowSoc und EnergyLearnAlpha in Promille |
| 56, 57 | u8 | ImdHardwareType und UndervoltageBehavior |
| 58 | u16 | ImdAverageCount |
| 60 | u32 | ImdRan in kΩ |
| 64, 68 | u32 | ImdPwmTimeout und ImdStartupTimeout in ms |
| 72, 74 | u16 | IsolationCritical/Recovery in kΩ |
| 76, 78 | u16 | LeakageSettle in ms und SampleCount |
| 80, 84, 88 | u32 | LeakageWarning/Leak/Severe in kΩ |
| 92 | u16 | LeakageRecoveryHysteresis in Promille |
| 94 | u8 | LeakageConfirmationCount |
| 96 | u32 | gemeinsamer HV-Gain × 1000 |
| 100 | u32 | VBatt-Gain × 1 000 000 |
| 104 | u16 | nominale Referenzspannung in mV |
| 108, 112, 116 | i32 | VVEHI-, VACCU-, VDCDC-Offset in µV |
| 120 | i32 | VBatt-Offset in µV |
| 124 | u16 | Leakage-Versorgung in mV |

Nicht aufgeführte Bytes sind reserviert und werden von der Firmware mit 0
serialisiert. Mehrbytewerte sind durchgehend little-endian.

## 3. Voraussetzungen auf CAN

Auf dem autoritativen Bus muss `VCU_BMS_Control` 0x100 mit fortschreitendem
Alive-Counter weiterlaufen. Vor einem Write prüfen:

- `DeveloperMode == DEV_DISABLED`
- `AirNSwitch`, `PrechargeSwitch`, `AirPSwitch`, `DcdcSwitch` sind offen
- Actual-Rückmeldungen der vier Pfade sind mindestens 100 ms offen
- `SchedulerHealthy == SCHEDULER_HEALTHY`
- `WatchdogFeedEnabled == WATCHDOG_FEED_ENABLED`

Die bestätigten Rückmeldungen stehen zugleich als `AirNActual`,
`PrechargeActual`, `AirPActual` und `DcdcActual` in `BMS_Status`.

Während des Boot-Scans antworten NVM-Befehle mit `SERVICE_BUSY`. Ein leerer
EEPROM ist zulässig: Compile-Time-Defaults bleiben aktiv und
`ServiceNvmSequence` ist 0.

## 4. Reservierte Testpage prüfen

`BMS_ServiceRequest` 0x120 senden:

| Signal | Wert |
|---|---:|
| `ServiceCommand` | `EEPROM_SELFTEST` (4) |
| `ServiceTarget` | `NVM_TARGET_NONE` (0) |
| `ServicePayloadLength` | 0 |
| `ServiceCommitRequest` | `COMMIT_REQUESTED` (1) |
| `ServiceSequence` | neue frei gewählte Sequenz |
| `ServiceRequestAliveCounter` | fortschreitend modulo 16 |

Auf 0x121 folgt zunächst `SERVICE_BUSY`. Nach Page-Write, ACK-Poll und
separatem Readback folgt mit derselben `ServiceResponseSequence`
`SERVICE_OK`. `SERVICE_NVM_ERROR` setzt je nach Ursache Fault 11, 13 oder 14.

Mit dem Oszilloskop an PA7 ist WP im Idle HIGH. Während des 64-Byte-Writes ist
ein kurzer LOW-Puls sichtbar; bei ACK-Poll und Readback ist WP wieder HIGH.
SDA/SCL dürfen mehrere ACK-Poll-Versuche zeigen, aber keine feste Wartephase.

## 5. Beispiel: IMD-Typ provisionieren

Zum Setzen von `IR155_3204_MHS` zunächst 0x120 senden:

| Signal | Wert |
|---|---:|
| `ServiceCommand` | `CONFIG_STAGE` (2) |
| `ServiceTarget` | `IMD_CONFIG` (5) |
| `ServiceSubIndex` | 0 |
| `ServiceValue0` | 2 |
| `ServicePayloadLength` | 1 |
| `ServiceCommitRequest` | 0 |

Nach `SERVICE_OK` folgt ein zweiter Request:

| Signal | Wert |
|---|---:|
| `ServiceCommand` | `CONFIG_COMMIT` (3) |
| `ServiceTarget` | `IMD_CONFIG` (5) |
| `ServicePayloadLength` | 0 |
| `ServiceCommitRequest` | 1 |
| `ServiceSequence` | neue Sequenz |

Der Commit antwortet zunächst `SERVICE_BUSY` und nach Verifikation
`SERVICE_OK`. `ServiceNvmSequence` steigt dabei an. Die neue Konfiguration wird
bewusst erst nach einem MCU-Neustart aktiv. Danach kann sie mit `CONFIG_READ`,
Target 5 und `ServiceSubIndex=0` zurückgelesen werden; Byte 0 von
`ServiceResponseValue0` muss den Wert 2 enthalten.

## 6. Erwartete Fehlerreaktionen

- Write bei nicht offenen Leistungspfaden: `SERVICE_DENIED_STATE`, kein WP-Puls
- Stage ohne Payload oder Commit ohne `COMMIT_REQUESTED`:
  `SERVICE_INVALID_VALUE`
- I²C-/ACK-Timeout: `SERVICE_NVM_ERROR`, Fault 11
- Readback-Abweichung: `SERVICE_NVM_ERROR`, Fault 13
- fehlerhafter expliziter Selbsttest: `SERVICE_NVM_ERROR`, Fault 14
- ungültiger neuer A/B-Slot nach Reset: älterer CRC-gültiger Slot gewinnt

Ein Produktionsdump darf außerhalb der Testpage nie durch den Selbsttest
verändert werden. Brownout-/PVD-Writes und automatische Boot-Writes sind nicht
implementiert.
