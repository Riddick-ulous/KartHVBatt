# Architektur: STACK_CONTROLLER (ProtoA)

## Zweck

Source of Truth fuer stacklokale Funktionen: Zellmonitoring, Balancing,
Temperaturmesskette und isoUART-Anbindung je Stack.

## Scope

Enthalten:

- `18S2P` Stackaufbau
- `2 x TLE9012` pro Stack
- Zellabgriff und Busbar-Anbindung
- Temp-Frontend Summary (Detail in ADRs + Schematic Notes)
- isoUART Reihenfolge und Enumerationsbezug

Nicht enthalten:

- Systemweite Schuetz-/Precharge-Logik
- ESS-weite Safety-Freigabelogik

## Fixe Stackdaten

- Anzahl Stacks gesamt: `9`
- Stackkonfiguration: `18S2P`
- Zellmonitoring pro Stack: `2 x TLE9012` (je ~9 Zellen logisch)

## Elektrische Stackstruktur

```text
Stack (18S2P)
  |- TLE9012_A: Zellspannungen Segment A
  |- TLE9012_B: Zellspannungen Segment B
  |- Temp-Frontend: NTC + TMUX1309A + RC + TLE9012 TMP
  `- isoUART Ringanbindung
```

## Zellspannungsabgriff (ProtoA)

- verzinnte Kupferdraehte
- punktgeschweisst auf Busbars
- Weiterfuehrung zum Stack-BMS-PCB

## Temperaturmessung (Summary)

- `10k` NTCs auf Breakout-PCBs
- analoger MUX (`TMUX1309A`) pro TLE-Pfadgruppe
- RC hinter MUX, Messung nach `>= 3tau`
- Diagnose mit separatem `RDIAG` pro TMP-Kanal
- Zeroing zentral in ESS-Ebene

Detaildokumente:

- `ADR/ADR-002_TempSensing_Analog_NTC_MUX.md`
- `ADR/ADR-003_Temp_FaultContainment_ProtoA.md`
- `ADR/ADR-004_Filtering_and_Sampling.md`
- `SCHEMATIC_NOTES/Temp_MUX_NTC_TLE9012.md`

## isoUART Anbindung

- Jeder TLE9012 ist Teil des Ringbusses.
- Physische Reihenfolge: Bottom -> Top.
- Enumeration wird ESS-seitig orchestriert, ueber Pack-Kommunikationspfad ausgefuehrt.

Referenz:

- `../../ADR/ADR-002_isoUART_Ring_Enumeration.md`

## ProtoA vs ProtoB

ProtoA:

- externer Drahtabgriff und externe NTC-Breakouts
- hoeherer Harness-/Loetaufwand, dafuer schnelle Iteration

ProtoB Ziel:

- integrierte Zell- und Temp-Anbindung
- reduzierte Kabel-/Loetstellenanzahl

## Offene Punkte

1. finales mechanisches Routing der Abgriffleitungen.
2. finale Connector-Definition Flex/Breakout zu Stack-PCB.
3. Fertigungsgrenzen Punktschweissen/Zugentlastung.
