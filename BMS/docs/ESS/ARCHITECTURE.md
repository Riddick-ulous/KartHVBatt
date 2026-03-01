# ESS Architektur (ProtoA)

## Zweck

Dieses Dokument ist die systemweite Source of Truth fuer die ESS-Ebene.
Es beschreibt nur die uebergreifende Architektur, Safety-Signalfluesse und
verweist auf die Subsystem-Spezifikationen.

## 3-Level Control Architecture

### Ebene 1: Stack Controller (pro Stack)

- Cell-Monitoring ueber TLE9012-Chain
- Cell-Balancing
- Temp-Monitoring ueber `10k NTC + TMUX1309A + TLE9012`
- MUX-Select/Scan/Diagnose durch TLE9012

### Ebene 2: Pack Controller (pro Pack, 2x)

- Lokale Schuetz-Ansteuerung `HV+` / `HV-`
- Lokale Fuse-Ueberwachung (falls Sense bestueckt)
- HVIL-Segment (pack-intern + pack-connector)
- Packinterne Feuchtigkeitssensorik (`2` Sensoren gesamt, Verteilung offen)
- Kommunikationsknoten zum ESS Controller

### Ebene 3: ESS Controller (systemweit, 1x)

- Systemorchestrierung fuer beide Packs
- TSAL-Logik + Treiber
- HV-Bus-Spannungsmessung
- HV-Strommessung `HV+` und `HV-` inkl. Kalibrierpfad
- Precharge-System (bestehendes Design)
- Wasserkuehlungssteuerung
- Akustische Signalisierung (Buzzer)
- Crash-Sensor Eingang
- Isolationswaechter-Integration
- CCS2 Ladeinterface
- Fahrzeug-I/O (Medical/Red/Green/Rain/Ready-to-move)
- Zeroing/Kalibrierung zentral
- HV DC/DC Einbindung (offen, ProtoB)

## System-Blockdiagramm (Text)

```text
Pack Links (HV+/- Schuetze, Fuse, Stack 1..N) ----\
                                                    >--- Gemeinsamer ESS-HV-Bus --- Inverter
Pack Rechts (HV+/- Schuetze, Fuse, Stack N..9) ----/

ESS Controller
  |- steuert beide Pack Controller
  |- verarbeitet Safety (Crash, HVIL, Iso)
  |- steuert TSAL/Buzzer/Vehicle Lights
  `- erfasst HV-Bus U/I + Kuehlung + Ladepfad
```

## Kommunikationsdiagramm (Text)

```text
ESS Controller
  <-> Pack Controller Links
  <-> Pack Controller Rechts

Pack Controller
  <-> lokale TLE9012-Chains via isoUART Ring
```

## Safety Signal Flow (Text)

```text
Crash Sensor ----\
HVIL Kette -------> Schuetzfreigabe-Logik -> HV+ / HV- Schuetze (beide Packs)
Iso-Waechter ----/

ESS Controller setzt Fault-Status, TSAL und Buzzer.
Crash/HVIL Hardwarepfade bleiben priorisiert.
```

## isoUART Ring und Enumeration

- Ringtopologie
- Physische Reihenfolge: Bottom -> Top
- Default Enumeration: Bottom -> Top
- Fallback Enumeration: Top -> Bottom
- TLE9012: `Node-ID=0` forwardet nicht, `Node-ID!=0` aktiviert Forwarding

Details:

- `ADR/ADR-002_isoUART_Ring_Enumeration.md`

## Schnittstellenliste

| Ebene | Signal / Interface | Richtung | Domaene | Protokoll | Kritikalitaet |
| --- | --- | --- | --- | --- | --- |
| Stack Controller | Zellspannungen | Eingang | HV-lokal | Analog AFE | Safety |
| Stack Controller | Temp NTC/MUX | Eingang | HV-lokal | Analog AFE | Safety |
| Stack Controller | Balancing Control | Ausgang | HV-lokal | intern TLE | Safety |
| Stack Controller | isoUART Chain | I/O | isoliert | isoUART | Safety |
| Pack Controller | Schuetz HV+ / HV- | Ausgang | LV->HV Aktor | GPIO/Driver | Safety |
| Pack Controller | Fuse Sense (optional) | Eingang | HV->LV Sense | ADC/GPIO | Monitoring |
| Pack Controller | HVIL Segment | Eingang | LV Safety | GPIO | Safety |
| Pack Controller | Feuchtigkeitssensor | Eingang | LV | I2C/ADC (offen) | Monitoring |
| Pack Controller | ESS Link | I/O | LV/isoliert | UART/CAN (offen) | Safety |
| ESS Controller | TSAL Driver | Ausgang | LV->HV Anzeige | GPIO/Driver | Safety |
| ESS Controller | HV-Bus Spannung | Eingang | HV->LV Sense | ADC/isoliert | Safety |
| ESS Controller | HV-Strom HV+ / HV- | Eingang | HV->LV Sense | Analog/isoliert | Safety |
| ESS Controller | Precharge Control | Ausgang | LV->HV Aktor | GPIO/Driver | Safety |
| ESS Controller | Crash Sensor | Eingang | LV Safety | GPIO | Safety |
| ESS Controller | Isolationswaechter | Eingang | LV/isoliert | Digital/Analog (offen) | Safety |
| ESS Controller | Kuehlpumpe / Sensorik | I/O | LV | GPIO/PWM/ADC | Monitoring |
| ESS Controller | Vehicle Lights + Buzzer | Ausgang | LV | GPIO/Driver | Komfort |
| ESS Controller | CCS2 Interface | I/O | LV/isoliert | offen | Safety |

## ProtoA vs ProtoB

ProtoA:

- 2 Packs (links/rechts), verbunden ueber verschraubtes HV-Kabel
- je Pack eigene HV+ und HV- Schuetze und eigene HV Fuse
- Stack-Messkette analog NTC/MUX/TLE mit `3tau` Settling und RDIAG pro Kanal

ProtoB:

- Optionale Topologie LV + HV Supervisor
- HV DC/DC Integration fuer LV-Versorgung
- CCS2 Detailumsetzung und finale Sensorauswahl

## Referenzen

- `ADR/ADR-001_System_Overview_ProtoA.md`
- `ADR/ADR-003_Control_Layers_Stack_Pack_ESS.md`
- `ADR/ADR-004_Controller_Topology_LV_vs_LV+HV.md`
- `subsystems/STACK_CONTROLLER/ARCHITECTURE.md`
- `subsystems/PACK_CONTROLLER/ARCHITECTURE.md`
- `subsystems/SAFETY/Functional_Safety_ProtoA.md`
- `subsystems/VEHICLE_IO/Vehicle_Lighting_and_Signals.md`
- `subsystems/SENSORS/Moisture_and_Cooling.md`
- `CALIBRATION/Temp_Zeroing_PackController.md`

## Offene Punkte / TODO

1. Feuchtigkeitssensor-Verteilung final entscheiden: `1+1` vs `2+0`.
2. ESS<->Pack Protokoll final festlegen (UART/CAN, galvanische Trennung).
3. HV DC/DC Architektur fuer ProtoB festlegen.
4. CCS2 Hardware/Firmware Stack finalisieren.
5. Controller-Topologie Option A/B per Lab validieren.
