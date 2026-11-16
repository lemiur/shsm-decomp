# shsm_gdbfake: a fake GDB stub (test fixture)

Port of `FakeStub` in `tools/test_dolphin.py` (lines 38-197). A test fixture, used by `tests/shsm_gdb_test.c` and by
the dolphin tool's tests; not used by any tool at run time. Header `lib/shsm_gdbfake.h`.

A made-up machine behind the GDB remote protocol on 127.0.0.1 (a free port), served by one thread for one connection:
- memory `mem[0x10000]` at 0x80000000 (`m`/`M` outside it reply `E01`/`E02`), `gpr[32]` (`gpr[1]` = 0x8000F000),
  `fpr[32]` (doubles, read as 64-bit hex), `pc` 0x80001000, `lr` 0x80001104; `p` 0x41 is 0x2032, other specials 0;
- Dolphin's quirks: `g` returns the GPRs only; `s` on a breakpoint does not move; `c` from a breakpoint stops again at
  once; an interrupt (0x03) while running is answered with a stop reply (signal 2), an empty packet and a second stop
  reply; `Z`/`z` types 0-4 accepted (0 and 1 kept in the breakpoint set), others answered with an empty packet; `k`
  and `D` end the connection;
- `program[nprogram]` (`ShsmGdbFakeStep`: pc, GPR and FPR updates, optional lr): `c` advances through it until the
  pc hits a breakpoint (stop reply) or it runs out (running, no reply);
- faults: `corrupt` (replies sent with a bad checksum first), `nak` (packets refused once with `-`), `stall_on` (a
  command prefix that gets no reply), `drop_on` (a prefix that closes the connection); `-` resends the last packet.

Usage:

    ShsmGdbFake *f = shsm_gdbfake_new();           /* listening; f->port */
    f->gpr[3] = 0x12345678; f->stall_on = "m";     /* configure before start */
    shsm_gdbfake_start(f);
    ShsmGdbClient *c = shsm_gdb_connect("127.0.0.1", f->port, 2.0, 2.0);
    ...
    CHECK_EQ_U64(shsm_gdbfake_received_count(f, "p40"), 1);   /* stub.received.count("p40") */
    shsm_gdb_client_free(c);
    shsm_gdbfake_free(f);                          /* stops and joins the thread */

`shsm_gdbfake_received_count/_total` and `shsm_gdbfake_bp_count` read the logs under the fixture's lock; read the
machine fields (`mem`, `gpr`, `pc`) only once the client has received the reply that follows a change.
