# BMS Dokumentation (ProtoA)

Diese Dokumentation ist hierarchisch organisiert:

1. Globale Systemarchitektur
2. Subsystem-Architekturen
3. ADRs je Subsystem

## Einstieg

1. `ARCHITECTURE.md`
2. `ADR/ADR-001_System_Overview_ProtoA.md`
3. `STACK/Stack_Architecture_18S2P.md`
4. `PACK/Pack_Controller_Overview.md`
5. `ADR/ADR-005_isoUART_Ring_Enumeration.md`
6. `ADR/ADR-006_Control_Layers_Stack_Pack_ESS.md`
7. `ADR/ADR-007_Controller_Topology_LV_vs_LV+HV.md`
8. Temp-Detaildokumente (`ADR-002..004`, `SCHEMATIC_NOTES`, `CALIBRATION`)
9. `SAFETY/Functional_Safety_ProtoA.md`, `IO/Vehicle_Lighting_and_Signals.md`, `SENSORS/Moisture_and_Cooling.md`

## Struktur

- `ARCHITECTURE.md`: globaler Systemkontext und Subsystem-Landkarte
- `STACK/Stack_Architecture_18S2P.md`: Stackaufbau 18S2P mit 2x TLE9012
- `PACK/Pack_Controller_Overview.md`: Masterfunktionen, HV-Infrastruktur, Fahrzeuginterfaces
- `SAFETY/Functional_Safety_ProtoA.md`: Crash/HVIL/Iso und Mindest-Faultverhalten
- `IO/Vehicle_Lighting_and_Signals.md`: Fahrzeugsignale, TSAL und Buzzer
- `SENSORS/Moisture_and_Cooling.md`: Feuchtigkeitssensorik und Kuehlungslogik
- `SUBSYSTEMS/STACK/ARCHITECTURE.md`: Stack-Subsystem (AFE, Zellmessung, Balancing, Schnittstellen)
- `SUBSYSTEMS/TEMPERATURE/ARCHITECTURE.md`: Temperatur-Subsystem inklusive Timing- und Ratenbudget
- `SUBSYSTEMS/PACK_CONTROLLER/ARCHITECTURE.md`: Pack-Controller (MCU, BMS-Comm, Peripherie, Aktorik)
- `SUBSYSTEMS/*/ADR/`: Entscheidungen pro Subsystem (kurz und stabil)
- `ADR/`: Temp-Subsystem ADR-Set mit Schaltungs-/Timing-/Fault-Spezifikation
- `SCHEMATIC_NOTES/`: Pin-nahe Verdrahtungs- und Implementierungsnotizen
- `CALIBRATION/`: Inbetriebnahme- und Kalibrierverfahren
- `DATASHEETS/`: Herstellerdatenblaetter und App Notes

## ADR Index

- `ADR/ADR-001_System_Overview_ProtoA.md`
- `ADR/ADR-006_Control_Layers_Stack_Pack_ESS.md`
- `ADR/ADR-007_Controller_Topology_LV_vs_LV+HV.md`
- `ADR/ADR-005_isoUART_Ring_Enumeration.md`
- `SUBSYSTEMS/STACK/ADR/ADR-001_BMS_AFE_TLE9012.md`
- `SUBSYSTEMS/TEMPERATURE/ADR/ADR-002_TemperatureSensing_Analog_NTC_MUX.md`
- `SUBSYSTEMS/TEMPERATURE/ADR/ADR-003_Temp_FaultContainment_ProtoA.md`
- `SUBSYSTEMS/TEMPERATURE/ADR/ADR-004_Filtering_and_Sampling.md`
- `SUBSYSTEMS/PACK_CONTROLLER/ADR/ADR-001_PackController_STM32G474_TLE9015.md`
- `ADR/ADR-002_TempSensing_Analog_NTC_MUX.md`
- `ADR/ADR-003_Temp_FaultContainment_ProtoA.md`
- `ADR/ADR-004_Filtering_and_Sampling.md`
