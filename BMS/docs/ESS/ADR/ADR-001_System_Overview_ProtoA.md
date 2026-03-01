# ADR-001: System Overview und Scope ProtoA

- Status: Accepted
- Datum: 2026-02-24
- Geltung: Gesamtbatteriesystem ProtoA

## Context

ProtoA benoetigt eine konsistente Systemarchitektur fuer Hardware, Firmware,
Test und Debug mit klarer Trennung zu ProtoB.

## Decision

Die ProtoA-Systemarchitektur wird wie folgt festgelegt:

1. Batterie besteht aus `9` Stacks mit je `18S2P`.
2. Pro Stack werden `2 x TLE9012` eingesetzt.
3. Temperaturmessung bleibt analog mit NTC + MUX + TLE9012.
4. Stackkommunikation erfolgt ueber isoUART in Ringtopologie.
5. Control Layers sind fix getrennt:
- Stack: Monitoring/Balancing/Temp-Scan
- Pack: lokale Schuetze/Fuse/HVIL-Segment/Feuchte
- ESS: Orchestrierung, Safety, TSAL, HV U/I, Vehicle-I/O
6. Packschutz erfolgt ueber Schuetzinfrastruktur je Pack mit HVIL- und Crash-Prioritaet.
7. Fahrzeuginterfaces (Medical/Signal-Lights, TSA-Light) sind ESS-seitig.
8. Messpfade fuer HV-Strom/HV-Spannung sind ESS-seitig zusaetzlich zum Zellmonitoring.

## ProtoA Scope (fix)

- Verdrahteter Zellabgriff ueber punktgeschweisste Drahtanschluesse
- NTC-Breakout-PCBs mit direkter Kabelanbindung
- Hall-Stromsensorik auf separater Sensor-PCB
- Zwei Feuchtigkeitssensoren im Gehaeuse
- Buzzer fuer Status/Warn/Fault

## ProtoB Scope (explizit offen)

- Integrierte Zell-/Temp-Anbindung
- CCS2 Detailumsetzung
- Endgueltige Komponentenselektion einzelner Sensoren/Isowaechter

## Rationale

- Klare Baseline vermeidet unstabile Schnittstellen zwischen Hardware/Firmware/Test.
- Expliziter ProtoA/ProtoB-Schnitt verhindert Vermischung von MVP und Folgegeneration.
- Ring-Topologie und zentrale Mastersteuerung verbessern Debugbarkeit und Fault-Recovery.

## Consequences

- Dokumentation wird entlang der Baseline gefuehrt (Architektur, Stack, Pack, ADRs).
- Offene Punkte bleiben markiert und werden nicht implizit als fix behandelt.

## Related

- `../ARCHITECTURE.md`
- `../subsystems/STACK_CONTROLLER/ARCHITECTURE.md`
- `../subsystems/PACK_CONTROLLER/ARCHITECTURE.md`
- `ADR-002_isoUART_Ring_Enumeration.md`
