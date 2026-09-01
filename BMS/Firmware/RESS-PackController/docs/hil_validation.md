# PackController HIL-/Aufbauvalidierung — Codex-Übergabestand

## Fan-PWM

Firmwarekennlinie:

```text
pulse_us = 1000 + 10 × clamp(FanSpeedRequest_pct, 0, 100)
frequency = 50 Hz
default / stale CAN = 100 % = 2000 µs
```

| Prüfschritt | Soll / Akzeptanz |
|---|---|
| 0/25/50/75/100-%-Request | 1000/1250/1500/1750/2000 µs |
| Frequenz | 50 Hz ±1 % |
| Pulsbreite | Sollwert ±10 µs, kein Glitch beim Update |
| Power-up | spätestens nach Timer-Initialisierung 2000-µs-Failsafe |
| ungültiger/staler Request | innerhalb 100 ms auf 2000 µs |
| reale Fan-/Reglerreaktion | monotone Drehzahl/Luftleistung; 2000 µs entspricht Maximum |

TIM3 darf beim JTAG-Halt nicht eingefroren werden, damit der letzte – standardmäßig maximale – Fanwert bestehen bleibt. Ist die reale Kennlinie invertiert oder besitzt der Regler andere Endpunkte, werden `pulse_min_us`, `pulse_max_us` und `invert` im Boardprofil geändert; die PWM-Grundfrequenz bleibt 50 Hz.

## Balancing-Thermik

Bei 4,25 V gilt je aktivem Kanal:

| Größe | Wert |
|---|---:|
| Strom | `4,25 V / (39 Ω + 1,6 Ω + 10 Ω) = 84,0 mA` |
| 39-Ω-Verlust | 0,275 W |
| 10-Ω-Verlust | 0,071 W |
| TLE-Pfad 1,6 Ω | 0,011 W |
| externe Verlustleistung | 0,346 W je Kanal |

Startkonfiguration ist TLE-PWM 100 % mit maximal drei gleichzeitig aktiven Kanälen je Slave. Die TLE-interne Off-Time wird am Stromverlauf vermessen; bei 76,6 % effektivem Duty ergeben sich etwa 0,265 W je Kanal beziehungsweise 0,795 W externe mittlere Verlustleistung je Slave.

### Aufbauversuch

| Randbedingung | Durchführung |
|---|---|
| Zellspannung | geregelte 4,25 V an den drei geprüften Kanälen |
| Muster A | drei thermisch benachbarte Balancing-Kanäle |
| Muster B | drei räumlich verteilte Kanäle |
| Umgebung | höchste für Balancing freigegebene Aufbau-/Gehäusetemperatur |
| Laufzeit | mindestens 60 min und bis Temperaturänderung <1 K in 5 min |
| Messstellen | 39 Ω, 10 Ω, TLE9012, PCB-Hotspot, Umgebung, Kanalstrom |
| Parallelbetrieb | Kommunikation, Zellmessung und TMP-Referenzprüfung laufen weiter |

Akzeptanz:

- `I_balance = 84 mA ±10 %`; effektiver Duty entspricht der TLE-Konfiguration.
- Für jedes Bauteil gilt `T_measured + 20 K` unter dessen zulässiger, leistungsderateter Grenztemperatur.
- Keine Verfärbung, Lötstellen-/PCB-Schädigung, TLE-Diagnose oder Kommunikationsstörung.
- Zellspannungsfehler bleibt innerhalb der TLE-Spezifikation; TMP-Referenzabweichung bleibt unter ±2,5 %.
- Abschalten von `BalanceEnable`, CAN-Stale, OV/UV, Temperatur- oder TLE-Fehler löscht alle Balancing-Masken im nächsten 20-ms-Zyklus.

Besteht der Versuch nicht, wird zuerst `max_active_per_slave` reduziert. Erst danach wird ein kleinerer PWM-Duty verwendet; Rotation bleibt bei mehr als drei Kandidaten aktiv. Freigegebene Startwerte: Balancing nur im Ringbetrieb, `4,000 V` Mindestspannung, 15-mV-Start-/5-mV-Stop-Hysterese, 0…55 °C und 10-s-Rotation.
