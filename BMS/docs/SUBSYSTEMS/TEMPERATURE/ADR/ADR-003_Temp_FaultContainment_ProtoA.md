# ADR-003: Fault-Containment fuer Temperaturkanaele

- Status: Accepted
- Datum: 2026-02-24
- Subsystem: TEMPERATURE

## Context

Ein Einzelfehler darf in ProtoA nicht zum Totalverlust der Temperaturmessung je Stack fuehren.

## Decision

Mindestanforderung:

- nach Einzelfehler bleiben mindestens `9/18` Kanaele verfuegbar (`>=50%`)

Architekturrichtung:

- Segmentierung mindestens `2x9` oder funktional gleichwertig

## Rationale

- definierte degradierte Betriebsfaehigkeit statt Totalausfall
- klare Sicherheits- und Diagnosegrenze fuer ProtoA

## Consequences

- Segmentierungslogik in MUX-/Connector-/Flex-Topologie erforderlich
- Single-Point-Failure ueber alle 18 Kanaele ist unzulaessig

## Related Spec

- `../ARCHITECTURE.md`
