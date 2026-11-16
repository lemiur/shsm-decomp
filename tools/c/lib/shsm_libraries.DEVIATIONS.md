# shsm_libraries: accepted differences from tools/common/libraries.py

## D-shsm_libraries-1. The process cache is per thread
- Behaviour: `load_libraries` keeps one module-level cache; `shsm_load_libraries` keeps one per thread (the tables
  live in the thread's long-lived arena), so two threads each parse the file once.
- Reason: the C tools call it from the main thread; a per-thread cache needs no lock and gives the same tables.
