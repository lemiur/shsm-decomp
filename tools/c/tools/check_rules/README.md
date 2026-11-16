# Rule checker

`build/tools/check_rules.exe` (C port of the retired `tools/rules/check_rules.py`) mechanically checks the rules of the `decomp-code-rules` skill that can be detected statically
in C/C++ source. No dependency on other project tools.

```
build/tools/check_rules.exe                      # src/, plus every header it includes
build/tools/check_rules.exe src/revolution include/revolution/os.h
build/tools/check_rules.exe --units main/Runtime.PPCEABI.H/__init_cpp_exceptions
build/tools/check_rules.exe --changed origin/main # source/header files changed vs. a git revision (+ untracked)
build/tools/check_rules.exe --json src            # machine-readable
build/tools/tests/test_check_rules_selftest_test.exe   # self-test (all check_rules tests: ctest --preset tools -R check_rules)
```

Options: `--tier match,checked,...` (filter what is printed; the exit code ignores it), `--no-summary`,
`--functions` (list clean functions in the summary too), `--allow FILE` (exceptions record, default
`docs/exceptions.md`), `--configure FILE` (default `configure.py`), `--classes FILE` (the retail class
model for the R13 slot check, default `build/R5WEA4/classes.json`, written by `build/tools/classes.exe scan`; when
it is missing, unreadable or stale the summary says the slot check was skipped and why, and nothing fails), `--root DIR`.
An explicit `--allow` or `--configure` file that does not exist is a usage error.

classes.json is **stale** when the input stamp `classes.exe scan` records in it (`inputs`: the asm tree's `.s` count,
total size and newest mtime, the symbols file's size and mtime) no longer matches those files, or when it has no
stamp (written by an older `classes.py`): the R13 slot checks are then skipped with a note saying to run
`build/tools/classes.exe scan`. The checker recomputes the stamp itself (same computation; it imports no project
code).

**Per-file cache.** A run reuses, per file, what an earlier run computed from identical inputs, from the
machine-wide content-addressed cache (`$SHSM_CACHE_DIR`, else `%LOCALAPPDATA%\shsm-cache`; the entry format of
`tools/c/lib` cache, so its `prune()` manages them): the file's parse results without its tokens ("facts":
keyed on the checker's own source, the file's path and bytes and the project's storage-class macros) and the
findings of the per-file checks ("findings": keyed on the facts key plus the file's language, its unit status, the
allow list's asm sets, the project-wide offset macros and noted volatile names, and the path and bytes of every file
in its include closure). Cross-file rules (R1, R2), the R13 slot check, RULE-EXCEPTION comments, the allow list's
rulings, unit status and the summaries are recomputed every run. Keys are content, not paths or mtimes, so the
gate's fresh tree copy hits too. The output (text, `--json`, exit code) is identical with and without the cache
(`PerFileCache` in the self-test proves it per kind of edit); a damaged entry is a miss. `--no-cache` or
`SHSM_RULES_CACHE=0` bypasses it. Any edit to the checker invalidates every entry.

`#if 0` arms are compiled out: their code, comments and nested directives are not checked (the `#if 0` itself is
still an R37 finding, and the `#else` arm of `#if 0 ... #else` is live). Other conditionals are read as written.

## Output

One line per finding: `path:line: RULE-ID [tier] message (in function)`.

- `tier` is the rule's tier from the skill: `match`, `checked`, `free`, `evidence`; or `advisory` for a finding
  of a [match] rule that is outside the rule's narrow checker row (never blocks).
- Findings covered by a valid `RULE-EXCEPTION`, and findings on the allow list, are not violations. They print
  apart under `# exceptions and listed originals`, with the reason appended.
- `# summary by file` and `# summary by function` follow. A function with a [match] violation is
  `MATCH-PENDING`; a listed assembly function is `ORIGINAL-ASM`.
- `--json` prints one object: `findings` (violations), `exceptions`, `allowed`, `exception_comments`,
  `functions` (every function with its status and counts), `blocking`, `hand_reviewed` (the rules no check emits), `r13` (the slot check: classes checked,
  `matched` with the retail vtable and how it was found, `unmatched`, `skipped` (not modelled), counts per kind
  `added`/`missing`/`order`/`override`/`advisory`, and `note` when the check was skipped), `exit`.
- The summary ends with one `# R13 slots: ...` line with the same counts, or the skip note.

Exit status: `1` when a [match]-tier violation lies in a `Matching` unit, `2` on a usage or environment error (a
path or unit that does not exist, a failing `git` for `--changed`, a missing `--allow`/`--configure` file; message
on stderr), else `0`. The text output lists each `MATCH-PENDING` function of a Matching unit before the result line.
"In a Matching unit" means: the finding's file is a `Matching` source file from `configure.py`, or a header that a
`Matching` source includes (transitively). File-scope findings (a global, a type) count, not only findings inside
a function. Findings in `NonMatching`, `Equivalent` and unlisted units are reported but never fail the run.
`configure.py` is parsed for its `Object(Matching|NonMatching|Equivalent, "path")` entries and never imported.

Headers that the scanned sources include are checked too (the rules about globals and types live there). All of
`src/` and `include/` is indexed for the cross-file checks (R1, R2) even when only some files are scanned.

Includes are resolved per translation unit, in the order MWCC searches: the including file's directory, then the
`-i` directories of the unit being compiled (in flag order), then the default `include/`, then `src/`. The `-i`
directories come from `configure.py` (parsed, never imported; the generated `build.ninja` is gitignored and may be
absent or stale): each `Object(...)` takes the `-i <dir>` flags of its library's `cflags` (the `def` helper such as
`ZlibLib`, or the dict written out in `config.libs`, through list variables and `*spread`s) plus its own
`extra_cflags`, so a header reached only through `-i include/zlib`, `-i include/libjpeg` or
`-i include/revolution/hbm` is checked, and its includers are the units it affects. Generated directories
(`build/...`) are ignored. A header scanned on its own, with no unit, is resolved through every library's `-i`
directory as a last resort. Without `configure.py` (or for a unit it does not list), `include/<library>/` named
like the source directory (`src/zlib/` to `include/zlib/`) is tried.

IJG's declaration macros (`GLOBAL(type)`, `LOCAL(type)`, `METHODDEF(type)`, `EXTERN(type)`) are read as the return
type: the function is named by the identifier after them. Macro bodies are never parsed as code or declarations.

## Allow list and exceptions

- **`docs/exceptions.md`** is read for: the `ORIGINAL-ASM` table (unit, function), the `Listed inline-asm
  helpers` table (unit, function, helper), and the `Review rulings` table (function, rule). A finding of rule R in a
  function with a ruling for R is reported as allowed. The unit column is the source path under `src/` without
  extension (`revolution/os/OSTime`).
- **`// RULE-EXCEPTION: <rule>, <reason>`** (also `/* */`; `R14/R30` for several rules). The reason must say why
  the bytes need it (at least 12 characters, 3 words). It covers its own line (trailing form), or the next
  statement. A comment in the banner above a function covers the whole function. It never waives a [match]
  finding: that stays a violation and the checker adds an `R0` finding. A malformed exception (no colon, unknown
  rule, missing or too short reason) is an `R0 [free]` finding.
- **Evidence-note conventions** (same line, or a whole-line comment directly above):
  `// volatile: <evidence>` (R17; `// volatile: delay loop, <reference>` for the delay-loop counter, `// volatile: read by a release-disabled assertion, <reference>` for the assert-only local), `// byteptr: <allocator header | file format | offsetof table>, <evidence>`
  (R4), `// pad: <SDK or file-format name>` (R17 padding member). Source-side listing of assembly:
  `// ORIGINAL-ASM: <evidence>` or `// ASM-HELPER: <evidence>` in the six lines above the function.
  Prefer listing in `docs/exceptions.md`, which is the reviewer's record.

## Checks

IDs and tiers are the skill's. "FP" lists known false positives and the remedy.

### [match] rules (a hit blocks a Matching unit)

| ID | detection | known false positives |
|---|---|---|
| R1 | A type name (`struct`/`class`/`union`/`enum`, qualified by enclosing class/namespace) defined in two files, with different or identical bodies. A `struct B` whose first 2+ member declarations copy the leading members of a larger `struct A` (a struct with function-pointer or macro-declared members, such as IJG's `JMETHOD(...)`, has data the member list cannot show and is never a prefix copy), when a cast to `A*`/`B*` exists (layout view; only a real cast expression counts, not a parameter list or declaration such as `int g(A*);`). In C++ builds, a `class`/`struct` keyword that differs between a definition and another declaration of the name. C's first-member inheritance is not a copy and does not fire. Separate private types that share a name do not fire: every definition in a C source file (none in a header, none in C++), no defining file including another, all bodies different and no member list a prefix of another (IJG's `my_upsampler` in jdsample.c and jdmerge.c). A definition inside `#ifndef G` / `#if !defined(G) && ...` (no `||`, no `#else`) is not a second definition when, in every translation unit that sees both copies, `G` is already `#define`d where that unit first includes the file (zlib.h's `struct internal_state` under `!defined(ZUTIL_H)` against zutil.c's own; through the unit's `-i` directories). | |
| R2 | A file-scope `extern` declaration without initialiser in a source file; a local `extern` variable declaration in a function; a non-`extern`, non-`static`, non-`const` variable defined in a header (an address binding `T x[] : 0xADDR;` is a declaration); `struct T {...} name;` in a header; one non-static global declared with different type text in different files (`u32` vs `unsigned long` counts as different). `extern T x = init;` is a definition and passes. K&R parameter declarations (`f(a, b) int a; char *b; { ... }`) are the function's parameters, not globals; an object-like macro whose whole body is `static` or `extern` (`#define local static`, collected from every indexed file) is that storage class. | A linker symbol that has no header yet fires; declare it in a header. |
| R4 | `*(T*)((char*)p + K)`, `((T*)((u8*)p + K))->m` or `[i]`, also with an integer-address cast (`(u32)p + K`), where K is a numeric literal or an all-caps constant and a pointer cast with a dereference wraps the sum. Same with `this` as operand. `*(scalar*)this`. A numeric `#define *_OFFSET`/`*_OFS` macro (literal body, also parenthesised) is reported at its use, and only where it offsets a pointer: added to or subtracted from a pointer or address (a cast to a pointer or to `u32`/`uintptr_t`/`intptr_t`/`uptr`, `&x`, `this`, or an identifier declared as a pointer parameter or local, also through a chain `(T*)p + n + K_OFFSET`, and `p += K_OFFSET`). The definition itself is context, not a finding. A use as a plain value (buffer-length reservation, bit position or flag, array size, comparison, assignment to a field such as `p_buf->offset = L2CAP_MIN_OFFSET`, arithmetic on integers) is not R4: the Broadcom BTE headers define protocol constants with these names. A use inside `*(T*)((char*)p + K_OFFSET)` is reported once. Not seen: a pointer reached through a member (`p->data + K_OFFSET`), which has no declared type to read. Byte-pointer arithmetic with a constant whose name contains `HEADER`/`HDR` (allocator and array headers), a variable or `sizeof` offset, `offsetof`, or a `// byteptr:` note is allowed. | A file parser reading fixed offsets through a byte pointer fires: add `// byteptr: file format, <evidence>`. |
| R13 | A `virtual` method named `pad*`/`dummy*`. **Slot check** against `classes.json` (`--classes`): every class defined in a checked file that has virtual functions (own or inherited) is matched to its retail vtable by name, `__vt__` symbol, placeholder address (`unk_<addr>`: a constructor, destructor, factory or the vtable), or the mangled class of the constructors, destructors and member functions classes.json lists; a class no table fits is reported once as advisory. The source hierarchy is resolved from the parsed headers and the complete-object slots are modelled as MWCC lays them out: the primary base is the first base when it is polymorphic (CBaseBehaviour's empty first base leaves it none); its slots come first, overrides replacing the owner; then every virtual the class declares that is not in them (new virtuals and overrides of non-primary bases) in declaration order; the destructor is one slot. This is compared, by function name (`fn_8009CB1C__17CHitEffectManagerFR7unk_Msg` is CHitEffectManager's `fn_8009CB1C`, `__dt__` the destructor), with the retail words of the complete group: **[match]** an *added* slot (a declared new virtual with no retail slot), a *missing* slot (a retail slot of the class's own part whose mangled target belongs to the class and that it does not declare, or declares non-virtual), *order* (own virtuals out of retail slot order; pure virtuals take the null slots in order); **[checked]** an override the source declares where retail inherits, or a retail override (or own slot) the source does not declare as such. | Only solid evidence gives [match]/[checked]: a shape-only vtable, unresolved, unnamed (`fn_` without mangling) or null slots that cannot be placed, an implicit destructor, and a base part that does not line up with the source's primary base (the base's own check reports that) are advisory. Classes not modelled (advisory): a base not defined once in `src/`/`include/`, virtual bases. **Overloads:** each overloaded virtual has its own slot in declaration order; an override replaces the base slot with the same name and parameter types (or the only candidate when neither side overloads the name and the parameter counts agree: `release(bool)` hides `release()`, it does not override it); same-named retail slots pair by mangled parameter text where it is precise (`Fv` = no parameters), else in order. **Templates:** each retail instance of a class template (`TagProcessorBase<w>`) is checked against the template's model, and a base written `Foo<X>` resolves to the template. **Pure virtuals:** a retail slot holding 0 or `__pure_virtual` is a null slot; pure virtuals take the null slots in order. Embedded (non-primary) groups and their thunks are not compared as such: overrides of non-primary bases are checked through their complete-group slots. **Thunk slots.** A word whose function only forwards to another (`thunk` in classes.json: a this-adjusting thunk or a tail call) is named after the thunk's target, unless its own symbol is a mangled member function that is not a compiler thunk (`@N@...`): `Material::FindAnimationLink` forwards to the helper `detail::FindAnimationLink` and still implements the virtual `FindAnimationLink`. **Appended override slots.** MWCC appends to the primary table one slot per override of a non-primary base's virtual (in declaration order, after the class's own new virtuals as declared), and retail names that slot after the implementing function (an unmangled `fn_80067E74`), not after the base's virtual (`fn_8003AC14`). Such a source slot is paired with a retail own slot by name, then through the embedded thunks (`base_target` and the sub-table position of the base virtual name to the thunk's target), then, when the counts of unpaired overrides and thunk-called own slots agree, in order. A retail own slot an embedded group's thunk calls that no source override pairs with is a **[match]** *missing* slot; a pair out of order is an *order* finding. **Secondary sub-tables.** A polymorphic non-primary base with no retail vtable of its own (`unk_CPlayerBase700`) is not reported as unmatched when the derived class's vtable holds its sub-table: the groups at the class's `own_vptrs` offsets are matched, in order, to the polymorphic non-primary bases (only when their counts agree), and the base's slots are compared with the group's embedded slots: a base virtual with no group slot is **[match]** *added*, a group slot the base does not declare is **[match]** *missing* (advisory for a shape-only table); a mangled implementation whose method name differs is advisory. A class whose first base is not polymorphic (an empty `unk_BehaviourBase`) has no primary base: its own vptr is the complete group at addend 0, every polymorphic base with no vtable of its own is read from the groups after it, and a group whose slots call the class's functions directly (no thunk, so classes.json lists no embedded words) takes as many words after its header as the base declares; the words after the sub-table are the class's own part (`unk_TutorialMessageBox`). Unmangled retail names (`fn_800576C0`) are not compared with the source's names for the primary base's slots (an override is named after its implementation, not the base's virtual); only mangled slots, the slot count and the own part say whether the base lines up. A stale classes.json (its input stamp no longer matches the asm tree or symbols file) skips the slot check with a note: rerun `build/tools/classes.exe scan` after a split change. |
| R17 | `volatile` (or `vu8..vu64`, `vs8..vs64`, `vf32`, `vf64`) in a declaration, cast or macro without a `// volatile: <evidence>` note (or a note on another declaration of the same name); a volatile *local variable* always (a note cannot excuse a carrier; a pointer to a volatile hardware block with a note passes), except the SDK delay-loop counter of the user's ruling of 2026-10-18: a plain scalar `volatile T v;` (no initialiser) whose declaration carries `// volatile: delay loop, <reference project and file>` and whose every other use is in the header of one empty-bodied (`{}` or `;`) `for (v = A; <test of v>; v++)` loop (tp `__VIInit`'s `for (a = 0; a < 1000; a++) {}`); and the assert-only local of the user's ruling of 2026-11-15: a plain scalar `volatile T v;` (no initialiser, not a pointer) whose declaration carries ` (tp `__VIInit`'s `for (a = 0; a < 1000; a++) {}`); (tp `__VIInit`'s `for (a = 0; a < 1000; a++) {}`); volatile: read by a release-disabled assertion, <reference project and file>`, assigned once by a statement `v = <expr>;` that does not read it, every other use inside the arguments of a function-like macro one of whose definitions in the include closure expands to nothing (`((void)0)`, `(void)0` or empty: the SDK's release `ASSERTMSG1`), at least one such use (tp `__DSP_boot_task`'s `mail`); the note without that shape, or the shape without the note, still fires. A file-scope or global volatile with its note passes (ruling 7(a) of 2026-11-15 needs no checker change); a `static volatile` local is still a local; the note without that shape, or the shape without the note, still fires; `typedef`s that define the volatile names are skipped. Empty `if`/`else` body or block (except an empty `{ }` branch of an `if` or `else if` that is followed by `else if`/`else`, with or without a comment inside: Broadcom's `if (a == NO_ACTION) { } else if (a == PARK) ...`, user's ruling 2026-10-23; a lone `if (x) { }`, the last branch of a chain and an empty `else { }` still fire), bare `;` statement (empty loop bodies, `case X: ;`, `label: ;`, `for (...) { ; }` pass), `{ }` block; `do { } while (0)` in a function body (macros pass); `x = x;` self-assignment; token `+ 0` and `* 1`, except a whole subscript `[X * 1]` / `[X + 0]` when the function also subscripts `[X * n]` / `[X + n]` with another literal (an index series: IJG's `inptr[DCTSIZE*1]`, `errorptr[dir3+0]`); a local that is never read (only declared or assigned; a name in the body of a macro the function uses counts as a read, and so does a name in the arguments of a macro call, also one that expands to nothing in release (the engine debug-warning `RWS_WARN` of `include/climax/Debug.h`, `RWASSERT`; user's ruling, 2026-11-16); C++ objects with a constructor are skipped; an R36/BUG/NOTE comment within two lines before excuses it); padding members named `pad`, `padding`, `_pad`, `pad0` (names like `fpscr_pad` pass; a `// pad:` note passes); a comment that is an explicit probe marker (`PROBE`, `fakematch`, a comment starting `probe:`/`steering:`). **No R17 finding inside a function whose closing line carries the plain `// no copy in retail` marker (optionally `// no copy in retail (dead-stripped)` or `// no copy in retail (dead-stripped or inlined)`)**: the body is the reference's text kept for a function retail dropped, not matched code (user's ruling, 2026-10-23); other rules still apply there, and an `(inlined)`-only marker does not exempt (that code reaches retail through callers). | An intentionally unused local with a side-effecting initialiser needs an R36 note. `vuNN` locals fire like `volatile`. |
| R18 | An `asm`/`__asm` function body, an `asm { }`/`asm ( )` block, `INCLUDE_ASM`, `__emit`, reported on the function whose own body holds the assembly (user's ruling, 2026-11-15). A helper with an `asm` block defined in the same source file and called once (one call site in the file's other function bodies) is the caller's: the finding is reported at the call, in the caller (an `asm` function, or a body listed as ORIGINAL-ASM, keeps its own finding). A header-level `asm` inline is a helper of the original: listed in the table "Header-level asm inlines (ORIGINAL-ASM)" by its header path (`include/game/VectorPS.h`, keyed without the extension) it is allowed, its function status is `ORIGINAL-ASM` and its callers have no finding; unlisted it is "helper not listed as ORIGINAL-ASM" (neither a source marker nor a `Listed inline-asm helpers` row excuses it). Otherwise allowed (printed under "listed originals"): functions in the `ORIGINAL-ASM` table, helpers in the `Listed inline-asm helpers` table (by helper or by caller), a source marker above the helper or the caller. A prototype that merely carries `asm` is not a body. | An `asm` function the evidence shows was original assembly must be added to `docs/exceptions.md`. |

### Advisory and [checked]/[free] rules (reported, never blocking)

| ID | tier | detection | known false positives |
|---|---|---|---|
| R0 | free | `RULE-EXCEPTION` malformed, unknown rule, weak reason, or aimed at a [match] finding. | |
| R3 | free | Header without an include guard or `#pragma once`. Header section order is not checked. | |
| R7 | checked | Placeholder names: the profile (`fn_XXXXXXXX`, `lbl_XXXXXXXX`, `unk_` with a hex suffix or an invented role (`unk_Ramp`), `field_0x`, `dtor_/sinit_/jumptable_/gap_XXXXXXXX`, `stringBase`), the skill's list (`FUN_`, `DAT_`, `sub_`, `local_`, `param_`, `iVar3`, `temp1`, `ptr2`, `result2`, `v3`), m2c artefacts (`var_*`, `temp_*`, `phi_*`, `saved_reg*`, `spNN`, `M2C_*`), and the skill's address-like regex. One finding per name per function, with a use count. | Original MSL code uses `v1`/`v2` (real names; the numbered-generic rule is the skill's). Whitelist in `ADDRESS_LIKE_WHITELIST`. |
| R9 | free | Decimal power-of-two or all-ones mask/flag next to a bitwise operator; a hex literal as a small comparison bound; a raw float-bit hex literal in a statement mentioning `float`/`f32`. | |
| R11 | free | A literal of 0x1000 or more in a function body with no comment on the same or the previous line. | Hardware addresses and masks with an obvious meaning; add a comment or a name. |
| R14 | checked | Every `goto`, noting a non-meaningful label (`L_01`, `loc_`, `LAB_`). | `goto fail`-style cleanup is genuine; the finding documents it. |
| R15 | checked | Annotated function addresses (`} // 0x8XXXXXXX`) that decrease in file order (non-static, non-member functions only; Metrowerks reorders statics/inlines; function templates and functions in an unnamed namespace are skipped: a template instance is emitted where it is instantiated, as `lyt_textBox.cpp`'s `CalcStringRectImpl`). | |
| R17 (advisory) | advisory | `register` locals (genuine only where the codebase uses it; listed asm helpers are exempt), `!!x`, `if (0)`/`if (1)`, bare `{ }` scope blocks. The braces of an aggregate initialiser (`= { {a, b}, {} }`, nested lists included) are not blocks. | |
| R19 | checked / free | Cast chains of pointer casts, a cast of a parameter or simple local to its own declared type: [checked] for pointer casts, [free] for arithmetic casts (the skill's tiers). A cast that adds or removes `const` (`(char*)utf8` of a `const char*`) is not to its own type. | `(char*)ptr` to document a boundary. |
| R21 | free | An address (`0x8XXXXXXX`) in a comment other than the `// 0x8XXXXXXX` annotation; dates, percentages, `objdiff`, `permuter`, `plateau`, `matched at N`. | |
| R22 | free | C++11 words (`nullptr`, `constexpr`, `noexcept`, `static_assert`, `decltype`, `enum class`, ...) in C++ units; `<stdint.h>`, `<stdbool.h>`. | |
| R25 | checked | `()` instead of `(void)` in C definitions and prototypes; a prototype in a source file for a function not defined there; a local `extern` function declaration. An object definition with constructor arguments is not a prototype: a qualified name at file scope (`const T Cls::member(&Base::member);`, a static member definition) and a declarator whose arguments start with a literal, `&`, `*`, `-`, `!` or `~` (`unk_801A8F6C lbl_8052968C("CHitEffectManager", ...);`) cannot be parameter lists. The language of a plain `.h` header is decided before any C-only rule, from evidence in this order: (1) its own text uses C++-only syntax (`namespace`, `template <`, `class Name`, `public:`/`private:`/`protected:`, `virtual`, `typename`, `operator`, `friend`, `explicit`, `::`; `class`/`operator`/`private` count only in their C++ shape, and `extern "C"` under `#ifdef __cplusplus` is not such syntax): C++, whatever includes it; (2) only C++ sources include it (transitively): C++; any C source includes it: C; (3) no source includes it: C++ under `include/game/` and `include/climax/` (`CPP_HEADER_DIRS`), else C. `.hh`/`.hpp`/`.hxx` are always C++. The decision also feeds every other language-dependent check (R22 dialect words, the R1 class-keyword and private-type rules, the R13 class index, the unused-local read count). | |
| R26 | checked | `((T*)param)->m` / `[i]` where `param` is a `void*` parameter. | The Metrowerks runtime writes this form. |
| R31 | free | Function-like macros whose parameters are not parenthesised, or whose operator expansion is not parenthesised. | Original SDK/runtime macros (`va_start`, `CTORCALL_COMPLETE`). |
| R33 | free | Function (longer than a trivial body) with no whole-line comment directly above. | |
| R35 | free/checked | `if`/`else`/`for`/`while`/`do` bodies without braces; an assignment inside a condition (the idioms `!(p = f())` and `(p = f()) != 0` pass; in C++ a type-led condition declaration `if (void* p = f())` is a declaration with an initialiser, not an assignment, and only its initialiser is scanned); a comma operator outside `for` headers (the commas of an `= { ... }` initialiser list, nested lists included, are not comma operators). | |
| R37 | checked | `#if 0`, `#if 1`, `#ifdef NON_MATCHING`/`NONMATCHING`; a `NONMATCHING` comment in a Matching unit. | |
| R4 (advisory) | advisory | A pointer formed from a byte pointer plus a small literal offset without a dereference. | Allocator walks; ignore or add `// byteptr:`. |

## Rules not mechanically checkable

These rules are in `HAND_REVIEWED` (rules_final.c): no check emits them, whatever their tier in `RULES` (a
[checked] tier is the skill's, a reviewer's duty, not a claim that this tool tests it). R5 (enum domains), R6
(original types/APIs), R8, R29, R28 (naming style and prefixes need the module style),
R10, R12 (placement needs evidence), R16 (helper recovery), R20 (const/reference evidence), R23 (formatting is
per-module style), R24 (generated material), R27 (struct layout), R30 beyond dummy locals and numbered names
(merged variables), R32 (unit layout is per-module), R34 (function boundaries need the binary), R36 (quirk labels
need judgement). R13 beyond the slot check (unmatched classes, shape-only tables, unresolved or unnamed slots,
virtual bases, templates, overloaded virtuals, the thunks of non-primary groups). Review these by hand against
`decomp-code-rules/references/rules.md`.

## Maintenance

The profile constants (placeholder patterns, byte-pointer types, offset-constant and header-constant names,
address-like whitelist) sit in `tools/c/tools/check_rules`. Tiers follow the skill; do not move a rule into
[match] without a mechanical row. Run the check_rules tests (`ctest --preset tools -R check_rules`) after any change. The smoke test
of the live tree (the merged `src/` has no blocking finding) is always on in the C test.
