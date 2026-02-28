# ADR-004: Filterung und Sampling-Regeln

- Status: Accepted
- Datum: 2026-02-24
- Subsystem: TEMPERATURE

## Context

Das Temperatur-Frontend muss EMV-Stoerungen daempfen und trotzdem die
Zielabtastraten erreichen.

## Decision

- RC-Filter wird hinter dem MUX platziert
- Messsequenz pro Kanal ist fix:
1. MUX schalten
2. `>=3tau` warten
3. ADC sampeln

## Rationale

- geringe Bauteilzahl bei klarer Timing-Regel
- reproduzierbarer Messpfad fuer Scheduler und Test

## Consequences

- RC-Auslegung und Sampling-Budget sind gekoppelt
- Zielraten sind nur mit validiertem Timingbudget freizugeben

## Related Spec

- `../ARCHITECTURE.md`
