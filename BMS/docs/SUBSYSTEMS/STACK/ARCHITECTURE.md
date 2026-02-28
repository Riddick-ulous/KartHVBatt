# Subsystem Architektur: Stack

## Zweck

Dieses Dokument beschreibt die technische Auspraegung des Stack-Subsystems.
Temperatur-spezifische Detailregeln und Ratenberechnungen sind ausgelagert nach:
`../TEMPERATURE/ARCHITECTURE.md`.

## Scope

Enthalten:

- AFE/BMS-IC Auswahl und Rolle
- Zellspannungsmessung und Balancing-Konzept
- Stack-Schnittstellen (Pack-Controller, Zellanschluss, Temp-Frontend)
- Mechanische und Interconnect-Randbedingungen auf Stack-Ebene

Nicht enthalten:

- Temperatur-Timingbudget, `tau`-Auslegung und Sampling-Berechnungen

## Implementierte Systemstruktur (ProtoA)

```text
Stack-PCB
  |- TLE9012 (AFE, Messung, Temperatur-Akquisition)
  |- Zellspannungsanbindung je Seriengruppe
  |- Passives Balancing je Seriengruppe
  |- Temperatur-Frontend-Anbindung (MUX/Filter auf Stack-PCB)
  `- LV-Interconnect zum Pack-Controller
```

## Kernentscheidungen (Stack)

- Ein zentrales Stack-PCB pro Stack
- Alle Stacks identisch aufgebaut
- AFE fix: Infineon TLE9012
- Kein separater Stack-MCU

## Schnittstellen

### Nach oben: Pack-Controller

- Messdatenaggregation
- Diagnose- und Zustandsdaten
- Balancing-Kommandoebene
- Interface- und Aktorikdetails im Pack-Controller-Subsystem:
- `../PACK_CONTROLLER/ARCHITECTURE.md`

### Nach aussen: Zell- und Harness-Anbindung

- Zellanschluss ueber PCB-Pads oder optionale Connector-Loesung
- Low-Voltage Stack-to-Stack bevorzugt ueber Molex Micro-Fit
- Hochstrompfad ueber Busbar-/Pressfit-/Schraubkonzept (projektspezifisch)

### Quer: Temperatur-Subsystem

- Temperaturfuehler auf Flex/Breakout
- MUX und RC-Filter auf Stack-PCB
- Detaildesign in `../TEMPERATURE/ARCHITECTURE.md`

## Offene Punkte (Stack)

- finale Wahl der Zellanschlussmechanik je Fertigungsvariante
- finale High-Current-Verbindung je Einsatzfall (Kart vs. stationaer)

## Zugehoerige ADRs

- `ADR/ADR-001_BMS_AFE_TLE9012.md`
