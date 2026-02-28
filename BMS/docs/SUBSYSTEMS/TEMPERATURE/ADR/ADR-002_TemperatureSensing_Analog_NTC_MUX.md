# ADR-002: Temperaturmessung analog mit NTC und MUX

- Status: Accepted
- Datum: 2026-02-24
- Subsystem: TEMPERATURE

## Context

ProtoA braucht `18` Temperaturkanaele pro Stack mit robustem Verhalten bei
Lastwechseln und niedrigen Prototyping-Kosten.

## Decision

Temperaturmessung wird analog umgesetzt mit:

- `0603` NTCs auf Flex/Breakout
- MUX auf Stack-PCB
- einem RC-Filter hinter dem MUX
- ADC-Erfassung ueber TLE9012

## Rationale

- geringere Kosten und Integrationsaufwand im ProtoA-Horizont
- passive, austauschbare Flex-Sensorik fuer schnelle Iteration
- bessere analoge Signalintegritaet durch MUX-Position auf Stack-PCB

## Consequences

- Messrate ist direkt von Kanalzahl, `tau` und ADC/Scheduler-Timing abhaengig
- Kanalumschaltung muss die `>=3tau`-Regel einhalten

## Related Spec

- `../ARCHITECTURE.md`
