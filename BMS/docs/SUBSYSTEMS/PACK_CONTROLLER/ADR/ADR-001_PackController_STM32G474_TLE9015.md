# ADR-001: Pack Controller mit STM32G474 und TLE9015

- Status: Accepted
- Datum: 2026-02-24
- Subsystem: PACK_CONTROLLER

## Context

ProtoA benoetigt eine zentrale Pack-Controller-Instanz, die Stack-Kommunikation,
Sicherheitsperipherie und HV-Aktorik koordiniert.

## Decision

Der Pack Controller wird mit folgenden Kernkomponenten umgesetzt:

- `STM32G474` als zentrale MCU
- `TLE9015` als BMS-Kommunikationsinterface

Der Pack Controller uebernimmt zentral die Koordination von:

- Isowaechter-Interface
- Stromsensor-Interface
- Schuetz- und Precharge-Ansteuerung
- HV-Indikator (rote LED)

## Rationale

- klare zentrale Verantwortlichkeit fuer sicherheitsrelevante Pack-Funktionen
- konsistente Trennung zwischen Stack-Subsystem und Pack-Steuerung
- robuste Erweiterbarkeit bei unveraenderter Stack-Hardware

## Consequences

- Verfuegbarkeit und Verhalten des Pack Controllers sind kritischer Systempfad
- Schnittstellen zu Isowaechter und Stromsensor muessen funktional klar abgegrenzt bleiben
- Detaillierte Bus-/Pin-Definitionen werden in separaten Implementierungsdokumenten gepflegt

## Related Spec

- `../ARCHITECTURE.md`
- `../../../DATASHEETS/STM32G474/`
- `../../../DATASHEETS/TLE9015/`
- `../../../DATASHEETS/Isowaechter/`
- `../../../DATASHEETS/Current_Sensor/`
- `../../../DATASHEETS/Contactors_Precharge/`
