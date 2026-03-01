# ADR-004: Filtering, Sampling und Sequencer fuer Temp-Messung

- Status: Accepted
- Datum: 2026-02-24
- Geltung: ProtoA

## Context

- Temp-Pfade nutzen MUX vor TLE9012-TMP-Eingaengen.
- Zur EMV-Robustheit wird ein RC hinter dem MUX eingesetzt.
- Gueltige Messung erst nach Einschwingen (`>= 3tau`).
- Sequencer muss Messzustand und Diagnosezustand bedienen.

## Decision

- Pro TMP-Pfad: `MUX -> R_TMPz -> (Knoten) -> C_TMPz nach AGND -> TMPz`.
- Select-States:
- `00`, `01`, `10` = NTC-Messung
- `11` = Diagnose (`RDIAG_x`)
- Diagnose wird standardmaessig alle `10` Messzyklen eingefuegt (ca. `1 Hz` bei `10 Hz` Sensorscan).
- Diagnosebewertung erfolgt primaer in `Ohm`, nicht in `C`.

RDIAG-Festwerte (kanalspezifisch):

- `TMP0: 866 kOhm`
- `TMP1: 887 kOhm`
- `TMP2: 909 kOhm`
- `TMP3: 931 kOhm`
- optional `TMP4_REF: 953 kOhm`

## Timingmodell

Definitionen:

- `tau = R_TMPz * C_TMPz`
- `t_settle = 3tau`
- `t_tmp = TLE-Messzeit pro TMP-Kanal` (Annahme initial: `1.0 ms`)
- `t_meas_state = 4 * t_tmp`
- `t_ovh = Sequencer-Overhead pro State` (Annahme initial: `0.2 ms`)
- `t_state = t_settle + t_meas_state + t_ovh`
- `Ndiag = Diagnoseintervall in Messzyklen` (Default: `10`)

Dann:

- `T_scan = (3 + 1/Ndiag) * t_state`
- `f_sensor = 1 / T_scan`

Explizit:

- `f_sensor = 1 / ((3 + 1/Ndiag) * (3tau + 4*t_tmp + t_ovh))`

## Tau-Constraints fuer Zielraten

Umgestellt nach `tau_max`:

`tau_max = ((1/f_target)/(3 + 1/Ndiag) - (4*t_tmp + t_ovh)) / 3`

Beispiel mit `t_tmp=1.0 ms`, `t_ovh=0.2 ms`, `Ndiag=10`:

- fuer `f_target=2 Hz`: `tau_max = 52.4 ms`
- fuer `f_target=10 Hz`: `tau_max = 9.4 ms`

Konservativer Fall `Ndiag=1` (Diagnose in jedem Scan):

- `2 Hz`: `tau_max = 40.3 ms`
- `10 Hz`: `tau_max = 6.9 ms`

Designvorgabe ProtoA:

- Zielbereich `tau = 2..6 ms`
- Harte Obergrenze fuer 10-Hz-Betrieb: `tau <= 6.9 ms` (konservativ)

## Diagnosefenster und Schwellen (Ohm)

Pro Kanal mit `RDIAG_nom`:

- Pass: `0.95 * RDIAG_nom .. 1.05 * RDIAG_nom`
- Warnung: `0.90..0.95` oder `1.05..1.10` relativ zu `RDIAG_nom`
- Fehler: `<0.90` oder `>1.10` relativ zu `RDIAG_nom`

Globale Extremschwellen:

- `R_short_th = 1 kOhm` (harte Kurzschlussindikation)
- `R_open_th = 1.5 MOhm` (harte Open/Floating-Indikation)

## Diagnose-Logik

- Stuck-Select:
- Bei `S=11` muessen alle `TMP0..TMP3` im jeweiligen RDIAG-Passfenster liegen.
- Wenn ein Pfad in `2` aufeinanderfolgenden Diagnose-States nicht im Fenster liegt: Fault.

- MUX dead:
- Ein TMP-Pfad liefert ueber alle Select-States nahezu konstante Werte (`DeltaR/R < 1%`) ausserhalb erwarteter Tabelle.

- Floating/Open:
- `R > R_open_th` oder hohe Streuung (`sigma_R / R > 5%`) in kurzer Folge.

- Short:
- `R < R_short_th` stabil ueber mindestens `2` Messungen.

- TMP4_REF Nutzung:
- Bei vorhandenem `RDIAG_REF` dient TMP4 als Vergleichspfad fuer TLE-Messkette ohne MUX-Umschaltung.

## Auto-Source-Selection: Risiken und Absicherung

Nutzen:

- RDIAG im hohen Widerstandsbereich ist direkt kompatibel mit TLE9012-Auto-Source-Selection.

Risiken:

- Messspruenge an Stromstufen-Grenzen.
- Erhoehte Jittermoeglichkeit bei Widerstaenden nahe Umschaltgrenzen.

Absicherung:

1. RDIAG-Werte nicht dicht beieinander, sondern gestaffelt (`866/887/909/931 kOhm`).
2. Diagnoseauswertung in Ohm mit Fenster statt nur Temperaturkonversion.
3. Lab-Test: Source-Stufen-Tracking je Kanal ueber Temperatur und Lastwechsel.

## Offene Parameter / TODO

1. Finale `R_TMPz` / `C_TMPz` je TMP-Pfad.
2. Finale TLE-Messzeit `t_tmp` aus Registerkonfiguration/Lab.
3. Finale Diagnosefenster nach EVT-Messstatistik.
4. Finaler Einsatz von `TMP4_REF` (bestueckt ja/nein pro Layoutvariante).
