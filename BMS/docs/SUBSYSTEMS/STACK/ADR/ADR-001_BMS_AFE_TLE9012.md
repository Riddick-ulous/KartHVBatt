# ADR-001: BMS-AFE auf TLE9012 festgelegt

- Status: Accepted
- Datum: 2026-02-24
- Subsystem: STACK

## Context

ProtoA benoetigt ein kosteneffizientes, integriertes AFE fuer eine identische
`9 x 18S2P`-Stack-Architektur.

## Decision

Der Stack nutzt als BMS-AFE durchgaengig den **Infineon TLE9012**.

## Rationale

- hoher Integrationsgrad fuer ProtoA-TCO
- einheitliche Stack-Hardware fuer schnelle Iteration
- ausreichende Skalierbarkeit fuer den geplanten Stackbereich

## Consequences

- Stack-Design und Mess-/Steuerpfade sind an TLE9012-Faehigkeiten gebunden
- AFE-Wechsel erfordert eigene Migrationsentscheidung

## Related Spec

- `../ARCHITECTURE.md`
- `../../../DATASHEETS/Infineon_TLE9012/`
