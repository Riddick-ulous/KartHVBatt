# PackController CAN contract

`pack_controller.dbc` is the sole manually maintained signal definition. The C
codec under `generated/can` is generated with `cantools==41.3.0`:

```bash
python tools/generate_dbc.py
python tools/generate_dbc.py --check
```

The contract validator additionally performs strict DBC parsing and confirms
that all nine frames are recognized as CAN FD with bit-rate switching.
