# Moisture and Cooling (ProtoA)

## Scope

Feuchtigkeitssensorik im Gehaeuse und Wasserkuehlungssteuerung auf ESS-/Packebene.

## Feuchtigkeitssensorik (2 Sensoren gesamt)

Fix:

- Insgesamt `2` Sensoren im Batteriesystem.
- Reaktion bei Schwellwertverletzung: Logging + konfigurierbarer Fault.

Offene Verteilungsoptionen:

### Option M1: 1 Sensor pro Pack (`1+1`)

Vorteile:

- bessere Fehlerlokalisierung links/rechts
- bessere Servicezuordnung pro Pack

Nachteile:

- laengeres Harness pro Sensorpfad
- mehr Stecker-/Verdrahtungsaufwand

### Option M2: 2 Sensoren in einem Pack (`2+0`)

Vorteile:

- geringerer Verkabelungsaufwand
- einfachere Integration in einer Gehaeusehaelfte

Nachteile:

- schlechtere raeumliche Abdeckung
- schlechtere Lokalisierung im Fehlerfall

Entscheidungskriterien (fix):

1. Servicefaehigkeit/Fehlerlokalisierung
2. Kabel-/Steckeraufwand
3. thermische/feuchte Hotspot-Abdeckung

## Kuehlung

Fix:

- Kuehlmittelpumpe durch ESS Controller angesteuert
- Temperatursensorik eingebunden
- optionale Durchflusssensorik vorgesehen

Regelstrategie ProtoA:

1. Pumpe EIN bei Tractive-System aktiv.
2. Erhoehte Pumpenleistung bei Temp- oder Lastgrenznaehe.
3. Fault bei Sensorsignalverlust (konfigurierbar Warn/Fault).

## Schnittstellen

| Signal | Ebene | Domaene | Protokoll | Kritikalitaet |
| --- | --- | --- | --- | --- |
| Moisture Sensor 1 | Pack/ESS | LV | I2C/ADC (offen) | Monitoring |
| Moisture Sensor 2 | Pack/ESS | LV | I2C/ADC (offen) | Monitoring |
| Pumpensteuerung | ESS | LV | PWM/GPIO | Monitoring |
| Temperatursensor Kuehlkreis | ESS | LV | ADC/I2C (offen) | Monitoring |
| Durchflusssensor (optional) | ESS | LV | Digital/Analog (offen) | Monitoring |

## TODO (konkret)

1. Sensorverteilung `1+1` vs `2+0` bis Architekturfreeze entscheiden.
2. Endgueltigen Feuchtigkeitssensortyp festlegen.
3. Pumpentreiberstufe und Stromgrenzen finalisieren.
4. Schwellwerte fuer Feuchte/Kuehlfaults per Lab-Daten kalibrieren.
5. Diagnosebits fuer ESS-Statusframe definieren.
