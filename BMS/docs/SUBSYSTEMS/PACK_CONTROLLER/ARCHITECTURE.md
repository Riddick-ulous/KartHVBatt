# Subsystem Architektur: Pack Controller

## Zweck

Dieses Dokument beschreibt die technische Auspraegung des Pack-Controller-Subsystems
fuer ProtoA. Es definiert Komponentenrollen, funktionale Schnittstellen und
Aktorik-Interlocks ohne GPIO-, Pinout- oder Protokolldetails.

## Scope

Enthalten:

- MCU- und Kommunikationsbausteine des Pack Controllers
- Funktionale Anbindung an BMS-Stacks, Isowaechter und Stromsensor
- Aktorik fuer Schuetze, Precharge und HV-Indikator
- Interlock-Regeln auf Aktorebene

Nicht enthalten:

- Bus-/Protokollfestlegung (wenn nicht final bestaetigt)
- GPIO-/Pin-Mapping
- Detaillierter Zustandsautomat

## Implementierte Systemstruktur (ProtoA)

```text
Pack-Controller-PCB
  |- STM32G474 (zentrale Steuerung)
  |- TLE9015 (BMS Stack Communication Interface)
  |- Interface zum Isowaechter (funktional)
  |- Interface zum Stromsensor (funktional)
  |- Schuetztreiber Main+ / Main- / Precharge
  `- HV Indicator Output (rote LED)
```

## Komponenten und Rollen

- `STM32G474`: zentrale Steuerung, Diagnosekoordination, Aktorikfreigaben
- `TLE9015`: Kommunikationsinterface zum Stack-BMS-Netzwerk
- `Isowaechter Interface`: Einlesen von Isolationsstatus fuer Sicherheitsfreigabe
- `Current Sensor Interface`: Erfassung Stromwert und Grenzwertbeobachtung
- `Contactor Outputs`: Ansteuerung `Main+`, `Main-`, `Precharge`
- `HV Indicator Output`: rote LED, zeigt HV-aktive Betriebsphase

## Schnittstellenmatrix (funktional)

| Interface | Richtung | Signalrolle | Safety-Relevanz | Status |
| --- | --- | --- | --- | --- |
| BMS Stack Communication (via TLE9015) | bidirektional | Stackdaten erfassen, Kommandos senden | hoch | implemented |
| Insulation Monitor Interface | Eingang | Isolationsstatus und Fault-Info | hoch | implemented |
| Current Sensor Interface | Eingang | Packstrom fuer Grenzwerte und Diagnose | hoch | implemented |
| Contactor Control Output Main+ | Ausgang | Hauptschuetz positiv schalten | hoch | implemented |
| Contactor Control Output Main- | Ausgang | Hauptschuetz negativ schalten | hoch | implemented |
| Contactor Control Output Precharge | Ausgang | Vorladepfad schalten | hoch | implemented |
| HV Indicator Output (rote LED) | Ausgang | HV-aktive Phase sichtbar anzeigen | mittel | implemented |

## Aktorik und Interlocks

Topologie:

- `2` Hauptschuetze (`Main+`, `Main-`)
- `1` Precharge-Pfad

Interlock-Regeln auf Aktorebene:

- Schliessen von `Main+` und `Main-` nur mit gueltiger Sicherheitsfreigabe
- Precharge-Ausgang ist separat steuerbar, keine implizite Kopplung auf GPIO-Ebene dokumentiert
- Rote HV-LED: **an bei HV active**

## Offene Punkte

- finales Protokoll/Bussystem fuer Isowaechter-Interface
- finales Protokoll/Bussystem fuer Stromsensor-Interface
- detaillierte zeitliche Schaltlogik zwischen Precharge und Hauptschuetzen

## Zugehoerige ADRs

- `ADR/ADR-001_PackController_STM32G474_TLE9015.md`
