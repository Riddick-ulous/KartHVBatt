# ADR-002: isoUART Ringtopologie und Enumeration

- Status: Accepted
- Datum: 2026-02-24
- Geltung: ProtoA

## Context

Die Stackkommunikation muss robust gegen Leitungsfehler sein und nach Wake-up
eine reproduzierbare Node-ID-Vergabe auf allen TLE9012 ermoeglichen.

## Decision

1. isoUART wird als Ring aufgebaut.
2. Standardrichtung fuer Enumeration ist Bottom -> Top.
3. Bei Enumerationsfehler wird auf Gegenrichtung Top -> Bottom umgeschaltet.
4. Master speichert das erfolgreiche Mapping `physische Position -> Node-ID`.

## Technische Grundlagen (TLE9012)

- Nach Wake-up haben alle TLE9012 `Node-ID = 0`.
- `Node-ID = 0` forwardet keine Frames.
- Erst nach Vergabe von `Node-ID != 0` wird Forwarding aktiv.
- IDs werden sequentiell ab masternahem IC vergeben.

## Standard-Enumeration Bottom -> Top

1. Wake-up Pattern senden.
2. Write `Node-ID=0 -> new ID=1`.
3. ACK pruefen, Readback pruefen.
4. Write `Node-ID=0 -> new ID=2`.
5. Wiederholen bis:
- keine Antwort/Timeout, oder
- erwartete Anzahl erreicht.

## Fallback-Enumeration Top -> Bottom

1. Ringrichtung umschalten (TX/RX tauschen).
2. Wake-up und Enumeration erneut.
3. Wenn erfolgreich: Richtung und Mapping persistent speichern.

## Pseudocode

```text
function enumerate_ring(expected_count):
  for direction in [BOTTOM_TO_TOP, TOP_TO_BOTTOM]:
    set_ring_direction(direction)
    send_wakeup()
    wait(t_wakeup_settle)

    assigned = 0
    mapping = []

    for new_id in 1..expected_count:
      resp = write_id_zero_to_new_id(new_id, timeout=t_ack)

      if resp == TIMEOUT:
        break
      if resp == DOUBLE_ACK:
        return recover_double_ack(direction, assigned)

      rb = readback_node_id(new_id, timeout=t_ack)
      if rb != new_id:
        return recover_id_stuck_zero(direction, new_id, rb)

      mapping.append((physical_index=assigned+1, node_id=new_id))
      assigned += 1

    if assigned == expected_count:
      store_mapping(direction, mapping)
      return OK

  return ENUM_FAILED
```

## Zustandsdiagramm (Textform)

```text
IDLE
  -> WAKEUP
WAKEUP
  -> ENUMERATE_FORWARD
ENUMERATE_FORWARD
  -> VERIFY
VERIFY
  -> ENUMERATE_FORWARD (next id)
VERIFY
  -> DONE (expected count reached)
VERIFY
  -> SWITCH_DIRECTION (timeout / id_stuck / double_ack)
SWITCH_DIRECTION
  -> WAKEUP
SWITCH_DIRECTION (already reversed and failed)
  -> FAIL_LOCKOUT
DONE
  -> STORE_MAPPING
STORE_MAPPING
  -> IDLE
```

## Fehlerfaelle und Recovery

### 1) ID bleibt 0

Symptom:

- Readback nach ID-Schreibzugriff liefert nicht `new_id`.

Recovery:

1. denselben Schritt einmal wiederholen.
2. bei erneutem Fehler Enumeration abbrechen.
3. Richtung umschalten und neu starten.
4. Fault-Event loggen: `ENUM_ID_STUCK_ZERO`.

### 2) Doppeltes ACK

Symptom:

- unerwartet mehr als eine gueltige Antwort im ACK-Fenster.

Recovery:

1. Sofortiger Enumerationsabbruch.
2. Wake-up neu senden.
3. Richtung wechseln und neu enumerieren.
4. Bei Wiederholung: Fault-Latch `ENUM_DOUBLE_ACK`.

### 3) Timeout

Symptom:

- keine Antwort innerhalb `t_ack`.

Recovery:

1. aktuellen ID-Schritt einmal retry.
2. weiterhin Timeout: Richtung wechseln und neu starten.
3. wenn beide Richtungen fehlschlagen: `ENUM_FAILED`, System in safe state.

## Parameter (ProtoA Startwerte)

- `t_wakeup_settle = 5 ms`
- `t_ack = 3 ms`
- `max_retry_per_id = 1`
- `max_direction_attempts = 2` (vorwaerts + rueckwaerts)

## Rationale

- Ring bietet alternativen Pfad bei Segmentfehlern.
- Richtungswechsel reduziert Ausfallwirkung bei Leitungsbruch/Knotenausfall.
- Sequentielle ID-Vergabe mit Readback ist eindeutig und debugbar.

## Consequences

- Master-Firmware benoetigt explizite Direction-Control und Mapping-Store.
- Test muss beide Enumerationsrichtungen und Recoverypfade abdecken.

## Validation (Pflicht)

1. Enumeration bei nominalem Ring in beiden Richtungen.
2. Fault-Injection: Leitungsunterbrechung an mehreren Positionen.
3. Fault-Injection: absichtlich blockierender Knoten.
4. Nachweis, dass Mapping nach Power-Cycle reproduzierbar geladen wird.
