# BMS Dokumentation (ProtoA)

Kanonische Doku-Struktur ist `docs/ESS`.
Alle System- und Subsystementscheidungen werden dort gepflegt.

## Einstieg

1. `ESS/ARCHITECTURE.md`
2. `ESS/ADR/ADR-001_System_Overview_ProtoA.md`
3. `ESS/ADR/ADR-002_isoUART_Ring_Enumeration.md`
4. `ESS/ADR/ADR-003_Control_Layers_Stack_Pack_ESS.md`
5. `ESS/ADR/ADR-004_Controller_Topology_LV_vs_LV+HV.md`
6. `ESS/subsystems/STACK_CONTROLLER/ARCHITECTURE.md`
7. `ESS/subsystems/PACK_CONTROLLER/ARCHITECTURE.md`
8. `ESS/subsystems/SAFETY/Functional_Safety_ProtoA.md`
9. `ESS/subsystems/VEHICLE_IO/Vehicle_Lighting_and_Signals.md`
10. `ESS/subsystems/SENSORS/Moisture_and_Cooling.md`
11. `ESS/CALIBRATION/Temp_Zeroing_PackController.md`

## Struktur

- `ESS/ARCHITECTURE.md`: systemweite ESS-Architektur und 3-Level-Control
- `ESS/ADR/`: systemweite ADRs
- `ESS/subsystems/STACK_CONTROLLER/`: stackspezifische Architektur, ADRs, Schematic Notes
- `ESS/subsystems/PACK_CONTROLLER/`: packspezifische Architektur und ADRs
- `ESS/subsystems/SAFETY/`: funktionale Safety-Spezifikation
- `ESS/subsystems/VEHICLE_IO/`: Fahrzeuglicht- und Signaldokumentation
- `ESS/subsystems/SENSORS/`: Feuchtigkeit/Kuehlung
- `ESS/CALIBRATION/`: Inbetriebnahme- und Kalibrierdokumente
- `DATASHEETS/`: globale Referenzdatenblaetter

## Ownership-Regel

- Jede fachliche Anforderung hat genau einen Dokument-Owner.
- Weitere Nennungen erfolgen nur als Verweis, nicht als parallele Spezifikation.
