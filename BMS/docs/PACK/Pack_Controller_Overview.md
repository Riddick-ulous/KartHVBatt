# Pack Controller Overview (ProtoA)

## Scope

Uebersicht der Pack-Controller-Funktionen, externen Schnittstellen und
HV-Infrastruktur auf Packebene.

## Kernfunktionen (fix)

- isoUART Masterbetrieb
- TLE9012 Enumeration und Mapping-Management
- Datensammlung aller Stackwerte
- Zeroing/Kalibrierung fuer Temperaturkanaele

## Precharge und Schuetze

- Precharge-Circuit vorhanden (Design aus bestehendem Projekt)
- Zentrale Schuetzsteuerung auf Packebene
- Schuetzsatz gemeinsam fuer linkes/rechtes Pack

## Akustische Signalisierung

- Piezo-Buzzer auf Pack-Controller
- Signalklassen:
- Fehler
- Warnung
- Status (z. B. Ready/Fault)

## Feuchtigkeitssensorik

- `2` Feuchtigkeitssensoren im Batteriegehaeuse
- Ziel: Kondensation und Leckage erkennen
- Reaktion: Logging + konfigurierbarer Fault

## Strom- und Spannungsmessung

Strom:

- Hall-Effekt-Stromsensoren (IC-basiert)
- separate Sensor-PCB
- Messpunkte: `HV+` und `HV-`
- Kalibrierung ueber extern schaltbare Hochlast-Widerstaende

Spannung:

- separate HV-Spannungsmessung
- Schaltung analog zum Precharge-Messpfad
- unabhaengige Packspannungsbeobachtung zusaetzlich zum Zellmonitoring

## Sicherheit und HV-Infrastruktur

- Externer Isowaechter (z. B. Bender Klasse) in Packintegration
- Crash-Sensor oeffnet Schuetze hardwareseitig, unabhaengig vom Controller
- Vollstaendiges HVIL, unterbricht Schuetzansteuerung bei Oeffnung

## Fahrzeug-/FIA-Interfaces

Externe Licht-/Signalanschluesse:

- Medical Light
- Red/Green Light (FIA Appendix J)
- Rain Light
- Ready-To-Move Light

Interne Anzeige:

- Tractive System Active Light nahe HV-Stecker

## Kuehlung und Laden

- Kuehlmittelpumpensteuerung integriert
- Temperatur- und optional Durchflusssensorik integriert
- CCS2 Ladeinterface vorgesehen, Details noch offen

## Packkonfiguration (Kart)

- Zwei mechanisch getrennte Batteriepakete: links/rechts
- Hauptschuetze trennen links/rechts voneinander
- Hauptschuetze trennen nicht jedes Einzelpack separat vom Inverter
- gemeinsamer Satz Schuetze fuer beide Seiten

## Offene Punkte (bewusst offen in ProtoA)

1. HV DC/DC Architektur fuer LV-Versorgung
2. CCS2 Detailimplementierung
3. Exakte Modellauswahl fuer Feuchtigkeitssensor, Crash-Sensor, Isowaechter

## Referenzen

- `../ARCHITECTURE.md`
- `../ADR/ADR-001_System_Overview_ProtoA.md`
- `../ADR/ADR-005_isoUART_Ring_Enumeration.md`
- `../CALIBRATION/Temp_Zeroing_PackController.md`
