# Temp Zeroing im Pack-Controller

## Zweck

Kanal-Offsetkorrektur fuer NTC/MUX/TLE-Streuung zur Verbesserung der relativen
Kanalgleichheit im Stack.

## Preconditions

- homogener thermischer Zustand im Pack
- alle Tempkanaele diagnostisch gueltig
- kein aktives Balancing waehrend Zeroing
- keine aktiven Temp-Faults im aktuellen Zyklus

## Datensatzformat (NVM)

```text
struct TempZeroingRecordV1 {
  uint16 version;                 // = 1
  uint16 stack_id;
  uint32 timestamp_unix_s;
  uint16 channel_count;           // ProtoA: 18
  float  offset_c[channel_count]; // T_corr = T_meas + offset_c[i]
  float  ref_temp_c;
  uint32 crc32;
}
```

## Zeroing Ablauf

1. Messfenster starten und pro Kanal `N=32` Samples erfassen.
2. Kanalmedian in Ohm bilden, danach in `C` umrechnen.
3. Referenztemperatur je Stack bestimmen:
- Default: Median aller validen Kanaele.
4. Offset je Kanal berechnen:
- `offset_c[i] = T_ref_c - T_meas_c[i]`
5. Plausibilitaet pruefen:
- Kanal verwerfen, wenn Rohdiagnose ungueltig.
6. Datensatz als `TempZeroingRecordV1` in NVM schreiben.
7. Laufzeitkorrektur aktivieren:
- `T_corr_c[i] = T_meas_c[i] + offset_c[i]`

## Limits

- `abs(offset_c[i]) <= 3.0 C` akzeptiert
- `3.0 C < abs(offset_c[i]) <= 5.0 C` Warnung, Kanal bleibt aktiv
- `abs(offset_c[i]) > 5.0 C` Fehler, Kanal auf suspect setzen
- Standardabweichung im Zeroingfenster:
- `sigma_c[i] <= 0.2 C` erforderlich, sonst Wiederholung

## Diagnosekopplung

- Zeroing wird nur in Mess-States (`00/01/10`) berechnet.
- Diagnose-State (`11` mit RDIAG) muss parallel innerhalb Fenstergrenzen liegen.
- Bei RDIAG-Fault wird Zeroing-Lauf abgebrochen.

## Verifikation

1. Direkt nach Zeroing erneut `N=32` Samples aufnehmen.
2. Restabweichung gegen Referenz pruefen:
- Ziel: `<= 0.5 C` typisch, `<= 1.0 C` worst case.
3. Ergebnis und CRC in Log speichern.

## Re-Trigger Bedingungen

- Tausch von NTC/Flex/MUX/TLE-Board
- Connector-Rework im Temp-Pfad
- signifikante Drift (`>1.0 C`) ueber Langzeittrend
