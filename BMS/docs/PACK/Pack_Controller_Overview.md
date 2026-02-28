# Pack Controller Overview (ProtoA)

## Scope

Uebersicht der Pack-Controller-Funktionen, externen Schnittstellen und
HV-Infrastruktur auf Packebene.

## Kernfunktionen (fix)

- Lokale Schuetz-Ansteuerung `HV+` / `HV-` je Pack
- Lokale Fuse-Ueberwachung (falls Fuse-Sense bestueckt)
- HVIL-Segment (pack-intern + pack-connector)
- Packinterne Feuchtigkeitssensorik
- Kommunikationsknoten zwischen ESS Controller und lokalen Stack/TLE Chains

## Schuetze und Fuse

- Je Pack eigene `HV+` und `HV-` Schuetze
- Je Pack eigene HV Fuse
- Schuetzfreigabe wird durch ESS Safetylogik vorgegeben
- Lokale Aktoransteuerung bleibt auf Packebene

## Feuchtigkeitssensorik

- `2` Feuchtigkeitssensoren im Batteriegehaeuse
- Offene Verteilung: `1+1` pro Pack oder `2+0`
- Ziel: Kondensation und Leckage erkennen
- Reaktion: Logging + konfigurierbarer Fault ueber ESS

## Kommunikation

- Upstream: ESS Controller <-> Pack Controller
- Downstream: Pack Controller <-> lokale TLE9012 Chains (isoUART Ring)
- Enumeration default Bottom -> Top, Fallback Top -> Bottom
- Mapping wird systemseitig durch ESS verwaltet

## Packkonfiguration (Kart)

- Zwei mechanisch getrennte Batteriepakete: links/rechts
- Hauptschuetze trennen links/rechts voneinander
- Hauptschuetze trennen nicht jedes Einzelpack separat vom Inverter
- gemeinsamer Satz Schuetze fuer beide Seiten

## Offene Punkte (bewusst offen in ProtoA)

1. Feuchtigkeitssensor-Verteilung `1+1` vs `2+0`
2. ESS<->Pack Protokollfinalisierung
3. Fuse-Sense Bestueckvariante pro Pack

## Referenzen

- `../ARCHITECTURE.md`
- `../ADR/ADR-001_System_Overview_ProtoA.md`
- `../ADR/ADR-005_isoUART_Ring_Enumeration.md`
- `../ADR/ADR-006_Control_Layers_Stack_Pack_ESS.md`
- `../SAFETY/Functional_Safety_ProtoA.md`
