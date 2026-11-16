# shsm_pe: accepted differences from tools/mwcc_trace/symbols.py

No entries. The reader makes the Python's reads in the Python's order with the same bounds (one check per
`struct.unpack_from` format), so a damaged file fails at the same read: `SHSM_PE_EXC_STRUCT` carries CPython's
`unpack_from requires a buffer of at least ...` text, `SHSM_PE_EXC_INDEX` its `index out of range` (bytes) or
`list index out of range` (the section list), `SHSM_PE_EXC_NO_DEBUG_INFO` the NoDebugInfo texts and `AVP_ERR_IO`
`str(OSError)`. How a caller prints them (a one-line traceback) is that tool's deviation.
