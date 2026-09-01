# Generated CAN codecs

`pack_controller.c` and `pack_controller.h` are generated from
`can/pack_controller.dbc` using the pinned `cantools` version. Regenerate them
with `python tools/generate_dbc.py`; verify drift with
`python tools/generate_dbc.py --check`. Do not edit the generated files.
