# Schematic Notes: Temp MUX + NTC + TLE9012

## Scope

Pin-nahe Verdrahtungsbeschreibung pro TLE9012 fuer analoges Temp-Subsystem mit
`2x TMUX1309A`, `TMP0..TMP3`, Diagnose ueber `RDIAG_x` und optionalem `TMP4_REF`.

## 1) Pin-level Zuordnung pro TLE9012

Bausteine pro TLE-Domain:

- `U_MUX_A`: TMUX1309A
- `U_MUX_B`: TMUX1309A

Gemeinsame Steuerpins:

- `S0_Tx` -> `U_MUX_A.S0`, `U_MUX_B.S0`
- `S1_Tx` -> `U_MUX_A.S1`, `U_MUX_B.S1`
- `EN_Tx` (optional) -> `U_MUX_A.EN`, `U_MUX_B.EN`

Versorgung:

- `U_MUX_A.VDD`, `U_MUX_B.VDD` -> `VDD_TEMP_STACK`
- `U_MUX_A.GND`, `U_MUX_B.GND` -> `AGND_TEMP`

Kanalzuordnung:

- `U_MUX_A.CH_A(COM)` -> `TMP0_PATH_IN`
- `U_MUX_A.CH_B(COM)` -> `TMP1_PATH_IN`
- `U_MUX_B.CH_A(COM)` -> `TMP2_PATH_IN`
- `U_MUX_B.CH_B(COM)` -> `TMP3_PATH_IN`

Danach je Pfad identisch:

- `TMPz_PATH_IN -> R_TMPz -> TMPz_NODE`
- `TMPz_NODE -> C_TMPz -> AGND_TEMP`
- `TMPz_NODE -> TLE9012.TMPz`

## 2) NTC-Anschluss Topologie

- Jeder NTC ist als `10k@25C` gegen `AGND_TEMP` geschaltet.
- NTC-Leitung vom Flex/Breakout geht ueber Connector zum zugeordneten MUX-Input.
- Empfohlener optionaler Leitungsschutz pro NTC-Leitung:
- Serienwiderstand `R_SER_NTC = 47..100 Ohm` nahe Connector
- ESD-Klemme gegen `AGND_TEMP` (low leakage)

## 3) MUX-Eingangsbelegung pro TMP-Kanal

Pro TMP-Pfad (`TMP0..TMP3`) gilt:

- Input `I0` bei `S1:S0=00` -> NTC_a
- Input `I1` bei `S1:S0=01` -> NTC_b
- Input `I2` bei `S1:S0=10` -> NTC_c
- Input `I3` bei `S1:S0=11` -> `RDIAG_x` nach `AGND_TEMP`

Vollbelegung (pro TLE, 12-NTC-Template):

- `TMP0`: `NTC1`, `NTC2`, `NTC3`, `RDIAG_0`
- `TMP1`: `NTC4`, `NTC5`, `NTC6`, `RDIAG_1`
- `TMP2`: `NTC7`, `NTC8`, `NTC9`, `RDIAG_2`
- `TMP3`: `NTC10`, `NTC11`, `NTC12`, `RDIAG_3`

## 4) Truth Table (Select)

| S1 | S0 | TMP0 | TMP1 | TMP2 | TMP3 |
| --- | --- | --- | --- | --- | --- |
| 0 | 0 | NTC1 | NTC4 | NTC7 | NTC10 |
| 0 | 1 | NTC2 | NTC5 | NTC8 | NTC11 |
| 1 | 0 | NTC3 | NTC6 | NTC9 | NTC12 |
| 1 | 1 | RDIAG_0 | RDIAG_1 | RDIAG_2 | RDIAG_3 |

## 5) RDIAG pro TMP-Kanal (fix)

Berechnungsbasis:

- NTC-Modell mit `B=3950 K`: `R(-50 C) ~ 860 kOhm`

Festwerte:

- `RDIAG_0 = 866 kOhm`
- `RDIAG_1 = 887 kOhm`
- `RDIAG_2 = 909 kOhm`
- `RDIAG_3 = 931 kOhm`

Bauteilanforderung:

- Toleranz `0.1%`
- Tempco `<= 25 ppm/C`
- Platzierung nahe MUX/TLE-Pfad (nicht auf Flex)

Wichtig:

- Jeder TMP-Kanal hat eigenen RDIAG.
- Keine gemeinsame RDIAG-Schiene fuer mehrere TMP-Kanaele.

## 6) TMP4 Nutzung (Entscheidungsvorschlag)

Vorschlag: `TMP4` mit separatem `RDIAG_REF = 953 kOhm` bestuecken.

Begruendung:

- Referenzmessung ohne MUX-Umschaltung
- bessere Trennung zwischen globalem TLE-Pfadfehler und MUX-/Select-Fehler
- geringe Zusatzkosten (ein Praezisionswiderstand + Filterpfad)

## 7) Diagnosefenster (Ohm)

Pro Kanal `RDIAG_nom`:

- Pass: `0.95..1.05 * RDIAG_nom`
- Warnung: `0.90..0.95` oder `1.05..1.10 * RDIAG_nom`
- Fehler: ausserhalb `0.90..1.10 * RDIAG_nom`

Extremschwellen:

- `R < 1 kOhm` -> Short
- `R > 1.5 MOhm` -> Open/Floating

## 8) Diagnoselogik (Kurz)

- Stuck select:
- Bei `S=11` muessen alle TMP-Pfade den zugeordneten RDIAG treffen.
- Abweichung ueber 2 Diagnosezyklen => Fault.

- MUX dead:
- TMP-Pfadwert bleibt ueber alle States nahezu konstant und unplausibel.

- Floating/Open:
- sehr hoher Widerstand oder hohe Varianz im Kanal.

- Line short:
- sehr niedriger Widerstand und geringe Varianz.

## 9) Offene Parameter / TODO

1. `R_TMPz` / `C_TMPz` final je TMP-Pfad.
2. `R_SER_NTC` final festlegen (`47` vs `100 Ohm`).
3. ESD-Typ final (Leakage vs Robustheit).
4. AGND-Quiet-Return-Fuehrung im PCB-Layout finalisieren.
5. TLE-Messzeit `t_tmp` im Labor gegen Registerkonfiguration absichern.
