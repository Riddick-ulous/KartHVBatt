# Architektur: PACK_CONTROLLER (ProtoA)

## Zweck

Source of Truth fuer packlokale Funktionen: lokale HV-Aktorik, HVIL-Segment,
Fuse-Sense-Option und Kommunikationsknoten zwischen ESS und Stackebene.

## Scope

Enthalten:

- lokale Schuetzansteuerung `HV+` / `HV-` je Pack
- lokale HV Fuse und optionale Fuse-Sense
- HVIL-Segment pack-intern + pack-connector
- Feuchtigkeitssensorik auf Packebene
- ESS<->Pack sowie Pack<->Stack Kommunikationsknoten

Nicht enthalten:

- TSAL, CCS2, HV-Bus U/I, Kuehlung, zentrale Safety-Policy

## Kernfunktionen

- Lokale Schuetz-Ansteuerung `HV+` / `HV-` je Pack
- Lokale Fuse-Ueberwachung (falls Sense bestueckt)
- HVIL-Segment je Pack
- Packinterne Feuchtigkeitssensorik
- Upstream/Downstream-Kommunikationsvermittlung

## Schuetze und Fuse

- je Pack eigene `HV+` und `HV-` Schuetze
- je Pack eigene HV Fuse
- Schuetzfreigabe wird ESS-seitig vorgegeben
- lokale Aktoransteuerung bleibt packseitig

## Feuchtigkeitssensorik

- `2` Sensoren gesamt im Batteriesystem
- offene Verteilung: `1+1` pro Pack oder `2+0`
- Reaktion: Logging + konfigurierbarer Fault ueber ESS

## Kommunikation

- Upstream: ESS Controller <-> Pack Controller
- Downstream: Pack Controller <-> lokale TLE9012 Chains (isoUART Ring)
- Enumeration: Default Bottom -> Top, Fallback Top -> Bottom
- Mapping-Verwaltung: ESS-seitig

## Kart-Packkonfiguration

- zwei mechanisch getrennte Batteriepakete: links/rechts
- Packs verbunden ueber verschraubtes HV-Kabel
- Hauptschuetze trennen links/rechts voneinander
- kein separates Trennen jedes Einzelpacks vom Inverter

## Referenzen

- `../../ARCHITECTURE.md`
- `../../ADR/ADR-001_System_Overview_ProtoA.md`
- `../../ADR/ADR-002_isoUART_Ring_Enumeration.md`
- `../../ADR/ADR-003_Control_Layers_Stack_Pack_ESS.md`
- `../SAFETY/Functional_Safety_ProtoA.md`

## Offene Punkte

1. Feuchtigkeitssensor-Verteilung `1+1` vs `2+0` finalisieren.
2. ESS<->Pack Protokoll finalisieren.
3. Fuse-Sense Bestueckvariante pro Pack festlegen.
