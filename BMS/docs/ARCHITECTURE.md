# BMS Architekturuebersicht (ProtoA)

## Zweck

Systemweite Architektur fuer ProtoA mit klarer Zuordnung der Funktionen auf drei
Steuerebenen: Stack Controller, Pack Controller, ESS Controller.

## 3-Level Control Architecture (Stack/Pack/ESS)

### Ebene 1: Stack Controller (pro Stack)

- Cell-Monitoring ueber TLE9012-Chain
- Cell-Balancing
- Temp-Monitoring ueber `10k NTC + TMUX1309A + TLE9012`
- MUX-Select/Scan/Diagnose durch TLE9012

### Ebene 2: Pack Controller (pro Pack, 2x)

- Lokale Schuetz-Ansteuerung `HV+` / `HV-`
- Lokale Fuse-Ueberwachung (falls Fuse-Sense bestueckt)
- HVIL-Segment (pack-intern + pack-connector)
- Pack-interne Feuchtigkeitssensorik (`2` Sensoren gesamt, Verteilung offen)

### Ebene 3: ESS Controller (systemweit, 1x)

- Zentraler Orchestrator fuer beide Packs
- TSAL-Logik + TSAL-Treiber
- HV-Bus-Spannungsmessung
- HV-Strommessung `HV+` und `HV-` inkl. Kalibrierpfad (Hochlast-Widerstaende)
- Precharge-System (bestehendes Design referenziert)
- Wasserkuehlungssteuerung (Pumpe + Sensorik)
- Akustische Signalisierung (Piezo/Buzzer)
- Crash-Sensor Eingang (systemkritisch)
- Isolationswaechter-Integration
- CCS2 Ladeinterface
- Fahrzeug-I/O (Medical/Red/Green/Rain/Ready-to-move)
- HV DC/DC Anbindung fuer LV (offen, ProtoB)

## Blockdiagramme (Textform)

### System-Blockdiagramm

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

### Kommunikationsdiagramm

```text
ESS Controller
  <-> Pack Controller Links
  <-> Pack Controller Rechts

Pack Controller
  <-> isoUART Ring zu lokalen TLE9012 (Bottom->Top default, Top->Bottom fallback)
```

### Safety Signal Flow

```text
Crash Sensor ----\
HVIL Kette -------> Schuetzfreigabe-Logik -> HV+ / HV- Schuetze (beide Packs)
Iso-Waechter ----/

ESS Controller bewertet Signale und setzt Fault-Status, TSAL und Buzzer.
Hardwarepfad Crash/HVIL bleibt sicherheitskritisch priorisiert.
```

## isoUART Ring und Enumeration (ProtoA)

- Topologie: Ring
- Physische Reihenfolge: Bottom -> Top
- Default Enumeration: Bottom -> Top
- Fallback Enumeration: Top -> Bottom (Richtungsumschaltung)
- TLE9012 Verhalten: `Node-ID=0` forwardet nicht, `Node-ID!=0` forwardet

Detail:

- `ADR/ADR-005_isoUART_Ring_Enumeration.md`

## Schnittstellenliste (Stack/Pack/ESS)

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

ProtoA (fix):

- 2 Packs (links/rechts), verbunden ueber verschraubtes HV-Kabel
- je Pack eigene HV+ und HV- Schuetze und eigene HV Fuse
- Stack-Messkette analog NTC/MUX/TLE mit 3tau Settling und RDIAG pro Kanal
- Zeroing zentral im ESS Controller

ProtoB (Ausblick):

- Optionale Zweicontroller-Topologie LV + HV Supervisor
- HV DC/DC Integration fuer LV-Versorgung
- CCS2 Detailimplementation und finale Sensorauswahl

## Referenzdokumente

- `ADR/ADR-001_System_Overview_ProtoA.md`
- `ADR/ADR-006_Control_Layers_Stack_Pack_ESS.md`
- `ADR/ADR-007_Controller_Topology_LV_vs_LV+HV.md`
- `STACK/Stack_Architecture_18S2P.md`
- `PACK/Pack_Controller_Overview.md`
- `SAFETY/Functional_Safety_ProtoA.md`
- `IO/Vehicle_Lighting_and_Signals.md`
- `SENSORS/Moisture_and_Cooling.md`

## Offene Punkte / TODO

1. Feuchtigkeitssensor-Verteilung final entscheiden: `1+1` pro Pack vs `2+0`.
2. ESS<->Pack Protokoll final festlegen (UART/CAN, galvanische Trennung).
3. HV DC/DC Architektur und Schutzkette fuer ProtoB festlegen.
4. CCS2 Stackup fuer Hardware/Firmware finalisieren.
5. Controller-Topologie Option A/B durch Lab-Validierung schliessen.



https://www.mouser.de/ProductDetail/Adam-Tech/EC2-05D12P3?qs=ZcfC38r4Pou03elb1ikkgg%3D%3D

EC2-05D12P3 als standard supply für HV meas und Precharge mit nachgeschaltetem LDO auf 5V bzw. 10V oder so