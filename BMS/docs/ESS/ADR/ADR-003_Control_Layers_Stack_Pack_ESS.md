# ADR-003: Control Layers Stack / Pack / ESS

- Status: Accepted
- Datum: 2026-02-24
- Geltung: ProtoA

## Context

Das System benoetigt klare funktionale Grenzen zwischen lokaler Mess-/Aktoriklogik
und systemweiter Orchestrierung, damit Service und Debug reproduzierbar bleiben.

## Decision

Die Funktionszuordnung ist fix auf drei Ebenen:

1. Stack Controller Ebene (pro Stack):
- TLE-Chain Cell-Monitoring
- Cell Balancing
- Temp-Monitoring NTC/MUX inkl. Scan/Diagnose

2. Pack Controller Ebene (pro Pack):
- Schuetzsteuerung HV+ / HV-
- optionale Fuse-Ueberwachung
- HVIL-Segment intern + pack-connector
- Feuchtigkeitssensorik auf Packebene

3. ESS Controller Ebene (einmal systemweit):
- TSAL, HV-Bus U/I, Precharge, Kuehlung, Buzzer
- Crash/Iso/HVIL-Auswertung auf Systemebene
- Fahrzeug-I/O und CCS2
- Zeroing/Kalibrierung zentral

## Rationale

- Lokale Fehler bleiben auf Stack/Pack eingrenzbar.
- ESS bleibt zentrale Entscheidungsinstanz fuer systemweite Freigaben.
- Firmwaretest und Service koennen entlang klarer Ebenen partitioniert werden.

## Consequences fuer Debug/Service

- Fehlerklassifikation erfolgt mit Ebenenbezug (Stack, Pack, ESS).
- Pack-Ausfall darf nicht automatisch den zweiten Packpfad maskieren.
- Serviceprozeduren erhalten getrennte Testplaene je Ebene:
- Stack: Messqualitaet/Temp/Balancing
- Pack: Schuetz/HVIL/Fuse
- ESS: Systemfreigabe/Signalisierung/Ladepfad

## Schnittstellenimplikation

- ESS <-> Pack Interface muss Fault-Status, Schuetzstate und Sensorzustand eindeutig transportieren.
- Pack <-> Stack Interface muss TLE-Enumeration und Messdaten robust kapseln.
- Crash/HVIL Hardwarepfad bleibt priorisiert gegenueber rein softwarebasierter Freigabe.

## Offene Punkte

1. Finales ESS<->Pack Kommunikationsprotokoll.
2. Endgueltige Positionierung der zwei Feuchtigkeitssensoren.
3. Detailgrenzen zwischen Pack-Aktorik und ESS-Freigabelogik in Firmware.

## Related

- `../ARCHITECTURE.md`
- `ADR-004_Controller_Topology_LV_vs_LV+HV.md`
- `../subsystems/PACK_CONTROLLER/ARCHITECTURE.md`
- `../subsystems/SAFETY/Functional_Safety_ProtoA.md`
