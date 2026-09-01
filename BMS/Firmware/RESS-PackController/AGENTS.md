# PackController – Codex-Arbeitsregeln

## Scope

Arbeite ausschließlich unter `software/RESS-PackController`. Eine Ausnahme ist ein explizit beauftragter, auf diesen Pfad gefilterter CI-Workflow unter `.github/workflows`.

## Normative Quellen

1. `docs/software_architecture.md`: Verhalten, Zustände und Modulgrenzen
2. `docs/fault_matrix.md`: stabile Fault-IDs, Erkennung, Reaktion und Reset
3. `can/pack_controller.dbc`: CAN-IDs, Layout, Skalierung und Enums
4. `docs/ioc_resource_assignment.md`: Pins, Clock, GPIO, AF, Timer, ADC, DMA und NVIC
5. `PackController.ioc`: aus den Dokumenten abgeleitet, nie stillschweigend führend

Bei einem Widerspruch nicht raten: sicheren Zustand beibehalten, Widerspruch dokumentieren und die normative Quelle korrigieren.

## Umsetzung

- GNU Arm, C11, C++17, CMake/Ninja; kein IAR, kein RTOS, kein Heap nach Startup, keine Exceptions und kein RTTI.
- STM32Cube-Code bleibt in `generated/stm32cube`; Fachlogik enthält keine STM32-Header.
- DBC-Code wird generiert und nicht von Hand parallel gepflegt.
- ISR setzen Flags oder bewegen Daten in feste Buffer; keine Fachlogik und keine blockierenden Delays.
- Jede Änderung bildet ein Inkrement aus Kapitel 18 ab und enthält die zugehörigen Host-Tests.
- `BOARD_BRINGUP`: `kTleSlaveCount = 1`; `DEV_OUTPUT_TEST` und `DEV_COMMISSIONING` exakt nach Architektur verfügbar.
- `PACK_162S2P`: `kTleSlaveCount = 18`; Ausgänge ausschließlich über Stateflow und Output Arbiter.
- Developer-Ausgänge sind nach Reset aus, nicht persistent, CAN-Keepalive-gebunden und werden weiterhin ausschließlich durch den Output Arbiter geschrieben.
- Außer den dokumentierten Developer-Ausnahmen darf keine Testoption Fault Manager, `HVReady`, SAFE oder Watchdog-Health-Gate umgehen.

## Vor jedem Commit

```bash
python3 tools/validate_contract.py \
  --dbc can/pack_controller.dbc \
  --fault-matrix docs/fault_matrix.md \
  --require-cantools
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```

Die CMake-/CTest-Schritte gelten ab Abschluss von Inkrement 0; der Contract-Check gilt sofort.
Die CI installiert die gepinnte `cantools`-Version und ruft den Contract-Check zusätzlich mit `--require-cantools` auf.

CubeMX-Regeneration und fachliche Änderungen nicht im selben Commit mischen. Generated-Code-Diffs auf Clock, GPIO-Bootwerte, Alternate Functions, DMA und IRQs begrenzen und vollständig reviewen.
