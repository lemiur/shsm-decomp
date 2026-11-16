/* A fake GDB stub for tests (tools/test_dolphin.py FakeStub): a made-up memory image and register set behind the
 * GDB remote protocol on a local port, served by a thread, with Dolphin's quirks and scripted faults. A test fixture
 * used by the shsm_gdb tests and the dolphin tool's tests; documented in shsm_gdbfake.md.
 *
 *     ShsmGdbFake *f = shsm_gdbfake_new();     // listening on 127.0.0.1:<f->port>
 *     f->gpr[3] = 0x12345678;                  // set up before start
 *     shsm_gdbfake_start(f);
 *     ShsmGdbClient *c = shsm_gdb_connect("127.0.0.1", f->port, 2.0, 2.0);
 *     ...
 *     shsm_gdb_client_free(c);
 *     shsm_gdbfake_free(f);                    // stops and joins the thread
 */
#pragma once
#include "avp/pool.h"
#include "shsm_net.h"

#define SHSM_GDBFAKE_MEM_BASE 0x80000000u
#define SHSM_GDBFAKE_MEM_SIZE 0x10000u

/* One program step: (pc, {gpr index: value}, {fpr index: float}, lr) */
typedef struct {
    u32 pc;
    int ngpr, nfpr;
    struct {
        int index;
        u32 value;
    } gpr[8];
    struct {
        int index;
        double value;
    } fpr[8];
    bool has_lr;
    u32 lr;
} ShsmGdbFakeStep;

typedef struct ShsmGdbFake {
    /* the machine (set before start; read by the test once the client has its reply) */
    u8 mem[SHSM_GDBFAKE_MEM_SIZE];
    u32 gpr[32];             /* gpr[1] = 0x8000F000 */
    double fpr[32];
    u32 pc, lr;              /* 0x80001000, 0x80001104 */
    /* the script and faults (set before start) */
    ShsmGdbFakeStep *program; /* advanced by `c` until a breakpoint; not owned */
    size_t nprogram;
    int corrupt;              /* replies sent with a bad checksum first */
    int nak;                  /* packets refused once with '-' */
    const char *stall_on;     /* command prefix that gets no reply (NULL: none) */
    const char *drop_on;      /* command prefix that closes the connection */
    /* state */
    bool running;
    int port;
    /* private */
    size_t prog_pos;
    AvpMutex lock;
    AvpU32Vec bps;            /* the exec breakpoint set (insertion order) */
    AvpCzVec received;        /* every command payload, "\x03" for an interrupt (heap strings) */
    char *last;               /* the last payload sent (resent on '-') */
    ShsmSocket lsock, conn;
    AvpThread *thread;
    volatile long stop;
} ShsmGdbFake;

ShsmGdbFake *shsm_gdbfake_new(void); /* binds 127.0.0.1 port 0 (aborts the test on failure) */
void shsm_gdbfake_start(ShsmGdbFake *f); /* serves one connection in a thread */
void shsm_gdbfake_free(ShsmGdbFake *f);  /* stops the thread, closes the sockets, frees */
/* stub.received.count(payload) / payload in stub.received */
size_t shsm_gdbfake_received_count(ShsmGdbFake *f, const char *payload);
size_t shsm_gdbfake_received_total(ShsmGdbFake *f);
/* len(stub.bps) */
size_t shsm_gdbfake_bp_count(ShsmGdbFake *f);
