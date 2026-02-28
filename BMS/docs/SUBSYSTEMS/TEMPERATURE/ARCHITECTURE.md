# Subsystem Architektur: Temperaturmessung

## Zweck

Dieses Dokument ist die alleinige technische Detailquelle fuer das Temperatur-Subsystem in ProtoA.
Es enthaelt Messkette, Timingregeln, Ratenbudget, Fault-Containment und offene Engineering-TODOs.

## Anforderungen (ProtoA, fix)

- `18` Temperaturkanaele pro Stack (`1` je `2P`-Gruppe)
- Messrate pro Stack: mindestens `2 Hz`, Ziel `10 Hz`
- Genauigkeit: `0.5 C` typisch / `1.0 C` worst case
- Einzelfehler darf nicht alle Kanaele ausfallen lassen
- Mindestverfuegbarkeit: `>= 9/18` (`>= 50%`)

## Messkette und Platzierung

```text
NTC (0603 auf Flex/Breakout)
  -> Connector
    -> MUX (Stack-PCB)
      -> RC-Filter (hinter MUX)
        -> TLE9012 ADC
```

Platzierungsregeln:

- MUX auf Stack-PCB (nicht auf Flex)
- ein analoges RC-Filter hinter dem MUX
- Flex bleibt passiv (nur NTCs)

## Steuerung und Sequenz

Der TLE9012 uebernimmt MUX-Umschaltung und ADC-Akquisition.
Pro Kanal gilt die Sequenz:

1. MUX-Kanal waehlen
2. `>= 3tau` warten (Einschwingen)
3. ADC-Sample aufnehmen

## Timing- und Ratenbudget

Sequentielles Modell pro Stack:

- `N = Anzahl Kanaele` (nominal `18`)
- `t_ch = t_switch + 3tau + t_adc`
- `t_scan = N * t_ch`
- `f_scan = 1 / t_scan`

Ratenziel umgestellt in Kanalzeitbudget (`N=18`):

- Fuer `2 Hz`: `t_ch <= 1 / (18 * 2) = 27.78 ms`
- Fuer `10 Hz`: `t_ch <= 1 / (18 * 10) = 5.56 ms`

Designimplikation:

- RC-Auslegung (`tau`) und Scheduler koennen nur gemeinsam optimiert werden
- Segmentierung, MUX-Anzahl und ADC-Zeit beeinflussen das erreichbare `f_scan` direkt

## Robustheit und Filteraufteilung

Hardware:

- RC-Filter fuer analoge Daempfung/EMV-Stabilitaet

Firmware (nur ergaenzend):

- Outlier-Reject
- Moving Average
- Rate Limiting

Regel:

- Softwarefilter duerfen reale, persistente Sensorfehler nicht maskieren

## Fault-Containment

Harte Vorgabe:

- Ein Einzelfehler darf nicht alle 18 Kanaele lahmlegen
- mindestens `9` Kanaele pro Stack verfuegbar

Architekturableitung:

- Segmentierung mindestens `2 x 9` oder aequivalent fault-containment-faehig
- Trennung ueber MUX-Pfade, Connector-Pins und/oder Flex-Segmente zulaessig

## Kalibrierung und Inbetriebnahme

Kanaloffset-Nullung erfolgt im Pack-Controller.
Verfahren siehe:

- `../../CALIBRATION/Temp_Zeroing_PackController.md`

## Offene Engineering-TODOs

1. finale MUX-Topologie (`1` vs `2` vs `4`)
2. RC-Werte und `tau` final auslegen
3. Scheduler-Timingbudget gegen `2 Hz`/`10 Hz` nachweisen
4. finale Flex-Segmentierung (`2x9` physisch vs `1x18` segmentiert)

## Zugehoerige ADRs

- `ADR/ADR-002_TemperatureSensing_Analog_NTC_MUX.md`
- `ADR/ADR-003_Temp_FaultContainment_ProtoA.md`
- `ADR/ADR-004_Filtering_and_Sampling.md`
