# Services

Hardware-independent service modules belong here. Increment 3 provides the DBC
adapter, per-bus Alive/drop/stale monitoring, authoritative MAIN/BACKUP source
selection and the minimal status, safety and service-response encoders. This
layer contains no STM32 HAL headers and performs no allocation.
