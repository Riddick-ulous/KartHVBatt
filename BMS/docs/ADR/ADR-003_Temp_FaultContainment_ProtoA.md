# ADR-003: Temp Fault-Containment ProtoA (>=50%)

- Status: Accepted
- Datum: 2026-02-24
- Geltung: ProtoA

## Requirement

Ein Einzelfehler darf pro Stack nicht zum Totalausfall der Temperaturmessung fuehren.
Mindestens `>= 50%` der Temp-Kanaele muessen verfuegbar bleiben.
Fuer ProtoA-Basisset entspricht das `>= 9/18`.

## Architekturannahme fuer Nachweis

- Temp-Subsystem ist in zwei unabhaengige Domains segmentiert.
- Domain A: TLE9012-A + eigene MUX/RC-Pfade + eigenes Flex-Segment.
- Domain B: TLE9012-B + eigene MUX/RC-Pfade + eigenes Flex-Segment.
- Safety-relevante Temp-Kanaele sind 9/9 auf beide Domains verteilt.

Hinweis:

- Das MUX-Schema unterstuetzt bis zu 12 Tempkanaele pro TLE.
- Der 9/18-Safetynachweis bezieht sich auf den ProtoA-Basissatz mit 18 Safety-Kanaelen.

## Fault Matrix

| Fault | Effekt lokal | Verbleibende Kanaele (18-Basis) | >=50% |
| --- | --- | --- | --- |
| NTC open | 1 Kanal ungueltig | 17/18 | ja |
| NTC short | 1 Kanal ungueltig | 17/18 | ja |
| Leitung offen im Segment (ein NTC-Pfad) | 1 Kanal ungueltig | 17/18 | ja |
| Leitungskurz im Segment (ein NTC-Pfad) | 1 Kanal ungueltig | 17/18 | ja |
| MUX defekt (ein TMUX1309A) | bis zu 2 TMP-Pfade betroffen | >=12/18 | ja |
| Select-Leitungen stuck in Domain A | Domain A degradiert, Domain B intakt | >=9/18 | ja |
| RC-Kondensator kurz (ein TMP-Pfad) | Pfad blockiert/geklemmt | >=15/18 | ja |
| RC-Kondensator offen (ein TMP-Pfad) | erhoehte Stoeranfaelligkeit auf 1 Pfad | >=17/18 | ja |
| ESD-Event am Flex-Connector einer Domain | Domain-Ausfall moeglich | 9/18 | ja |
| TLE9012-Ausfall einer Domain | komplette Domain weg | 9/18 | ja |

## Diagnosebezug

- `S=11` Diagnosezustand prueft je TMP-Pfad auf korrekten `RDIAG_x`.
- Bei Domain-Teilfehlern bleibt Gegen-Domain voll messfaehig.
- Fehlerisolation erfolgt pro Kanal, pro Pfad und pro Domain.

## Consequences

- Segmentgrenzen muessen elektrisch sauber trennbar sein (Pfad, Massebezug, Connector-Pins).
- Firmware muss degradierte Modi (9/18) explizit behandeln und loggen.
- Jeder Fault aus der Matrix braucht testbare Kriterien im HIL/Lab.

## Testpflicht (ProtoA)

1. Fault-Injection je Matrixzeile mit Nachweis verbleibender valid channels.
2. Nachweis der Mindestverfuegbarkeit `>= 9/18` fuer alle Domain-Einzelfehler.
3. Diagnose-Latenz je Faultklasse dokumentieren.
