# Functional Safety ProtoA

## Scope

Dieses Dokument definiert das Mindestverhalten der Safety-Kette fuer ProtoA:
Crash, HVIL, Isolationswaechter und Schuetzreaktion.

## Safety Inputs

- Crash-Sensor Eingang (systemkritisch)
- HVIL-Kette (Pack-intern + Pack-Connector)
- Isolationswaechter (extern, z. B. Bender)
- Schuetzrueckmeldung (falls verfuegbar)

## Safety Aktoren

- Schuetzsteuerung `HV+` und `HV-` je Pack
- Precharge-Enable/Freigabe
- TSAL-Status
- Buzzer-Status

## Safety Signalfluss

```text
Crash / HVIL / Iso-Fault
  -> Safety-Auswertung (ESS + Hardware-Interlock)
  -> Schuetze oeffnen
  -> Precharge sperren
  -> TSAL in Fault-Status
  -> Buzzer Fault-Muster
```

## Mindestverhalten bei Faults

| Fault | Primarreaktion | Sekundaerreaktion |
| --- | --- | --- |
| Crash Trigger | Schuetze sofort oeffnen | TSAL Fault, Buzzer Fault, Event Log |
| HVIL geoeffnet | Schuetze oeffnen | Precharge sperren, Event Log |
| Iso-Waechter Fault | Schuetze oeffnen (konfigurierbar hart/soft) | TSAL Fault, Buzzer Warn/Fault |
| Schuetzfeedback inkonsistent | erneuter Open-Befehl | Fault Latch + Service Flag |

## Designregeln

1. Crash/HVIL duerfen Schuetzfreigabe hardwareseitig unterbrechen.
2. Software darf Safety-Fault nicht automatisch ruecksetzen.
3. Rueckkehr aus Fault nur ueber definierten Reset-/Servicepfad.
4. Fault-Zeitstempel und Ursache muessen persistent geloggt werden.

## Fault Prioritaet

1. Crash
2. HVIL
3. Isolationswaechter
4. Sonstige Monitoringfaults

## Verifikationsanforderungen (Lab)

1. Crash-Injection: Reaktionszeit bis Schuetzoeffnung messen.
2. HVIL-Unterbrechung an mehreren Punkten der Kette.
3. Iso-Waechter Fault-Injection mit sicherer Schaltreaktion.
4. Kombinationsfaults (z. B. HVIL + Iso) mit Prioritaetsnachweis.

## Offene Punkte / TODO

1. Endgueltige Schwellwerte und Timing fuer Iso-Fault-Reaktion.
2. Finales Schuetzfeedback-Konzept (falls sensorisch verifiziert).
3. Fehlerklasse Warnung vs Fault fuer Iso-Waechter finalisieren.
