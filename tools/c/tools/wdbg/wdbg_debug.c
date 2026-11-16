/* The debug-API loop of tools/mwcc_trace/wdbg.py, statement for statement.
 * The loop's shape (STATUS_WX86_* codes, WOW64_CONTEXT, int3 / single-step re-arm) follows that project's
 * tools/c/lib/link_trace_debug.c ld_loop (another project of the author's, commit 49e811503d7ba5c9cff92160557df057c50a8ab3); the
 * behaviour (which events are handled, the statuses, the 120 s wait) is the Python's. */
#include "wdbg_debug.h"
#include "wdbg_sym.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define STATUS_WX86_SINGLE_STEP_ 0x4000001EUL
#define STATUS_WX86_BREAKPOINT_ 0x4000001FUL
#define CTX_FLAGS 0x10007UL /* WOW64_CONTEXT_i386 | CONTROL | INTEGER | SEGMENTS, the Python's 0x10007 */

static size_t host_read(void *ctx, u32 addr, void *buf, size_t n) {
    WdbgDebug *d = ctx;
    SIZE_T got = 0;
    ReadProcessMemory(d->process, (LPCVOID)(uintptr_t)addr, buf, n, &got);
    return got;
}

static void host_write(void *ctx, u32 addr, const void *p, size_t n) {
    WdbgDebug *d = ctx;
    SIZE_T got = 0;
    WriteProcessMemory(d->process, (LPVOID)(uintptr_t)addr, p, n, &got);
    FlushInstructionCache(d->process, NULL, 0);
}

void wdbg_debug_out(WdbgDebug *d, AvpStr text) {
    AvpFile *f = d->log;
    avp_fwrite_text(f, text);
    avp_fwrite_text(f, AVP_S("\n"));
    avp_fflush(f);
}

static void host_out(void *ctx, AvpStr text) { wdbg_debug_out(ctx, text); }

/* D.snap: a bytearray of hi-lo bytes, each 4 KiB read from lo in steps assigned to buf[p-lo:p-lo+len(b)] (slice
 * assignment: a read past the end grows the buffer, as the Python's does when hi-lo is not a page multiple). */
static bool host_snap(void *ctx, AvpStr name, i64 lo, i64 hi, const char **err) {
    WdbgDebug *d = ctx;
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    AvpBytes buf = {0};
    if (hi > lo) {
        avp_vec_reserve(&buf, (size_t)(hi - lo));
        buf.n = (size_t)(hi - lo);
        memset(buf.v, 0, buf.n);
    }
    u8 page[0x1000];
    for (i64 p = lo; p < hi; p += 0x1000) {
        size_t got = 0;
        if (p >= 0 && p <= 0xffffffffLL) {
            SIZE_T g = 0;
            ReadProcessMemory(d->process, (LPCVOID)(uintptr_t)p, page, sizeof page, &g);
            got = g;
        }
        size_t start = (size_t)(p - lo), end = start + got;
        if (start > buf.n) start = buf.n;
        if (end > buf.n) end = buf.n;
        /* replace buf[start:end] with page[:got] */
        size_t tail = buf.n - end, nn = start + got + tail;
        avp_vec_reserve(&buf, nn);
        memmove(buf.v + start + got, buf.v + end, tail);
        memcpy(buf.v + start, page, got);
        buf.n = nn;
    }
    const char *path = avp_sprintf(a, "%s/%.*s.bin", d->outdir, AVP_SF(name));
    bool ok = avp_write_file(path, buf.v, buf.n);
    if (!ok) *err = avp_sprintf(avp_arena(), "%s: %.*s", avp_io_err_class(), AVP_SF(avp_io_err_str(a)));
    avp_vec_free(&buf);
    avp_arena_reset(a, m);
    return ok;
}

static AvpStr host_sym(void *ctx, AvpArena *a, i64 addr) {
    WdbgDebug *d = ctx;
    return wdbg_sym(d, a, addr);
}

static bool host_lookup(void *ctx, AvpArena *a, AvpStr name, i64 *addr) {
    return wdbg_sym_lookup(ctx, a, name, addr);
}

WdbgHost wdbg_debug_host(WdbgDebug *d) {
    return (WdbgHost){.ctx = d, .read = host_read, .write = host_write, .out = host_out, .snap = host_snap, .sym = host_sym,
                      .build = d->build, .lookup = host_lookup};
}

typedef struct {
    u32 va;
    u8 byte;
    size_t n; /* 0 when the read at the create-process event failed (the Python's b'') */
} Orig;

static u32 read_u32(WdbgDebug *d, u32 addr) {
    u8 b[4] = {0};
    if (host_read(d, addr, b, 4) < 4) return 0;
    return (u32)b[0] | (u32)b[1] << 8 | (u32)b[2] << 16 | (u32)b[3] << 24;
}

/* The EXCEPTION line and the 32 stack words of wdbg.py's crash branch ("%#x" prints 0x0 for zero, as Python's). */
static void log_crash(WdbgDebug *d, void *h, DWORD ex) {
    WOW64_CONTEXT c;
    memset(&c, 0, sizeof c);
    c.ContextFlags = CTX_FLAGS;
    Wow64GetThreadContext(h, &c);
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    AvpStr sy = wdbg_sym(d, a, c.Eip);
    wdbg_debug_out(d, avp_str(avp_sprintf(a, "EXCEPTION 0x%x eip=0x%x (%.*s) eax=0x%x ebx=0x%x ecx=0x%x edx=0x%x "
                                          "esi=0x%x edi=0x%x esp=0x%x",
                                          (unsigned)ex, (unsigned)c.Eip, AVP_SF(sy), (unsigned)c.Eax, (unsigned)c.Ebx,
                                          (unsigned)c.Ecx, (unsigned)c.Edx, (unsigned)c.Esi, (unsigned)c.Edi,
                                          (unsigned)c.Esp)));
    const char *line = "  stack:";
    for (u32 i = 0; i < 32; i++)
        line = avp_sprintf(a, "%s%s0x%x", line, " ", (unsigned)read_u32(d, c.Esp + 4 * i));
    wdbg_debug_out(d, avp_str(line));
    avp_arena_reset(a, m);
}

void wdbg_debug_loop(WdbgDebug *d, WdbgSpec *spec) {
    size_t nbp = wdbg_spec_bp_count(spec);
    Orig *orig = avp_xcalloc(nbp ? nbp : 1, sizeof *orig);
    bool stepping = false;
    u32 step_bp = 0;
    const u8 cc = 0xCC;
    for (;;) {
        DEBUG_EVENT ev;
        if (!WaitForDebugEvent(&ev, 120000)) {
            wdbg_debug_out(d, AVP_S("timeout"));
            break;
        }
        DWORD status = DBG_CONTINUE;
        if (ev.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) {
            for (size_t i = 0; i < nbp; i++) {
                u32 va = wdbg_spec_bp_addr(spec, i);
                orig[i].va = va;
                orig[i].n = host_read(d, va, &orig[i].byte, 1);
                host_write(d, va, &cc, 1);
            }
        } else if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
            break;
        } else if (ev.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            DWORD ex = ev.u.Exception.ExceptionRecord.ExceptionCode;
            HANDLE h = OpenThread(0x1fffff, FALSE, ev.dwThreadId);
            if (ex == EXCEPTION_BREAKPOINT || ex == STATUS_WX86_BREAKPOINT_) {
                WOW64_CONTEXT c;
                memset(&c, 0, sizeof c);
                c.ContextFlags = CTX_FLAGS;
                Wow64GetThreadContext(h, &c);
                u32 va = c.Eip - 1;
                if (c.Eip != 0 && wdbg_spec_has_bp(spec, va)) {
                    c.Eip = va;
                    u32 *regs[WDBG_NREGS] = {(u32 *)&c.ContextFlags, (u32 *)&c.Dr0, (u32 *)&c.Dr1, (u32 *)&c.Dr2, (u32 *)&c.Dr3,
                                             (u32 *)&c.Dr6, (u32 *)&c.Dr7, (u32 *)&c.SegGs, (u32 *)&c.SegFs, (u32 *)&c.SegEs,
                                             (u32 *)&c.SegDs, (u32 *)&c.Edi, (u32 *)&c.Esi, (u32 *)&c.Ebx, (u32 *)&c.Edx,
                                             (u32 *)&c.Ecx, (u32 *)&c.Eax, (u32 *)&c.Ebp, (u32 *)&c.Eip, (u32 *)&c.SegCs,
                                             (u32 *)&c.EFlags, (u32 *)&c.Esp, (u32 *)&c.SegSs};
                    wdbg_spec_hit(spec, va, regs);
                    for (size_t i = 0; i < nbp; i++)
                        if (orig[i].va == va) host_write(d, va, &orig[i].byte, orig[i].n);
                    c.EFlags |= 0x100;
                    stepping = true;
                    step_bp = va;
                    Wow64SetThreadContext(h, &c);
                }
            } else if (ex == EXCEPTION_SINGLE_STEP || ex == STATUS_WX86_SINGLE_STEP_) {
                if (stepping) {
                    host_write(d, step_bp, &cc, 1);
                    stepping = false;
                }
                WOW64_CONTEXT c;
                memset(&c, 0, sizeof c);
                c.ContextFlags = CTX_FLAGS;
                Wow64GetThreadContext(h, &c);
                c.EFlags &= ~0x100UL;
                Wow64SetThreadContext(h, &c);
            } else {
                /* any other exception (a compiler crash): log where it happened, then pass it on */
                log_crash(d, h, ex);
                status = DBG_EXCEPTION_NOT_HANDLED;
            }
            if (h) CloseHandle(h);
        }
        ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, status);
    }
    free(orig);
}
