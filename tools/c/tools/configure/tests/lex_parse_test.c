/* configure: the lexer, the parser and the evaluator of the configure.py subset on small sources (valid ones: an
 * invalid source ends the process, which the differential harness's cases_unsupported.txt covers). The expected
 * values are what CPython 3.14 gives for the same text. */
#include <string.h>

#include "avp/fmt.h"
#include "avp/test.h"
#include "cfg.h"

static CfgToks lex(const char *src) { return cfg_lex(avp_arena(), avp_str(src), 1, false); }

/* the token stream as one line: N:name I:int S:'str' F:'fbody' O:op NL IN DE EOF */
static AvpStr toks(const char *src) {
    AvpArena *a = avp_arena();
    CfgToks t = lex(src);
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < t.n; i++) {
        if (i) avp_buf_putc(&b, ' ');
        CfgTok *k = &t.v[i];
        switch (k->kind) {
        case CFG_T_EOF: avp_buf_putz(&b, "EOF"); break;
        case CFG_T_NEWLINE: avp_buf_printf(&b, "NL%d", k->line); break;
        case CFG_T_INDENT: avp_buf_putz(&b, "IN"); break;
        case CFG_T_DEDENT: avp_buf_putz(&b, "DE"); break;
        case CFG_T_NAME: avp_buf_printf(&b, "N:%.*s", AVP_SF(k->text)); break;
        case CFG_T_INT: avp_buf_printf(&b, "I:%lld", (long long)k->ival); break;
        case CFG_T_STR:
            avp_buf_putz(&b, k->fstr ? "F:" : "S:");
            avp_buf_put(&b, avp_repr_str(a, k->text));
            break;
        case CFG_T_OP: avp_buf_printf(&b, "O:%.*s", AVP_SF(k->text)); break;
        }
    }
    return avp_buf_str(&b);
}

AVP_TEST(lexer_comments_indentation_and_joining) {
    CHECK_STR(toks("x = 1  # comment\n"), "N:x O:= I:1 NL1 EOF");
    CHECK_STR(toks("# only a comment\n\n   \nx\n"), "N:x NL4 EOF");
    CHECK_STR(toks("if a:\n    b\n\n    # c\n    c\nd\n"),
              "N:if N:a O:: NL1 IN N:b NL2 N:c NL5 DE N:d NL6 EOF");
    CHECK_STR(toks("f(a,\n  b)  # in brackets\nx = \\\n 2\n"), "N:f O:( N:a O:, N:b O:) NL2 N:x O:= I:2 NL4 EOF");
    CHECK_STR(toks("def f():\n  if x:\n    y"), "N:def N:f O:( O:) O:: NL1 IN N:if N:x O:: NL2 IN N:y NL3 DE DE EOF");
    CHECK_STR(toks("a -> b != c ** d ... 0x1F 1_000"), "N:a O:-> N:b O:!= N:c O:** N:d O:... I:31 I:1000 NL1 EOF");
}

AVP_TEST(lexer_strings) {
    CHECK_STR(toks("'a' \"b\" r'\\w' u'x'"), "S:'a' S:'b' S:'\\\\w' S:'x' NL1 EOF");
    CHECK_STR(toks("'\\t\\n\\x41\\101\\u00e9\\\\\\'\\q'"), "S:\"\\t\\nAA\u00e9\\\\'\\\\q\" NL1 EOF");
    CHECK_STR(toks("\"\"\"one\ntwo 'q' \"\"\"\nx"), "S:\"one\\ntwo 'q' \" NL2 N:x NL3 EOF");
    CHECK_STR(toks("'''a\\\nb'''"), "S:'ab' NL2 EOF");
    CHECK_STR(toks("f'{x}' F\"a{{b}}\" rf'\\d{y}'"), "F:'{x}' F:'a{{b}}' F:'\\\\d{y}' NL1 EOF");
    CHECK_STR(toks("x = 'h\xc3\xa9'"), "N:x O:= S:'h\u00e9' NL1 EOF");
}

static CfgOptions g_opts;

static Pv *run(const char *src) {
    memset(&g_opts, 0, sizeof g_opts);
    g_opts.prog = "configure.py";
    g_opts.script_path = "configure.py";
    g_opts.env.script = "configure.py";
    return cfg_exec_source(avp_arena(), &g_opts, avp_str(src));
}

static AvpStr get(Pv *globals, const char *name) {
    Pv *v = pv_dict_get(globals, avp_str(name));
    return v ? pv_repr(avp_arena(), v) : AVP_S("<missing>");
}

AVP_TEST(fstrings_and_concatenation) {
    Pv *g = run("v = 'R5'\nn = 3\n"
                "a = f'-I build/{v}/include'\n"
                "b = 'x' \"y\" f'{n}' r'\\w' f\"{{{v}}}\"\n"
                "c = f'{v!r} {[n, None]} {n if n else 0}'\n"
                "d = f'''multi\n{v}'''\n"
                "e = f'{ {\"k\": 1}[\"k\"] }'\n");
    CHECK_STR(get(g, "a"), "'-I build/R5/include'");
    CHECK_STR(get(g, "b"), "'xy3\\\\w{R5}'");
    CHECK_STR(get(g, "c"), "\"'R5' [3, None] 3\"");
    CHECK_STR(get(g, "d"), "'multi\\nR5'");
    CHECK_STR(get(g, "e"), "'1'");
}

AVP_TEST(annotations_defs_and_control_flow) {
    Pv *g = run("from typing import Any, Dict, List\n"
                "x: int = 5\n"
                "y: List[str]\n"
                "def f(a: str, b: Dict[str, Any] = None, *rest) -> Dict[str, Any]:\n"
                "    \"\"\"doc\"\"\"\n"
                "    if b is None:\n"
                "        return {'a': a, 'rest': rest}\n"
                "    elif a == 'x':\n"
                "        pass\n"
                "    else:\n"
                "        return b\n"
                "r1 = f('q')\n"
                "r2 = f('q', {'k': 1})\n"
                "r3 = f('x', {}, 1, 2)\n"
                "out = []\n"
                "for k, v in {'a': 1, 'b': 2}.items():\n"
                "    if k == 'b':\n"
                "        continue\n"
                "    out.append((k, v))\n"
                "for (i, j) in [(1, 2)]: out.extend([i, j])\n"
                "s = 1; t = 2;\n"
                "chain = u = 'same'\n");
    CHECK_STR(get(g, "x"), "5");
    CHECK_STR(get(g, "y"), "<missing>");
    CHECK_STR(get(g, "r1"), "{'a': 'q', 'rest': ()}");
    CHECK_STR(get(g, "r2"), "{'k': 1}");
    CHECK_STR(get(g, "r3"), "None");
    CHECK_STR(get(g, "out"), "[('a', 1), 1, 2]");
    CHECK_STR(get(g, "t"), "2");
    CHECK_STR(get(g, "chain"), "'same'");
    CHECK_STR(get(g, "u"), "'same'");
}

AVP_TEST(lists_alias_and_star_copies) {
    Pv *g = run("base = ['-a']\n"
                "alias = {'cflags': base}\n"
                "copy = [*base, '-b']\n"
                "base.append('-c')\n"
                "both = base + copy\n"
                "nums = (1, *[2, 3])\n"
                "idx = both.index('-b')\n"
                "neg = both[-1]\n");
    CHECK_STR(get(g, "alias"), "{'cflags': ['-a', '-c']}");
    CHECK_STR(get(g, "copy"), "['-a', '-b']");
    CHECK_STR(get(g, "both"), "['-a', '-c', '-a', '-b']");
    CHECK_STR(get(g, "nums"), "(1, 2, 3)");
    CHECK_STR(get(g, "idx"), "3");
    CHECK_STR(get(g, "neg"), "'-b'");
}

AVP_TEST(paths_and_project_classes) {
    Pv *g = run("from pathlib import Path\n"
                "from tools.project import Object, ProjectConfig, ProgressCategory, is_windows\n"
                "p = Path('config') / 'R5WEA4' / 'config.yml'\n"
                "q = Path('a', 'b/c').parent\n"
                "w = is_windows()\n"
                "c = ProjectConfig()\n"
                "c.version = str(p.name)\n"
                "o = Object(True, 'x/y.c', extra_cflags=['-O0'])\n"
                "opt = o.options['extra_cflags']\n"
                "cat = ProgressCategory('g', 'Game').name\n"
                "n = getattr(c, 'version')\n"
                "setattr(c, 'objcache', True)\n"
                "oc = c.objcache\n");
    CHECK_STR(get(g, "p"), "WindowsPath('config/R5WEA4/config.yml')");
    CHECK_STR(get(g, "q"), "WindowsPath('a/b')");
    CHECK_STR(get(g, "w"), "True");
    CHECK_STR(get(g, "opt"), "['-O0']");
    CHECK_STR(get(g, "cat"), "'Game'");
    CHECK_STR(get(g, "n"), "'config.yml'");
    CHECK_STR(get(g, "oc"), "True");
}

AVP_TEST(operators_follow_python) {
    Pv *g = run("a = 1 < 2 <= 2 != 3\n"
                "b = 'x' in ['x'] and 'y' not in {'x': 1}\n"
                "c = 0 or '' or None\n"
                "d = [] or 'fallback'\n"
                "e = not 'x'\n"
                "f = -(1 - 3) + 2 * 3\n"
                "g = 'big' if f > 5 else 'small'\n"
                "h = None is None is not False\n"
                "i = True == 1\n");
    CHECK_STR(get(g, "a"), "True");
    CHECK_STR(get(g, "b"), "True");
    CHECK_STR(get(g, "c"), "None");
    CHECK_STR(get(g, "d"), "'fallback'");
    CHECK_STR(get(g, "e"), "False");
    CHECK_STR(get(g, "f"), "8");
    CHECK_STR(get(g, "g"), "'big'");
    CHECK_STR(get(g, "h"), "True");
    CHECK_STR(get(g, "i"), "True");
}

AVP_TEST_MAIN("configure")
