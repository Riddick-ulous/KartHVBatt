# ADR-004: Controller Topology LV vs LV+HV Supervisor

- Status: Proposed (offen)
- Datum: 2026-02-24
- Geltung: ProtoA -> ProtoB Pfad

## Context

Die systemweite Controller-Topologie ist fuer ProtoA noch offen:

- Option A: nur LV-seitiger Hauptcontroller (ESS Controller)
- Option B: LV + HV Controller mit isolierter serieller Kopplung

## Optionen

### Option A: Single LV Controller

- Alle ESS-Funktionen auf einem LV-Controller
- HV-nahe Signale ueber isolierte Messpfade/Interfaces an LV

### Option B: LV + HV-Side Supervisor

- HV-nahe MCU uebernimmt lokal:
- HV-Busspannungsmessung
- Stromsensor-Auslese + Kalibrierpfad
- TSAL-Treiber
- optional hartes Crash/HVIL-Gating
- Kommunikation HV-MCU <-> LV-Controller ueber isolierte serielle Verbindung

## Bewertungsmatrix

| Kriterium | Option A (LV only) | Option B (LV+HV) |
| --- | --- | --- |
| BOM-Kosten/Fertigung | besser | schlechter |
| EMV/Signalqualitaet Drive<->Recup | mittel | besser |
| Safety/Fault-Containment | mittel | besser |
| Debuggability/Inbetriebnahme | besser in ProtoA | hoeherer Aufwand |
| Skalierbarkeit ProtoA->ProtoB | mittel | besser |

## ProtoA Default Empfehlung

Empfehlung fuer ProtoA: **Option A (LV only)**.

Begruendung:

- niedriger BOM- und Integrationsaufwand fuer `1 + (2..5)` Packs.
- kuerzere Inbetriebnahmezeit bei bestehendem Entwicklungsstand.
- ausreichende Basis fuer Funktionsnachweis mit klaren Safety-Tests.

## ProtoB Upgrade Path

Empfehlung fuer ProtoB: **Option B als geplante Erweiterung**.

Begruendung:

- bessere HV-nahe Signalqualitaet unter dynamischer Last.
- staerkere Fault-Containment-Grenzen zwischen HV-Messung und LV-Orchestrierung.
- robustere Erweiterbarkeit fuer komplexere Lade-/Safety-Funktionen.

## Reserve im ProtoA-Layout (zwingend vorsehen)

1. Footprint fuer HV-Side MCU (oder Modulheader).
2. Isolierter Serienkanal ESS<->HV MCU:
- mindestens `TX_ISO`, `RX_ISO`, `GND_ISO_REF` (falls erforderlich).
3. Reserve fuer optionales SPI/CAN isoliert:
- `SCK/MOSI/MISO/CS` oder `CAN_H/CAN_L`.
4. Zusaeztliche Enable/Gating-Leitung:
- `HV_ENABLE_GATE` zwischen ESS und HV-Aktorikpfad.
5. Messpfad-Abgriffpunkte als Testheader:
- HV-Busspannung, HV+ Strom, HV- Strom, TSAL-Drive-Monitor.

## Entscheidungs-/Lab-Kriterien zum Schliessen der ADR

### 1) EMV Test Drive<->Recup

- Vergleich Option A Baseline gegen HV-nahe Messwerterfassung.
- Bewertung von Signalrauschen, Latenz und TSAL-Stabilitaet.

### 2) Fault-Injection

- Crash-Sensor Trigger
- HVIL Oeffnung
- Iso-Waechter Fault
- Schuetzsteuerungsfehler
- Erwartetes Verhalten: kontrolliertes Oeffnen der Schuetze, TSAL/Buzzer Statuskonsistenz.

### 3) Kommunikationsrobustheit

- isoUART Ring Enumeration inkl. Richtungsfallback.
- ESS<->Pack Link unter Last und Stoerung.

## Annahmen

1. Isolierter Serienlink fuer Option B ist hardwareseitig in ProtoA vorbereitet, aber unbestueckt.
2. Bestehendes Precharge-/Schuetzdesign bleibt fuer ProtoA unveraendert.

## Related

- `../ARCHITECTURE.md`
- `ADR-003_Control_Layers_Stack_Pack_ESS.md`
- `../subsystems/SAFETY/Functional_Safety_ProtoA.md`
