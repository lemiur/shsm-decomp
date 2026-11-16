/* permute's docstring, which its --help prints (description: the first paragraph; epilog: the rest). */
#pragma once
#define PERMUTE_DESCRIPTION \
    "Set up (and optionally run or score) a decomp-permuter directory for one function of one unit."
#define PERMUTE_EPILOG \
    "Usage:\n" \
    "  build/tools/permute.exe <unit> <function> [--run] [-j N] [--stop-on-zero] [-- extra permuter args]\n" \
    "  build/tools/permute.exe <unit> <function> --score [--file F]\n" \
    "  common options: --lang {auto,c,c++}  --root DIR  --out DIR  --no-check  --keep-all\n" \
    "\n" \
    "  <unit>      e.g. MSL_C/MSL_Common/Src/mem_funcs (as in objdiff.json, \"main/\" optional)\n" \
    "  <function>  symbol name as in symbols.txt, e.g. __copy_longs_rev_unaligned. For a C++ unit a\n" \
    "              mangled symbol (DrawFrame__Q36nw4hbm3lyt6WindowFRC...) or its source name when that\n" \
    "              names exactly one function in the unit's retail asm (DrawFrame).\n" \
    "\n" \
    "Creates build/permute/<function>/ with:\n" \
    "  base.c|.cpp   the unit preprocessed by mwcceppc -E with the unit's flags, other function bodies\n" \
    "                stripped (declarations kept; calls to them stay out of line)\n" \
    "  target.s/.o   the retail function from build/R5WEA4/asm/<unit>.s, assembled with gc-wii-binutils\n" \
    "                (skipped with a note when build/binutils is missing; --score does not need it)\n" \
    "  compile.sh    mwcceppc with the unit's exact compiler version and cflags (from build.ninja)\n" \
    "  settings.toml func_name, compiler_type \"mwcc\", the PowerPC objdump path\n" \
    "and prints the base's score: it compiles the base and compares the function, found by its symbol\n" \
    "name in the compiled object, with the retail block (codecmp, relocation targets checked).\n" \
    "\n" \
    "Language. A .c unit is compiled as C. A .cpp unit is compiled as C++ with the unit's own flags\n" \
    "(base.cpp, the function found by its mangled name; other out-of-class function bodies are stripped, and\n" \
    "the stripped definitions of member functions are dropped because their class declares them).\n" \
    "A .cpp unit's unmangled (extern \"C\") function keeps the older route: the extern \"C\" wrappers are removed\n" \
    "and base.c is compiled as C. --lang overrides the choice.\n" \
    "\n" \
    "What runs on which language. decomp-permuter parses base.c with pycparser, a C parser: it cannot read\n" \
    "classes, templates, references or `Class::Function` definitions. So --run works for C bases only and is\n" \
    "refused for a C++ base. For a C++ base the directory is a compile-and-score harness: edit base.cpp by hand\n" \
    "(or generate variants) and run `--score` after each edit to see the row differences and relocation\n" \
    "target differences against retail. The permuter's own scoring (target.o, compile.sh) is the same, but its\n" \
    "randomizer cannot start from a C++ base.\n" \
    "\n" \
    "--score [--file F] does not regenerate anything: it compiles F (default: the directory's base) and\n" \
    "compares the function. Exit code 0 only when the function is exact.\n" \
    "--root DIR takes the build, retail asm, compilers and tools from another checkout (a lane slot).\n" \
    "--out DIR puts the directory elsewhere than <root>/build/permute/<function>.\n" \
    "\n" \
    "Requirements (see docs/toolchain.md): build/tools/configure.exe && tools/bin/ninja.exe (build.ninja, retail asm,\n" \
    "build/binutils for target.o: `tools/bin/ninja.exe build/binutils`), and for --run decomp-permuter in\n" \
    "tools/permuter with its Python packages (toml, Levenshtein) in tools/venv. --run starts\n" \
    "permuter_run.exe (tools/permuter/permuter.py in the venv Python); results land in build/permute/<function>/output-*/.\n" \
    "\n" \
    "Permuter output is evidence, never source: translate a high-scoring state into an authentic change\n" \
    "(see the permute skill and docs/compiler_regalloc.md).\n" \
    ""
