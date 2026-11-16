# shsm_dol: accepted differences from the Python

## D-shsm_dol-1. Short image in codecmp's reader
- Behaviour: `codecmp.Dol` on an image shorter than 224 bytes fails with Python's `struct.error`; every C entry
  point parses through `shsm_dol_parse`, which raises gatecheck's "DOL too short (N bytes)" below 0x100 bytes.
- Reason: codecmp only reads the retail DOL, which the byte gate pins; no short image reaches it.
