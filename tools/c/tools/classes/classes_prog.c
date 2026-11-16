/* classes_prog.c: analyze_function (the linear register-tracking pass), Program (from_dir, from_texts, cstring,
 * is_func, resolve_thunk, faddr), is_rtti_word, vtable_prefix, find_vtables, find_embedded_vtables and hx of
 * tools/classes.py. Python's crashes on impossible operands (a register number above r31, a non-register where a
 * register is assigned) become "no register" here: dtk never writes such rows. */
#include "classes_prog.h"

#include "shsm_asm.h"
#include "shsm_cli.h"

static AvpStr sub(AvpStr s, size_t i, size_t j) { return avp_strn(s.p + i, j - i); }
static bool eqz(AvpStr s, const char *z) { return avp_str_eqz(s, z); }
static bool startz(AvpStr s, const char *z) {
    size_t n = strlen(z);
    return s.n >= n && memcmp(s.p, z, n) == 0;
}
static bool endz(AvpStr s, const char *z) {
    size_t n = strlen(z);
    return s.n >= n && memcmp(s.p + s.n - n, z, n) == 0;
}

/* ^r(\d+)$ -> the register (0..31), -1 when none. */
static int gpr(AvpStr op) {
    if (op.n < 2 || op.p[0] != 'r') return -1;
    for (size_t i = 1; i < op.n; i++) {
        u8 c = (u8)op.p[i];
        if (c >= 0x80) {
            /* a non-ASCII decimal digit: let int() decide */
            i64 v;
            if (!avp_str_isdecimal(sub(op, 1, op.n)) || !avp_parse_int(sub(op, 1, op.n), 10, &v, NULL)) return -1;
            return v >= 0 && v < 32 ? (int)v : -1;
        }
        if (c < '0' || c > '9') return -1;
    }
    i64 v;
    if (!avp_parse_int(sub(op, 1, op.n), 10, &v, NULL)) return -1;
    return v >= 0 && v < 32 ? (int)v : -1;
}

static bool is_hexc(char c) { return (c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'f'); }
static bool is_dec_str(AvpStr s) { return s.n && avp_str_isdecimal(s); }

/* RE_MEM ^(-?(?:0x[0-9A-Fa-f]+|\d+))\((r\d+)\)$ -> (offset, base register); false when no match. */
static bool mem_base(AvpStr op, i64 *off, int *base) {
    if (op.n < 5 || op.p[op.n - 1] != ')') return false;
    i64 lp = avp_str_rfind(op, AVP_S("("), 0, AVP_END);
    if (lp <= 0) return false;
    AvpStr num = sub(op, 0, (size_t)lp), reg = sub(op, (size_t)lp + 1, op.n - 1);
    if (reg.n < 2 || reg.p[0] != 'r' || !is_dec_str(sub(reg, 1, reg.n))) return false;
    size_t i = num.n && num.p[0] == '-' ? 1 : 0;
    bool ok = false;
    if (num.n >= i + 3 && num.p[i] == '0' && num.p[i + 1] == 'x') {
        ok = true;
        for (size_t k = i + 2; k < num.n; k++)
            if (!is_hexc(num.p[k])) ok = false;
    }
    if (!ok) ok = is_dec_str(sub(num, i, num.n));
    if (!ok) return false;
    if (!classes_parse_int(num, off)) return false;
    i64 r;
    if (!avp_parse_int(sub(reg, 1, reg.n), 10, &r, NULL)) return false;
    *base = r >= 0 && r < 32 ? (int)r : -1;
    return true;
}

/* SAVE_REST ^_(save|rest)[gf]pr_\d+(_x)?$ */
static bool save_rest(AvpStr t) {
    if (!(startz(t, "_save") || startz(t, "_rest")) || t.n < 10) return false;
    if (t.p[5] != 'g' && t.p[5] != 'f') return false;
    if (memcmp(t.p + 6, "pr_", 3) != 0) return false;
    AvpStr d = sub(t, 9, t.n);
    if (endz(d, "_x")) d = sub(d, 0, d.n - 2);
    return is_dec_str(d);
}

bool classes_val_eq(const ClassesVal *x, const ClassesVal *y) {
    if (x->kind != y->kind) return false;
    switch (x->kind) {
    case CLASSES_V_NONE: return true;
    case CLASSES_V_IMM: return x->v == y->v;
    case CLASSES_V_HA:
    case CLASSES_V_SDA: return avp_str_eq(x->name, y->name);
    default: return x->v == y->v && avp_str_eq(x->name, y->name);
    }
}

const ClassesNew *classes_info_nw(const ClassesInfo *inf, AvpStr tok) {
    for (size_t i = 0; i < inf->nw.n; i++)
        if (avp_str_eq(inf->nw.v[i].tok, tok)) return &inf->nw.v[i];
    return NULL;
}

/* ---- analyze_function ----------------------------------------------------------------------------------------- */

typedef struct {
    int width;
    const char *kind;
    int sign; /* -1 None */
    bool store;
} MemOp;

static bool mem_op(AvpStr m, MemOp *o) {
    static const struct {
        const char *mn;
        int width;
        const char *kind;
        int sign;
        bool store;
    } T[] = {
        {"lbz", 1, "int", 0, false},   {"lbzu", 1, "int", 0, false},  {"lhz", 2, "int", 0, false},
        {"lhzu", 2, "int", 0, false},  {"lha", 2, "int", 1, false},   {"lhau", 2, "int", 1, false},
        {"lwz", 4, "int", -1, false},  {"lwzu", 4, "int", -1, false}, {"lfs", 4, "float", -1, false},
        {"lfsu", 4, "float", -1, false}, {"lfd", 8, "double", -1, false}, {"lfdu", 8, "double", -1, false},
        {"stb", 1, "int", -1, true},   {"stbu", 1, "int", -1, true},  {"sth", 2, "int", -1, true},
        {"sthu", 2, "int", -1, true},  {"stw", 4, "int", -1, true},   {"stwu", 4, "int", -1, true},
        {"stfs", 4, "float", -1, true}, {"stfsu", 4, "float", -1, true}, {"stfd", 8, "double", -1, true},
        {"stfdu", 8, "double", -1, true},
    };
    if (m.n < 3 || m.n > 5 || (m.p[0] != 'l' && m.p[0] != 's')) return false;
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) {
        if (eqz(m, T[i].mn)) {
            o->width = T[i].width;
            o->kind = T[i].kind;
            o->sign = T[i].sign;
            o->store = T[i].store;
            return true;
        }
    }
    return false;
}

static bool in_nodest(AvpStr m) {
    static const char *const N[] = {
        "cmpw",  "cmplw", "cmpwi", "cmplwi", "cmp",    "cmpl",   "cmpi",     "cmpli",    "mtctr",  "mtlr",
        "mtspr", "mtcrf", "mtmsr", "mtsr",   "tw",     "twi",    "dcbf",     "dcbi",     "dcbst",  "dcbt",
        "dcbz",  "dcbz_l", "icbi", "sync",   "isync",  "eieio",  "stmw",     "stswi",    "stwbrx", "sthbrx",
        "stwx",  "stbx",  "sthx",  "stfsx",  "stfdx",  "stwux",  "stfiwx",   "psq_stx",  "psq_stux", "mtfsf",
        "mtfsb0", "mtfsb1",
    };
    for (size_t i = 0; i < sizeof N / sizeof N[0]; i++)
        if (eqz(m, N[i])) return true;
    return false;
}

typedef struct {
    ClassesVal regs[32];
    int pend_sign[32]; /* reg -> access index awaiting extsb/extsh, -1 none */
    int loaded[32];    /* reg -> access index of a word loaded from an object, -1 none */
} Regs;

static void kill(Regs *R, int r) {
    if (r < 0) return;
    R->regs[r].kind = CLASSES_V_NONE;
    R->pend_sign[r] = -1;
    R->loaded[r] = -1;
}
static void kill_call(Regs *R) {
    kill(R, 0);
    for (int r = 3; r <= 12; r++) kill(R, r);
}

static const AvpStr TOK_A3 = {"a3", 2};

ClassesInfo *classes_analyze_function(AvpArena *a, const ClassesFunc *f, const AvpSet *returns_this) {
    ClassesInfo *info = AVP_NEW(a, ClassesInfo);
    const ClassesInsn *ins = f->insns.v;
    size_t nins = f->insns.n;
    /* thunk shape */
    if (nins == 2 && eqz(ins[1].mn, "b") && ins[0].nops == 3 && eqz(ins[0].ops[0], "r3") && eqz(ins[0].ops[1], "r3") &&
        (eqz(ins[0].mn, "subi") || eqz(ins[0].mn, "addi"))) {
        i64 v;
        if (classes_parse_int(ins[0].ops[2], &v)) {
            i64 adj = eqz(ins[0].mn, "subi") ? v : (i64)(0 - (u64)v);
            if (adj != 0 && ins[1].nops > 0 && !startz(ins[1].ops[0], ".L")) {
                info->has_thunk = true;
                info->thunk_adjust = adj;
                info->thunk_target = classes_unquote(ins[1].ops[0]);
            }
        }
    }
    Regs R;
    memset(&R, 0, sizeof R);
    for (int r = 0; r < 32; r++) R.pend_sign[r] = R.loaded[r] = -1;
    R.regs[3] = (ClassesVal){CLASSES_V_OBJ, TOK_A3, 0};
    i64 blrs = 0, this_rets = 0;

    for (size_t idx = 0; idx < nins; idx++) {
        const ClassesInsn *in = &ins[idx];
        AvpStr m0 = avp_str_rstrip_chars(in->mn, AVP_S(".+-"));
        const AvpStr *ops = in->ops;
        u32 nops = in->nops;
        /* pointer use of a loaded word: any memory operand whose base was loaded from this */
        for (u32 k = 0; k < nops; k++) {
            i64 off;
            int b;
            if (ops[k].n && ops[k].p[ops[k].n - 1] == ')' && mem_base(ops[k], &off, &b) && b >= 0 && R.loaded[b] >= 0)
                info->access.v[R.loaded[b]].pointer = true;
        }
        MemOp mo;
        bool is_mem = mem_op(m0, &mo);
        bool is_psq = eqz(m0, "psq_l") || eqz(m0, "psq_lu") || eqz(m0, "psq_st") || eqz(m0, "psq_stu");
        bool is_lmw = eqz(m0, "lmw");
        if (is_mem || is_psq || is_lmw) {
            if (nops < 2) {
                kill(&R, nops ? gpr(ops[0]) : -1);
                continue;
            }
            i64 off = 0;
            int base = -1;
            bool mb = mem_base(ops[1], &off, &base);
            int width, sign;
            const char *kind;
            bool load;
            if (is_mem) {
                width = mo.width;
                kind = mo.kind;
                sign = mo.sign;
                load = !mo.store;
            } else if (is_psq) {
                bool w1 = nops > 2 && eqz(avp_str_strip(ops[2]), "1");
                width = w1 ? 4 : 8;
                kind = "paired";
                sign = -1;
                load = startz(m0, "psq_l");
            } else {
                width = 4;
                kind = "int";
                sign = -1;
                load = true;
            }
            int acc_idx = -1;
            if (mb) {
                ClassesVal bv = {0};
                if (base != 1 && base >= 0) bv = R.regs[base];
                if (bv.kind == CLASSES_V_OBJ) {
                    i64 tot = bv.v + off;
                    if (avp_str_eq(bv.name, TOK_A3) || startz(bv.name, "ret@")) {
                        ClassesAccess acc = {tot, width, avp_str(kind), avp_str(load ? "load" : "store"), sign, false,
                                             bv.name};
                        avp_vec_push(&info->access, acc);
                        acc_idx = (int)info->access.n - 1;
                    }
                    if (!load && (eqz(m0, "stw") || eqz(m0, "stwu"))) {
                        int sr = gpr(ops[0]);
                        if (sr >= 0 && R.regs[sr].kind == CLASSES_V_SYM) {
                            ClassesStore st = {(i64)idx, bv.name, tot, R.regs[sr].name, R.regs[sr].v};
                            avp_vec_push(&info->stores, st);
                        }
                    }
                }
                if (endz(m0, "u") || eqz(m0, "psq_lu") || eqz(m0, "psq_stu")) kill(&R, base);
            }
            if (load && strcmp(kind, "int") == 0) {
                int d = gpr(ops[0]);
                if (is_lmw) {
                    if (d >= 0)
                        for (int r = d; r < 32; r++) kill(&R, r);
                } else {
                    kill(&R, d);
                    if (acc_idx >= 0 && d >= 0) {
                        if ((width == 1 || width == 2) && sign != 1) R.pend_sign[d] = acc_idx;
                        if (width == 4) R.loaded[d] = acc_idx;
                    }
                }
            }
            continue;
        }
        bool is_mr = eqz(m0, "mr"), is_or = eqz(m0, "or");
        if ((is_mr || is_or) && nops >= 2 && (is_mr || (nops >= 3 && avp_str_eq(ops[1], ops[2])))) {
            int d = gpr(ops[0]), s = gpr(ops[1]);
            if (d >= 0 && s >= 0) {
                ClassesVal v = R.regs[s];
                kill(&R, d);
                R.regs[d] = v;
                if (R.loaded[s] >= 0) R.loaded[d] = R.loaded[s];
            }
            continue;
        }
        if (eqz(m0, "lis") && nops == 2) {
            int d = gpr(ops[0]);
            kill(&R, d);
            if (endz(ops[1], "@ha")) {
                if (d >= 0) R.regs[d] = (ClassesVal){CLASSES_V_HA, classes_unquote(sub(ops[1], 0, ops[1].n - 3)), 0};
            } else {
                i64 v;
                if (classes_parse_int(ops[1], &v) && d >= 0)
                    R.regs[d] = (ClassesVal){CLASSES_V_IMM, {0}, (i64)(((u64)v << 16) & 0xFFFFFFFFu)};
            }
            continue;
        }
        if (eqz(m0, "li") && nops == 2) {
            int d = gpr(ops[0]);
            kill(&R, d);
            if (endz(ops[1], "@sda21")) {
                if (d >= 0) R.regs[d] = (ClassesVal){CLASSES_V_SDA, classes_unquote(sub(ops[1], 0, ops[1].n - 6)), 0};
            } else {
                i64 v;
                if (classes_parse_int(ops[1], &v) && d >= 0) R.regs[d] = (ClassesVal){CLASSES_V_IMM, {0}, v};
            }
            continue;
        }
        bool is_addi = eqz(m0, "addi"), is_subi = eqz(m0, "subi");
        if ((is_addi || is_subi) && nops == 3) {
            int d = gpr(ops[0]), s = gpr(ops[1]);
            ClassesVal sv = {0};
            if (s >= 0) sv = R.regs[s];
            ClassesVal nv = {0};
            if (endz(ops[2], "@l")) {
                if (sv.kind == CLASSES_V_HA && avp_str_eq(sv.name, classes_unquote(sub(ops[2], 0, ops[2].n - 2))))
                    nv = (ClassesVal){CLASSES_V_SYM, sv.name, 0};
            } else {
                i64 imm;
                if (classes_parse_int(ops[2], &imm)) {
                    if (is_subi) imm = (i64)(0 - (u64)imm);
                    if (sv.kind == CLASSES_V_SYM) nv = (ClassesVal){CLASSES_V_SYM, sv.name, sv.v + imm};
                    else if (sv.kind == CLASSES_V_OBJ && d != s) nv = (ClassesVal){CLASSES_V_OBJ, sv.name, sv.v + imm};
                    else if (sv.kind == CLASSES_V_IMM) nv = (ClassesVal){CLASSES_V_IMM, {0}, sv.v + imm};
                }
            }
            kill(&R, d);
            if (d >= 0) R.regs[d] = nv;
            continue;
        }
        if ((eqz(m0, "extsb") || eqz(m0, "extsh")) && nops == 2) {
            int d = gpr(ops[0]), s = gpr(ops[1]);
            if (s >= 0 && R.pend_sign[s] >= 0) info->access.v[R.pend_sign[s]].sign = 1;
            kill(&R, d);
            continue;
        }
        bool is_bl = eqz(m0, "bl");
        if ((is_bl || eqz(m0, "b")) && nops && !startz(ops[0], ".L")) {
            AvpStr target = classes_unquote(ops[0]);
            if (save_rest(target)) continue; /* the save/restore helpers touch only r11, r12 and the saved registers */
            ClassesCall call = {0};
            call.index = (i64)idx;
            call.addr = in->addr;
            call.target = target;
            for (int r = 3; r <= 10; r++) call.args[r - 3] = R.regs[r];
            call.tail = !is_bl;
            avp_vec_push(&info->calls, call);
            if (startz(target, "__dl__")) info->calls_dl = true;
            if (is_bl) {
                kill_call(&R);
                AvpStr tok = avp_str(avp_sprintf(a, "ret@%08llX", (unsigned long long)in->addr));
                const ClassesVal *a3 = &call.args[0];
                if (returns_this && avp_set_has(returns_this, target) && a3->kind == CLASSES_V_OBJ)
                    R.regs[3] = *a3;
                else
                    R.regs[3] = (ClassesVal){CLASSES_V_OBJ, tok, 0};
                if (startz(target, "__nw__") && a3->kind == CLASSES_V_IMM) {
                    bool found = false;
                    for (size_t k = 0; k < info->nw.n; k++)
                        if (avp_str_eq(info->nw.v[k].tok, tok)) {
                            info->nw.v[k].size = a3->v;
                            found = true;
                        }
                    if (!found) {
                        ClassesNew nw = {tok, a3->v};
                        avp_vec_push(&info->nw, nw);
                    }
                }
            }
            continue;
        }
        if (eqz(m0, "blr")) {
            blrs++;
            if (R.regs[3].kind == CLASSES_V_OBJ && R.regs[3].v == 0 && avp_str_eq(R.regs[3].name, TOK_A3)) this_rets++;
            continue;
        }
        if (eqz(m0, "bctrl") || eqz(m0, "blrl")) {
            kill_call(&R);
            continue;
        }
        if (nops && !in_nodest(m0) && !startz(m0, "b") && !startz(m0, "st")) kill(&R, gpr(ops[0]));
    }
    info->returns_this = blrs > 0 && this_rets == blrs;
    return info;
}

/* ---- Program -------------------------------------------------------------------------------------------------- */

ClassesProgram *classes_program_new(AvpArena *a, AvpMap *funcs, AvpMap *objs, AvpMap *symbols) {
    ClassesProgram *p = AVP_NEW(a, ClassesProgram);
    p->a = a;
    p->funcs = funcs;
    p->objs = objs;
    p->symbols = symbols ? symbols : avp_map_new();
    p->by_addr = avp_imap_new();
    p->info = avp_map_new_cap(avp_map_len(funcs));
    AvpMapIter it = {0};
    while (avp_map_next(funcs, &it)) {
        ClassesFunc *f = it.val->p;
        if (f->has_addr) {
            bool created;
            AvpVal *v = avp_imap_put(p->by_addr, (u64)f->addr, &created);
            if (created) v->p = f;
        }
    }
    memset(&it, 0, sizeof it);
    while (avp_map_next(funcs, &it)) avp_map_put(p->info, it.key, NULL)->p = classes_analyze_function(a, it.val->p, NULL);
    /* second pass: calls to functions returning their r3 (constructors) keep the object; the test compares the
     * operand as written (quoted), analyze_function the unquoted target */
    AvpSet *rt = avp_set_new();
    memset(&it, 0, sizeof it);
    while (avp_map_next(p->info, &it))
        if (((ClassesInfo *)it.val->p)->returns_this) avp_set_add(rt, it.key);
    memset(&it, 0, sizeof it);
    while (avp_map_next(funcs, &it)) {
        ClassesFunc *f = it.val->p;
        bool any = false;
        for (size_t i = 0; i < f->insns.n && !any; i++) {
            const ClassesInsn *in = &f->insns.v[i];
            any = in->nops && eqz(in->mn, "bl") && avp_set_has(rt, in->ops[0]);
        }
        if (any) avp_map_get(p->info, it.key)->p = classes_analyze_function(a, f, rt);
    }
    return p;
}

/* os.walk order: avp_walk is os.walk (top-down; each directory's entries in the order FindFirstFile/FindNextFile
 * return them, which on NTFS is the B-tree's case-insensitive name order); the files are sorted per directory by
 * code point, as from_dir's sorted(files). */
ClassesProgram *classes_program_from_dir(AvpArena *a, const char *asm_dir, const char *symbols_path) {
    AvpMap *funcs = avp_map_new(), *objs = avp_map_new();
    AvpWalk *w = avp_walk_open(asm_dir, true);
    AvpWalkItem item = {0};
    AvpSet *named;
    const char *record = shsm_asm_split_record(a, asm_dir, &named);
    AvpStrVec stale = {0};
    while (w && avp_walk_next(w, &item)) {
        size_t nf = item.files.n;
        AvpStr *files = AVP_NEWN(a, AvpStr, nf ? nf : 1);
        for (size_t i = 0; i < nf; i++) files[i] = avp_str_dup(a, item.files.v[i]);
        avp_sort_strs(files, nf);
        char *dirpath = avp_cz(a, item.dirpath);
        for (size_t i = 0; i < nf; i++) {
            if (!avp_str_endswith(files[i], AVP_S(".s"))) continue;
            char *path = avp_path_join(a, dirpath, avp_cz(a, files[i]));
            char *rel;
            if (!avp_path_relpath(a, path, asm_dir, &rel)) shsm_raise_io();
            AvpStr unit = avp_str(rel);
            unit = avp_str_replace(a, sub(unit, 0, unit.n - 2), AVP_S("\\"), AVP_S("/"), -1);
            if (named) {
                AvpStr listing = avp_str(avp_sprintf(a, "%.*s.s", (int)unit.n, unit.p));
                if (!avp_set_has(named, listing)) {
                    avp_vec_push(&stale, listing);
                    continue;
                }
            }
            AvpStr text;
            if (!avp_read_text(a, path, AVP_TM_UTF8_REPLACE, &text)) shsm_raise_io();
            classes_parse_asm_text(a, text, unit, funcs, objs);
        }
    }
    if (w) avp_walk_close(w);
    AvpMap *syms = NULL;
    if (symbols_path && *symbols_path && avp_exists(symbols_path)) {
        AvpStr text;
        if (!avp_read_text(a, symbols_path, AVP_TM_UTF8, &text)) shsm_raise_io();
        syms = classes_parse_symbols_text(a, text);
    }
    ClassesProgram *p = classes_program_new(a, funcs, objs, syms);
    p->note = shsm_asm_stale_note(a, &stale, record, 10);
    if (named) avp_set_free(named);
    avp_vec_free(&stale);
    return p;
}

ClassesProgram *classes_program_from_texts(AvpArena *a, const AvpStr *units, const AvpStr *texts, size_t n,
                                           AvpStr symbols_text) {
    AvpMap *funcs = avp_map_new(), *objs = avp_map_new();
    for (size_t i = 0; i < n; i++) classes_parse_asm_text(a, texts[i], units[i], funcs, objs);
    return classes_program_new(a, funcs, objs, classes_parse_symbols_text(a, symbols_text));
}

static int obj_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const ClassesObj *a = *(ClassesObj *const *)x, *b = *(ClassesObj *const *)y;
    if (a->addr != b->addr) return a->addr < b->addr ? -1 : 1;
    if (a->data.n != b->data.n) return a->data.n < b->data.n ? -1 : 1;
    return avp_str_cmp(a->name, b->name);
}

AvpStr classes_cstring(ClassesProgram *p, AvpStr sym, i64 addend) {
    if (!p->sorted_built) {
        /* aliases share an address: (address, data length, name) puts the longest image last */
        size_t n = 0, cap = avp_map_len(p->objs);
        p->obj_sorted = AVP_NEWN(p->a, ClassesObj *, cap ? cap : 1);
        AvpMapIter it = {0};
        while (avp_map_next(p->objs, &it)) {
            ClassesObj *o = it.val->p;
            if (o->has_loc && o->has_data) p->obj_sorted[n++] = o;
        }
        avp_sort(p->obj_sorted, n, sizeof(ClassesObj *), obj_cmp, NULL, false);
        p->obj_addrs = AVP_NEWN(p->a, i64, n ? n : 1);
        for (size_t i = 0; i < n; i++) p->obj_addrs[i] = p->obj_sorted[i]->addr;
        p->nsorted = n;
        p->sorted_built = true;
    }
    ClassesObj *o = classes_obj(p, sym);
    i64 base;
    if (o) {
        if (!o->has_loc) return (AvpStr){0};
        base = o->addr;
    } else {
        ClassesSymbol *s = classes_symbol(p, sym);
        if (!s) return (AvpStr){0};
        base = s->addr;
    }
    i64 addr = base + addend;
    /* bisect_right */
    size_t lo = 0, hi = p->nsorted;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (addr < p->obj_addrs[mid]) hi = mid;
        else lo = mid + 1;
    }
    if (lo == 0) return (AvpStr){0};
    ClassesObj *ob = p->obj_sorted[lo - 1];
    i64 k = addr - ob->addr;
    if (k >= (i64)ob->data.n) return (AvpStr){0};
    /* data.find(b"\0", k) with a negative k counts from the end, as a slice does */
    i64 start = k < 0 ? (i64)ob->data.n + k : k;
    if (start < 0) start = 0;
    i64 end = -1;
    for (i64 i = start; i < (i64)ob->data.n; i++)
        if (ob->data.v[i] == 0) {
            end = i;
            break;
        }
    if (end <= k) return (AvpStr){0};
    /* raw = data[k:end] */
    i64 from = start;
    for (i64 i = from; i < end; i++)
        if (ob->data.v[i] < 32 || ob->data.v[i] >= 127) return (AvpStr){0};
    return avp_str_dup(p->a, avp_strn((const char *)ob->data.v + from, (size_t)(end - from)));
}

bool classes_is_func(const ClassesProgram *p, AvpStr name) {
    if (avp_map_has(p->funcs, name)) return true;
    ClassesSymbol *s = classes_symbol(p, name);
    return s && eqz(s->type, "function");
}

AvpStr classes_resolve_thunk(const ClassesProgram *p, AvpStr name, i64 *adjust) {
    ClassesInfo *inf = classes_info(p, name);
    if (inf && inf->has_thunk) {
        *adjust = inf->thunk_adjust;
        return inf->thunk_target;
    }
    *adjust = 0;
    return name;
}

bool classes_faddr(const ClassesProgram *p, AvpStr name, i64 *addr) {
    ClassesFunc *f = classes_func(p, name);
    if (f && f->has_addr) {
        *addr = f->addr;
        return true;
    }
    ClassesSymbol *s = classes_symbol(p, name);
    if (!s) return false;
    *addr = s->addr;
    return true;
}

/* ---- vtables -------------------------------------------------------------------------------------------------- */

bool classes_is_rtti_word(const ClassesItem *w) {
    return w->kind == CLASSES_IT_SYM && w->v == 0 && startz(w->name, "__RTTI__");
}
static bool is_zero(const ClassesItem *w) { return w->kind == CLASSES_IT_INT && w->v == 0; }

bool classes_vtable_prefix(AvpArena *a, const ClassesProgram *p, const ClassesItem *words, size_t len, bool is_whole,
                           ClassesRttiList *rtti, bool sized, ClassesWords *out) {
    if (len < 3 || !is_zero(&words[1]) || (!is_zero(&words[0]) && !classes_is_rtti_word(&words[0]))) return false;
    size_t n = 0, nfun = 0;
    ClassesRttiList found = {0};
    for (size_t k = 0; k < len; k++) {
        const ClassesItem *w = &words[k];
        if (w->kind == CLASSES_IT_INT) {
            if (w->v != 0 && !(k && classes_is_rtti_word(&words[k - 1]))) break;
        } else if (classes_is_rtti_word(w)) {
            if (k + 1 >= len || words[k + 1].kind != CLASSES_IT_INT) break;
            ClassesRtti r = {(i64)k, w->name, words[k + 1].v};
            avp_vec_push(&found, r);
        } else if (!(w->v == 0 && classes_is_func(p, w->name))) {
            break;
        } else {
            nfun++;
        }
        n++;
    }
    if (n < len || !is_whole || found.n) {
        /* a header at the end belongs to the next table; so does a zero word that pads an unsized (dtk-split)
         * -RTTI on table to the next object's alignment */
        bool keep_zero = sized && is_whole && n == len;
        while (n > 2 && ((is_zero(&words[n - 1]) && !keep_zero) || classes_is_rtti_word(&words[n - 1]) ||
                         (words[n - 1].kind == CLASSES_IT_INT && classes_is_rtti_word(&words[n - 2]))))
            n--;
    }
    bool ok = nfun && n >= 3;
    if (ok) {
        if (rtti)
            for (size_t i = 0; i < found.n; i++)
                if ((size_t)found.v[i].k < n) avp_vec_push(rtti, found.v[i]);
        ClassesWords w = {0};
        w.v = AVP_NEWN(a, AvpStr, n);
        w.n = w.cap = n;
        for (size_t k = 0; k < n; k++)
            if (words[k].kind == CLASSES_IT_SYM && !classes_is_rtti_word(&words[k])) w.v[k] = words[k].name;
        *out = w;
    }
    avp_vec_free(&found);
    return ok;
}

static bool data_section(AvpStr s) {
    return !CLASSES_NONE(s) && (eqz(s, ".data") || eqz(s, ".rodata") || eqz(s, ".sdata") || eqz(s, ".sdata2"));
}

AvpMap *classes_find_vtables(AvpArena *a, const ClassesProgram *p, AvpMap *rtti) {
    AvpMap *out = avp_map_new();
    AvpMapIter it = {0};
    while (avp_map_next(p->objs, &it)) {
        ClassesObj *o = it.val->p;
        if (!data_section(o->section)) continue;
        ClassesRttiList r = {0};
        ClassesWords w;
        if (classes_vtable_prefix(a, p, o->items.v, o->items.n, o->pure_words, &r, startz(it.key, "__vt__"), &w)) {
            ClassesWords *wp = AVP_NEW(a, ClassesWords);
            *wp = w;
            avp_map_put(out, it.key, NULL)->p = wp;
            if (rtti && r.n) {
                ClassesRttiList *rp = AVP_NEW(a, ClassesRttiList);
                rp->v = avp_memdup(a, r.v, r.n * sizeof(ClassesRtti));
                rp->n = rp->cap = r.n;
                avp_map_put(rtti, it.key, NULL)->p = rp;
            }
        }
        avp_vec_free(&r);
    }
    return out;
}

ClassesEmbeddedList classes_find_embedded_vtables(AvpArena *a, const ClassesProgram *p, const AvpMap *known) {
    ClassesEmbeddedList out = {0};
    AvpSet *tried = avp_set_new();
    typedef struct {
        AvpStr tok, sym;
        i64 low;
    } Low;
    AVP_VEC(Low) lows = {0};
    AvpMapIter it = {0};
    while (avp_map_next(p->info, &it)) {
        ClassesInfo *inf = it.val->p;
        lows.n = 0;
        for (size_t i = 0; i < inf->stores.n; i++) {
            const ClassesStore *s = &inf->stores.v[i];
            if (avp_map_has(known, s->sym) || !avp_map_has(p->objs, s->sym)) continue;
            size_t k = 0;
            while (k < lows.n && !(avp_str_eq(lows.v[k].tok, s->tok) && avp_str_eq(lows.v[k].sym, s->sym))) k++;
            if (k == lows.n) {
                Low l = {s->tok, s->sym, s->addend};
                avp_vec_push(&lows, l);
            } else if (s->addend < lows.v[k].low) {
                lows.v[k].low = s->addend;
            }
        }
        for (size_t k = 0; k < lows.n; k++) {
            AvpStr sym = lows.v[k].sym;
            i64 base = lows.v[k].low;
            AvpStr key = avp_str(avp_sprintf(a, "%lld:%.*s", (long long)base, AVP_SF(sym)));
            if (avp_set_has(tried, key) || base <= 0 || base % 4) continue;
            avp_set_add(tried, key);
            ClassesObj *o = classes_obj(p, sym);
            if (!data_section(o->section)) continue;
            if ((i64)o->items.n * 4 < base) continue;
            size_t from = (size_t)(base / 4);
            ClassesRttiList r = {0};
            ClassesWords w;
            if (classes_vtable_prefix(a, p, o->items.v + from, o->items.n - from, o->pure_words, &r, false, &w)) {
                ClassesEmbedded e = {0};
                e.sym = sym;
                e.base = base;
                e.words = w;
                if (r.n) {
                    e.has_rtti = true;
                    e.rtti.v = avp_memdup(a, r.v, r.n * sizeof(ClassesRtti));
                    e.rtti.n = e.rtti.cap = r.n;
                }
                avp_vec_push(&out, e);
            }
            avp_vec_free(&r);
        }
    }
    avp_vec_free(&lows);
    avp_set_free(tried);
    return out;
}

AvpStr classes_hx(AvpArena *a, i64 v) {
    if (v < 0) return avp_str(avp_sprintf(a, "-0x%llX", (unsigned long long)(0 - (u64)v)));
    return avp_str(avp_sprintf(a, "0x%llX", (unsigned long long)v));
}
