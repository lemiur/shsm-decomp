# shsm_re: accepted differences from Python's `re`

None of these is reached by a pattern of the project's Python tools: the golden corpus (585 patterns, every
pattern the tools use, run-time ones instantiated) compiles and gives CPython's results in every case.

## D-shsm_re-1. Offsets are bytes, not code points
- Behaviour: spans, `pos` and `endpos` are byte offsets into the UTF-8 subject; Python's are code-point indexes.
- Reason: the callers slice UTF-8 buffers. A port that prints an offset converts it (`avp_u8_len` of the prefix); the
  two agree on ASCII text, which is every subject the tools match (symbols, listings, sources, docs).

## D-shsm_re-2. Constructs not reproduced
- Behaviour: `\N{NAME}` is an error (no Unicode name database); the LOCALE flag `(?L)` is refused; `re.DEBUG` prints
  nothing; under IGNORECASE Python's dotless-i equivalences are missing (PCRE2's simple case folding is used); a
  negated shorthand inside a class of a pattern that mixes ASCII and Unicode scopes may differ; code points assigned
  after PCRE2 10.44's Unicode data (for example U+1C89) are not word characters or digits; a compiled pattern over
  64 KiB or with more than 31 capturing groups is refused.
- Reason: no tool pattern uses any of them (grep and the corpus); the subjects are ASCII.

## D-shsm_re-3. Lookbehind width
- Behaviour: PCRE2 accepts lookbehinds Python rejects (alternatives of different fixed lengths, `(?<=a|bc)`), so a
  pattern Python refuses with "look-behind requires fixed-width pattern" may compile here.
- Reason: only a pattern Python rejects behaves differently; the tools' patterns are all valid Python (the corpus
  compiles each one in CPython first). A run-time pattern given by a user (`--select`) could show it: the C tool
  then matches where the Python tool stopped with an error.

## D-shsm_re-4. Invalid UTF-8 subjects
- Behaviour: a str-pattern search over bytes that are not valid UTF-8 throws `AVP_ERR_FORMAT`.
- Reason: Python cannot hold such a subject as `str` (its tools decode with `errors="replace"` or fail earlier);
  a port decodes the same way before matching.
