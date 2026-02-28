# ADR-002: Temp Sensing analog mit NTC + TMUX1309A + TLE9012

- Status: Accepted
- Datum: 2026-02-24
- Geltung: ProtoA

## Context

- Temp-Messung bleibt analog (keine digitalen Temp-ICs).
- Stack nutzt TLE9012 als einzige Mess-/Steuerinstanz (kein separater Stack-MCU).
- TLE9012 misst Widerstand ueber interne feste Messstromquellen mit Auto-Source-Selection.
- Temp-Diagnose muss auf Widerstandsebene funktionieren und bei Einzelfehler mindestens 50% erhalten.

## Decision

Pro TLE9012 wird die Temp-Messung wie folgt umgesetzt:

- `TMP0..TMP3` jeweils ueber 4:1-MUX
- MUX-Implementierung: `2x TMUX1309A` pro TLE9012
- Ein TMUX1309A bedient zwei TMP-Pfade, daher insgesamt 4 Pfade (`TMP0..TMP3`)
- `S0`, `S1` und optional `EN` werden fuer beide TMUX1309A gemeinsam geschaltet
- `S=00/01/10` fuer reale NTC-Messung
- `S=11` fuer Kanaldiagnose ueber separaten `RDIAG_x`
- Jeder TMP-Pfad hat eigenen `RDIAG_x` (keine Parallelschaltung von Diagnosewiderstaenden)

RDIAG-Festlegung (finale Defaultwerte, E96):

- `RDIAG_0 = 866 kOhm`
- `RDIAG_1 = 887 kOhm`
- `RDIAG_2 = 909 kOhm`
- `RDIAG_3 = 931 kOhm`
- optional `TMP4`: `RDIAG_REF = 953 kOhm`

## RDIAG-Ableitung (Annahme explizit)

Annahmen:

- `R25 = 10 kOhm`
- `B = 3950 K` (Arbeitsannahme bis Serien-NTC final fixiert)
- `T0 = 25 C = 298.15 K`
- Zielreferenz: `-50 C = 223.15 K`

Modell:

`R(T) = R25 * exp(B * (1/T - 1/T0))`

Rechnung:

- `1/T - 1/T0 = 1/223.15 - 1/298.15 = 0.001127 1/K`
- `B * (...) = 3950 * 0.001127 = 4.454`
- `exp(4.454) = 86.0`
- `R(-50 C) = 10 kOhm * 86.0 = 860 kOhm`

Abgeleitete diskrete Werte:

- nahe am Modellpunkt: `866 kOhm`
- mit Sicherheitsabstand oberhalb: `887 kOhm`, `909 kOhm`, `931 kOhm`

Empfehlung fuer alle RDIAG:

- Toleranz: `0.1%`
- Tempco: `<= 25 ppm/C`

## Rationale

- Diagnose bleibt kompatibel mit TLE9012-Widerstandsmessung (kein Spannungsteiler-Ansatz).
- `S=11` pro Kanal erlaubt robuste Erkennung von Stuck/Dead/Floating-Pfaden.
- Unterschiedliche RDIAG-Werte pro TMP-Kanal erhoehen Trennschaerfe bei Kreuzfehlern.
- RDIAG im Bereich um `-50 C` liegt klar ausserhalb normalem Betriebsbereich.
- Optionaler `TMP4`-RDIAG_Ref erlaubt Messpfadvergleich ohne MUX-Pfad.

## Consequences

- Zusaetzliche Praezisionswiderstaende pro TLE erforderlich.
- MUX- und Diagnose-Sequenz muss in Firmware strikt eingehalten werden.
- Lab-Validierung fuer Auto-Source-Selection und RDIAG-Fenster ist Pflicht.

## Lab-Validierung (verpflichtend)

1. RDIAG-Messung je TMP-Kanal bei `-20 C`, `25 C`, `85 C`.
2. RDIAG-Messung unter Drive/Recup-Lastwechseln.
3. Wiederholbarkeit je Select-State (`00/01/10/11`) ueber mindestens `1000` Zyklen.
4. Sicherstellen, dass Diagnosefenster keine Ueberlappung mit realem NTC-Bereich erzeugen.

## Related

- `ADR/ADR-003_Temp_FaultContainment_ProtoA.md`
- `ADR/ADR-004_Filtering_and_Sampling.md`
- `SCHEMATIC_NOTES/Temp_MUX_NTC_TLE9012.md`
