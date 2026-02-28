# Stack Architecture 18S2P (ProtoA)

## Scope

Technische Beschreibung der Stackebene fuer ProtoA inklusive Zellabgriff,
Temperaturanbindung und Schnittstelle zum Pack-Controller.

## Fixe Stackdaten

- Anzahl Stacks im System: `9`
- Stackkonfiguration: `18S2P`
- Zellmonitoring pro Stack: `2 x TLE9012` (je ca. 9 Zellen logisch)

## Elektrische Stackstruktur

```text
Stack (18S2P)
  |- TLE9012_A: Zellspannungen Segment A
  |- TLE9012_B: Zellspannungen Segment B
  |- Temp-Frontend analog: NTC + MUX + TLE9012 TMP-Pfade
  `- isoUART Anbindung in Ringtopologie
```

## Zellspannungsabgriff (ProtoA)

- Abgriff ueber verzinnte Kupferdraehte
- Draehte punktgeschweisst auf Busbars
- Weiterfuehrung zum Stack-BMS-PCB

## Temperaturanbindung (ProtoA)

- `10k` NTCs auf kleinen Breakout-PCBs
- Kabel direkt auf NTC-Boards geloetet
- Anschluss am Stack-BMS-PCB
- Messprinzip und Diagnose siehe Temp-ADRs

Referenzen:

- `../ADR/ADR-002_TempSensing_Analog_NTC_MUX.md`
- `../ADR/ADR-003_Temp_FaultContainment_ProtoA.md`
- `../ADR/ADR-004_Filtering_and_Sampling.md`

## isoUART Anbindung

- Jeder TLE9012 ist Teil des Ringbusses.
- Physische Reihenfolge wird Bottom -> Top modelliert.
- Enumeration wird systemseitig durch ESS orchestriert und ueber den
  Pack-Controller-Kommunikationspfad ausgefuehrt.

Referenz:

- `../ADR/ADR-005_isoUART_Ring_Enumeration.md`

## ProtoA vs. ProtoB

ProtoA:

- externer Drahtabgriff und externe NTC-Breakouts
- hoeherer Harness-/Loetaufwand, dafuer schnell iterierbar

ProtoB Ziel:

- integrierte Zell- und Temp-Anbindung
- reduzierte Kabel- und Loetstellenanzahl

## Offene Punkte (Stack-bezogen)

1. finales mechanisches Routing der Abgriffleitungen je Gehaeusevariante
2. finale Connector-Definition zwischen Flex/Breakout und Stack-PCB
3. Fertigungsgrenzen fuer Punktschweissen und Zugentlastung
