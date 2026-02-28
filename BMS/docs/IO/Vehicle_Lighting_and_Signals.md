# Vehicle Lighting and Signals (ProtoA)

## Scope

Fahrzeugseitige Signalausgaenge und Statusanzeigen am ESS Controller.

## Signale

- Medical Light
- Red Light
- Green Light
- Rain Light
- Ready-to-move Light
- TSAL (Tractive System Active Light)
- Piezo/Buzzer

## Funktionszuordnung

| Signal | Quelle | Zweck | Kritikalitaet |
| --- | --- | --- | --- |
| Medical Light | ESS Controller | Service/Notfallanzeige | Komfort |
| Red Light | ESS Controller | Fahrzeugzustand FIA-konform | Safety |
| Green Light | ESS Controller | Fahrzeugzustand FIA-konform | Safety |
| Rain Light | ESS Controller | Sichtbarkeit/Fahrzustand | Komfort |
| Ready-to-move | ESS Controller | Fahrbereitschaft anzeigen | Safety |
| TSAL | ESS Controller | Tractive-System aktiv anzeigen | Safety |
| Buzzer | ESS Controller | Fault/Warn/Status Akustik | Safety |

## Signalpegel / Pinout Status

Aktueller Status:

- Exakte Pinouts: offen
- Exakte Pegel/Driverklassen: offen

Fixe Anforderungen:

1. Treiberstufen gegen Kurzschluss absichern.
2. TSAL und Ready-to-move nicht auf denselben Fehlerpfad legen.
3. Buzzer muss mindestens drei Muster unterstuetzen:
- Status
- Warnung
- Fault

## TODO (konkret)

1. Pinmapping in ESS-Controller-Schematic final eintragen.
2. Ausgangspegel je Signal finalisieren (z. B. 5V/12V Treiber).
3. Lastgrenzen je Ausgang dokumentieren.
4. Fault-Defaultzustand je Lichtsignal spezifizieren.
5. FIA-Review der finalen Signalzuordnung dokumentieren.
