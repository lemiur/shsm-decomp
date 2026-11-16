/* levers: the Python's module docstring (the --help epilog) and RULE, generated from the original docstring;
 * keep the two in step. */
#pragma once

#define LV_RULE \
    "Search output is evidence of what the compiler is sensitive to, not source. A lever result is committed only if the resulting function passes the reader test (decomp-code-rules), and each use is recorded in docs/exceptions.md."

#define LV_EPILOG \
    "Lever search: honest single source edits to one function, compiled with the unit's exact flags and scored\n" \
    "against retail. Works on C and C++ alike (it edits source text; decomp-permuter parses only C).\n" \
    "\n" \
    "Reads the unit table and compiles with the unit's flags (below-normal priority, private directories under\n" \
    "build/tmp/levers/), scores by aligned rows and ordered relocation targets (codecmp's rules), and\n" \
    "writes build/levers/<unit>/<function>/: report.json and one\n" \
    "unified diff per improving state (NNN.diff, `git apply` accepts it). The source file is never touched except\n" \
    "by --apply.\n" \
    "\n" \
    "    build/tools/levers.exe <unit> <function> [--file F] [--depth 1..4] [--frontier K] [-j N] [--minutes M]\n" \
    "                           [--stop-on-exact] [--family decl,expr,...] [--limit N|--all] [--json]\n" \
    "    build/tools/levers.exe <unit> <function> --show N      # print diff N of the last run\n" \
    "    build/tools/levers.exe <unit> <function> --apply N     # apply diff N to the searched source file\n" \
    "    build/tools/levers.exe --batch [--unit U | --lib L | --status NonMatching] [--min-percent 90] [--minutes M]\n" \
    "\n" \
    "A lever is one mechanical edit that turns valid source into source a programmer could have typed, with the same\n" \
    "meaning. Families (order follows build/walls.json when wall.exe has classified the function: register and\n" \
    "schedule walls try decl/temp/order/expr first, control-flow walls try control first; all run within budget):\n" \
    "  decl     swap two adjacent declarations; move a declaration to just before its first use or to the top of\n" \
    "           its block; split `T a = x;` into declaration and assignment, and merge it back; split and merge\n" \
    "           multi-declarator lines\n" \
    "  expr     swap the operands of + * & | ^ == !=; mirror a comparison (a < b, b > a); x = x + n and x += n;\n" \
    "           i++ and ++i as statements; !(a == b) and a != b; reassociate a + b + c (integers only for + *)\n" \
    "  control  a ? b : c and the if/else assignment or return; invert an if/else; early return versus wrapping\n" \
    "           the rest in the if; `if (c) {..return..} rest` versus `else { rest }`; for and while; while and\n" \
    "           if/do-while (only where the file already has a do statement); merge nested ifs into && and split\n" \
    "  temp     inline a single-use local (side-effect-free initialiser, nothing in between writes or calls);\n" \
    "           introduce a named local for a subexpression used more than once (type known from a cast or an\n" \
    "           existing declaration only)\n" \
    "  type     signed/unsigned of a loop counter or index local; int/s32, float/f32 spellings of locals\n" \
    "  order    swap two adjacent independent assignments (no calls, distinct targets, no read of the other's target)\n" \
    "Every candidate passes the fake-form filter before it is compiled: no new volatile, register, asm, pragma,\n" \
    "goto, empty statement or block, do { } while (0), comma operator, cast, or unused local.\n" \
    "\n" \
    "Scoring: the target function's (differing rows, relocation-target mismatches). The whole unit is compiled and\n" \
    "compared; a candidate is acceptable only if no other function of the unit got worse than with the unmodified\n" \
    "source; one that improves the target at another's cost is listed apart (\"breaks <fn>\"), never as a success.\n" \
    "--depth 2 and deeper continue from a frontier of the best distinct states (distinct by compiled code).\n" \
    "\n" \
    "The project's rule for this kind of result: Search output is evidence of what the compiler is sensitive to, not source. A lever result is committed only if the resulting function passes the reader test (decomp-code-rules), and each use is recorded in docs/exceptions.md.\n"
