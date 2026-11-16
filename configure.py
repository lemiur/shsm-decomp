#!/usr/bin/env python3

###
# The project table: compiler flags, libraries and the object matching status.
# Read (interpreted, never run by Python) by build/tools/configure.exe, which
# generates build.ninja and the related files.
#
# Usage:
#   build/tools/configure.exe
#   tools/bin/ninja.exe
#
# No longer runnable with Python: tools/project.py was retired with the other
# Python tools. Build the C tools with tools\build_c.cmd.
###

import argparse
import sys
from pathlib import Path
from typing import Any, Dict, List

from tools.project import (
    Object,
    ProgressCategory,
    ProjectConfig,
    calculate_progress,
    generate_build,
    is_windows,
)

# Game versions
DEFAULT_VERSION = 0
VERSIONS = [
    "R5WEA4",  # 0: Silent Hill: Shattered Memories (Wii, USA)
]

parser = argparse.ArgumentParser()
parser.add_argument(
    "mode",
    choices=["configure", "progress"],
    default="configure",
    help="script mode (default: configure)",
    nargs="?",
)
parser.add_argument(
    "-v",
    "--version",
    choices=VERSIONS,
    type=str.upper,
    default=VERSIONS[DEFAULT_VERSION],
    help="version to build",
)
parser.add_argument(
    "--build-dir",
    metavar="DIR",
    type=Path,
    default=Path("build"),
    help="base build directory (default: build)",
)
parser.add_argument(
    "--binutils",
    metavar="BINARY",
    type=Path,
    help="path to binutils (optional)",
)
parser.add_argument(
    "--compilers",
    metavar="DIR",
    type=Path,
    help="path to compilers (optional)",
)
parser.add_argument(
    "--map",
    action="store_true",
    help="generate map file(s)",
)
parser.add_argument(
    "--debug",
    action="store_true",
    help="build with debug info (non-matching)",
)
if not is_windows():
    parser.add_argument(
        "--wrapper",
        metavar="BINARY",
        type=Path,
        help="path to wibo or wine (optional)",
    )
parser.add_argument(
    "--dtk",
    metavar="BINARY | DIR",
    type=Path,
    help="path to decomp-toolkit binary or source (optional)",
)
parser.add_argument(
    "--objdiff",
    metavar="BINARY | DIR",
    type=Path,
    help="path to objdiff-cli binary or source (optional)",
)
parser.add_argument(
    "--sjiswrap",
    metavar="EXE",
    type=Path,
    help="path to sjiswrap.exe (optional)",
)
parser.add_argument(
    "--ninja",
    metavar="BINARY",
    type=Path,
    help="path to ninja binary (optional)",
)
parser.add_argument(
    "--verbose",
    action="store_true",
    help="print verbose output",
)
parser.add_argument(
    "--non-matching",
    dest="non_matching",
    action="store_true",
    help="builds equivalent (but non-matching) or modded objects",
)
parser.add_argument(
    "--warn",
    dest="warn",
    type=str,
    choices=["all", "off", "error"],
    help="how to handle warnings",
)
parser.add_argument(
    "--no-progress",
    dest="progress",
    action="store_false",
    help="disable progress calculation",
)
parser.add_argument(
    "--objcache",
    action=argparse.BooleanOptionalAction,
    default=False,
    help="serve MWCC compiles from the shared object cache (build/tools/objcache.exe)",
)
args = parser.parse_args()

# Prefer locally pinned tools (see docs/toolchain.md) over downloads.
# tools/bin/ holds the release binaries, tools/compilers/ the mwcceppc versions.
_LOCAL_TOOLS = {
    "dtk": Path("tools/bin/dtk.exe"),
    "objdiff": Path("tools/bin/objdiff-cli.exe"),
    "ninja": Path("tools/bin/ninja.exe"),
    "compilers": Path("tools/compilers"),
}
for _name, _path in _LOCAL_TOOLS.items():
    if getattr(args, _name) is None and _path.exists():
        setattr(args, _name, _path)

config = ProjectConfig()
config.version = str(args.version)
version_num = VERSIONS.index(config.version)

# Apply arguments
config.build_dir = args.build_dir
config.dtk_path = args.dtk
config.objdiff_path = args.objdiff
config.binutils_path = args.binutils
config.compilers_path = args.compilers
config.generate_map = args.map
config.non_matching = args.non_matching
config.sjiswrap_path = args.sjiswrap
config.ninja_path = args.ninja
config.progress = args.progress
config.objcache = args.objcache
if not is_windows():
    config.wrapper = args.wrapper
# Don't build asm unless we're --non-matching
if not config.non_matching:
    config.asm_dir = None

# Tool versions
config.binutils_tag = "2.42-2"
config.compilers_tag = "20251118"
config.dtk_tag = "v1.8.4"
config.objdiff_tag = "v3.8.2"
config.sjiswrap_tag = "v1.2.2"
config.wibo_tag = "1.0.3"

# Project
config.config_path = Path("config") / config.version / "config.yml"
config.check_sha_path = Path("config") / config.version / "build.sha1"
config.asflags = [
    "-mgekko",
    "--strip-local-absolute",
    "-I include",
    f"-I build/{config.version}/include",
    f"--defsym BUILD_VERSION={version_num}",
]
config.ldflags = [
    "-fp hardware",
    "-nodefaults",
]
if args.debug:
    config.ldflags.append("-gdwarf-2")
if args.map:
    config.ldflags.append("-mapunused")
    config.ldflags.append("-listclosure")

# Use for any additional files that should cause a re-configure when modified
config.reconfig_deps = []

# Optional numeric ID for decomp.me preset
# Can be overridden in libraries or objects
config.scratch_preset_id = None

# Base flags, common to most GC/Wii games. Confirmed for the RVL SDK by
# matched units (see docs/toolchain.md, per-library profile).
cflags_base = [
    "-nodefaults",
    "-proc gekko",
    "-align powerpc",
    "-enum int",
    "-fp hardware",
    "-Cpp_exceptions off",
    "-O4,p",
    "-inline auto",
    '-pragma "cats off"',
    '-pragma "warn_notinlined off"',
    "-maxerrors 1",
    "-nosyspath",
    "-RTTI off",
    "-fp_contract on",
    "-str reuse",
    "-multibyte",
    "-i include",
    f"-i build/{config.version}/include",
    f"-DBUILD_VERSION={version_num}",
    f"-DVERSION_{config.version}",
]

# Debug flags
if args.debug:
    cflags_base.extend(["-sym dwarf-2", "-DDEBUG=1"])
else:
    cflags_base.append("-DNDEBUG=1")

# Warning flags
if args.warn == "all":
    cflags_base.append("-W all")
elif args.warn == "off":
    cflags_base.append("-W off")
elif args.warn == "error":
    cflags_base.append("-W error")

# Metrowerks runtime and MSL C flags (confirmed by matched units): packed
# 4-byte function alignment and stmw/lmw register saves.
cflags_runtime = [
    *cflags_base,
    "-use_lmw_stmw on",
    "-str reuse,pool,readonly",
    "-gccinc",
    "-common off",
    "-inline auto",
    "-func_align 4",
]

# MSL C: the runtime flags with unpooled read-only strings. Retail keeps MSL's small
# string literals (strtok's "", ".", "AM|PM", "%T", "NAN(") as separate objects in .sdata2;
# -str ...,readonly pools them into .rodata @stringBase0.
cflags_msl = [
    *cflags_base,
    "-use_lmw_stmw on",
    "-str reuse",
    '-pragma "readonly_strings on"',
    "-gccinc",
    "-common off",
    "-inline auto",
    "-ipa file",
    "-func_align 4",
    # fdlibm (e_*/k_*/s_* units) has no fmadd/fmsub in retail against about 300 fmul.
    "-fp_contract off",
]

# MetroTRK (unit boundaries inferred from the call graph and the public TRK_MINNOW_DOLPHIN
# layout; flags are the runtime flags with pooled, writable strings, unconfirmed).
cflags_trk = [
    *cflags_base,
    "-use_lmw_stmw on",
    "-str reuse,pool",
    "-gccinc",
    "-common off",
    # -pool off: uninitialised statics are laid out in reverse declaration order instead of first-use order
    # (gRecvBuf before gRecvCB in EXI2_GDEV_GCN/main.c); every other MetroTRK unit is unchanged by it.
    "-pool off",
    "-inline auto",
    "-func_align 4",
]

# RVL SDK banner: "Feb 27 2009 (0x4302_145)" -> CodeWarrior for Wii 1.0 (4.3 build 145).
config.linker_version = "Wii/1.0"


# GX is built without fused multiply-add, as zeldaret/tp and doldecomp/ogws build the whole RVL SDK.
# Retail GX has no fmadds/fmsubs; no unit matches differently with the base -fp_contract on.
cflags_gx = ["-fp_contract off"]


# Helper function for RVL SDK libraries. "-ipa file": __OSInterruptInit (OSInterrupt) needs it, and every
# SDK unit matches with it.
def RVLSDKLib(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "Wii/1.0",
        "cflags": [*cflags_base, "-ipa file"],
        "progress_category": "sdk",
        "objects": objects,
    }


# Broadcom BTE Bluetooth stack (RVL SDK). The protocol stack (bta, btm, btu, l2cap, sdp, rfcomm, hid, gap,
# hcicmds, bd, utl, ...) is built with GC/3.0a5 (packed at 4 bytes; Wii/1.0 fails 330 of its 727 functions,
# GC/3.0a5.2 fails WBT_ExtCreateRecord and six more); the Wii glue (gki_*, hcisu_h2, uusb_ppc, bte_hcisu,
# bte_init, bte_logmsg, bte_main, btu_task1) with Wii/1.0 (16-byte aligned; GC fails 27 of the 74 functions in eight of them).
# Same split as ThePlayerRolo/sdk_2009-12-11 and yannicksuter/mscharged-decomp (R4QK01).
def BTELib(objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": "bte",
        "mw_version": "GC/3.0a5",
        "cflags": [*cflags_base, "-ipa file", "-fp_contract off"],
        "progress_category": "sdk",
        "objects": objects,
    }


# Helper function for third-party middleware (FMOD, RenderWare, libjpeg)
def MiddlewareLib(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "Wii/1.0",
        "cflags": cflags_base,
        "progress_category": "middleware",
        "objects": objects,
    }


# IJG libjpeg 6b (decoder half), built with the game's flags rather than the
# SDK's: -O3 (-O4 merges adjacent case tests into a range compare that retail
# lacks), no fused multiply-adds (jidctflt), functions packed at 4 bytes, stmw/lmw register saves, strings in .data,
# -ipa file (jdinput consume_markers needs it; every other unit scores the same with or without it).
cflags_jpeg = [
    *cflags_base,
    "-O3",
    "-fp_contract off",
    "-use_lmw_stmw on",
    "-func_align 4",
    "-ipa file",
    "-i include/libjpeg",
]


# Helper function for IJG libjpeg
def JPEGLib(objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": "libjpeg",
        "mw_version": "Wii/1.0",
        "cflags": cflags_jpeg,
        "progress_category": "middleware",
        "objects": objects,
    }

# Climax engine and game code (game/PadRumble.cpp matches): functions packed at 4 bytes;
# no fused multiply-adds anywhere in the game ranges (3,595 fmuls, no fmadds/fmsubs); most
# game functions that save five or more GPRs and no FPRs use stmw (259) rather than
# _savegpr (135), so -use_lmw_stmw on is the default and units that save with _savegpr
# need a per-object override. -str noreuse: retail keeps a separate copy of a literal for
# each use (four "RumbleStartData*" in one pad-manager function). -O3 and -ipa file: each
# improves EventHandler and changes no other game function (docs/exceptions.md, game-2).
cflags_game = [
    *cflags_base,
    "-func_align 4",
    "-fp_contract off",
    "-use_lmw_stmw on",
    "-str noreuse",
    "-O3",
    "-ipa file",
]


# Helper function for Climax engine and game code
# zlib 1.2.3, inflate side only (adler32, crc32, inffast, inflate, inftrees, zutil; the game never
# deflates), unmodified upstream source built with the game's own flags: -O3 (-O4,p breaks inflate and
# inflate_table), -str noreuse (inflate's duplicated messages, e.g. "unknown compression method", are
# separate copies), -use_lmw_stmw on (stmw saves in inflate, inftrees, inffast). -ipa file and
# -fp_contract off change nothing here (no float code).
cflags_zlib = [
    *cflags_game,
    "-i include/zlib",
]


# Helper function for zlib
def ZlibLib(objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": "zlib",
        "mw_version": "Wii/1.0",
        "cflags": cflags_zlib,
        "progress_category": "middleware",
        "objects": objects,
    }


# Home Button Menu (HBM) and its nw4hbm lyt/ut subset, C++. Banner "Mar 3 2009 (0x4201_142)": CodeWarrior
# 4.2 build 142, i.e. Wii/1.0a (Wii/1.0 is 4.3 build 145). xbret/xenoblade builds this same drop (JP, HBM
# 20090303) with Wii/1.0a and these flags; small data off (HBM has no .sdata/.sbss/.sdata2), no fused
# multiply-add, RTTI on (docs/exceptions.md, "HBM rulings (sdk-hbm-1)").
cflags_hbm = [
    *cflags_base,
    "-ipa file",
    "-fp_contract off",
    "-sdata 0",
    "-sdata2 0",
    "-RTTI on",
    "-i include/revolution/hbm",
]


# RenderWare Graphics 3.7 (C), PROVISIONAL profile: the RW range (0x80290F00-0x802F5000) packs functions at
# 4 bytes, has no fused multiply-adds against 2,292 fmuls, and saves GPRs with stmw (198) except where FPRs are
# saved too (_savegpr 67), as the game code does; the remaining flags are confirmed per unit (docs/exceptions.md,
# "RenderWare rulings (rw-pilot)").
cflags_rw = [
    *cflags_base,
    "-func_align 4",
    "-fp_contract off",
    "-use_lmw_stmw on",
    "-str noreuse",
    "-O3",
    "-ipa file",
]


# Helper function for RenderWare Graphics 3.7
def RenderWareLib(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "Wii/1.0",
        "cflags": cflags_rw,
        "progress_category": "middleware",
        "objects": objects,
    }


# FMOD Ex 4.x (C++), PROVISIONAL profile from the retail range (0x8032BA54-0x803B4C10) and the first
# unit (fmod/fmod.cpp): functions packed at 4 bytes (1,881 functions, 451 on 16-byte boundaries); no fused
# multiply-adds against 1,507 fmuls; GPR saves with stmw (227) except _savegpr (97, 53 of them in large
# functions that save no FPRs); strings in .data, one copy per use ("fmod.cpp" twice in FMOD_System_Create:
# -str noreuse); null RTTI words in the vtables (-RTTI off). docs/exceptions.md, "FMOD rulings (fmod-1)".
cflags_fmod = [
    *cflags_base,
    "-func_align 4",
    "-fp_contract off",
    "-use_lmw_stmw on",
    "-str noreuse",
    "-O3",
    "-ipa file",
    "-i include/fmod",
]


# Helper function for FMOD Ex. Compiler: Wii/1.0a (4.2 build 142), not Wii/1.0: MusicSettings::MusicSettings
# (fmod_musicsystemi) keeps a loop-invariant aggregate copy inside its loop, which only the 4.2 builds
# (0x4201_127, 1.0RC1, 1.0a) do; Wii/1.0 and later hoist it out, as Gormiti's build of the same source shows.
# Every other FMOD unit compiles identically under Wii/1.0 and Wii/1.0a; 0x4201_127 breaks fmod_time.
def FMODLib(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "Wii/1.0a",
        "cflags": cflags_fmod,
        "progress_category": "middleware",
        "objects": objects,
    }


# Helper function for the Home Button Menu library
def HBMLib(objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": "hbm",
        "mw_version": "Wii/1.0a",
        "cflags": cflags_hbm,
        "progress_category": "sdk",
        "objects": objects,
    }


def GameLib(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "Wii/1.0",
        "cflags": cflags_game,
        "progress_category": "game",
        "objects": objects,
    }


Matching = True                   # Object matches and should be linked
NonMatching = False               # Object does not match and should not be linked
Equivalent = config.non_matching  # Object should be linked when configured with --non-matching


# Object is only matching for specific versions
def MatchingFor(*versions):
    return config.version in versions


config.warn_missing_config = True
config.warn_missing_source = False
# Everything not listed here is linked from dtk's asm objects (auto_* units).
# To add a unit: add it to config/R5WEA4/splits.txt and list it here, e.g.
#   RVLSDKLib("os", [Object(NonMatching, "revolution/os/OSCache.c")]),
# Flip NonMatching -> Matching only when the unit is byte-identical.
config.libs = [
    {
        # Split early because the Wii linker requires these two runtime units
        # to carry the .ctors$10/.dtors$10/.dtors$15 references.
        "lib": "Runtime.PPCEABI.H",
        "mw_version": config.linker_version,
        "cflags": cflags_runtime,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "Runtime.PPCEABI.H/__mem.c"),
            Object(Matching, "Runtime.PPCEABI.H/__va_arg.c"),
            Object(Matching, "Runtime.PPCEABI.H/global_destructor_chain.c"),
            Object(
                Matching,
                "Runtime.PPCEABI.H/NMWException.cp",
                extra_cflags=["-Cpp_exceptions on"],
            ),
            Object(Matching, "Runtime.PPCEABI.H/ptmf.c"),
            Object(Matching, "Runtime.PPCEABI.H/runtime.c"),
            Object(Matching, "Runtime.PPCEABI.H/__init_cpp_exceptions.cpp"),
            # __register_fragment: 'li r6' is scheduled before 'addi r5' (retail has them the other way round).
            Object(NonMatching, "Runtime.PPCEABI.H/Gecko_ExceptionPPC.cp"),
        ],
    },
    RVLSDKLib(
        "NdevExi2AD",
        [
            # Built unoptimised (-O0 -inline off: spilled parameters, no inlining of the ODEMU/EXI helpers, frame
            # pointer for block locals) with GC/3.0a3..3.0a5: Wii/1.0 and GC/3.0a5.2 count each local's declaration
            # as a use when -O0 ranks variables for registers, which reorders DBWrite's and __EXI2Imm's registers.
            Object(Matching, "revolution/NdevExi2AD/DebuggerDriver.c", mw_version="GC/3.0a5", extra_cflags=["-O0", "-inline off", "-func_align 4"]),
            Object(Matching, "revolution/NdevExi2AD/exi2.c", mw_version="GC/3.0a5", extra_cflags=["-O0", "-inline off", "-func_align 4"]),
        ],
    ),
    {
        "lib": "MetroTRK",
        "mw_version": config.linker_version,
        "cflags": cflags_trk,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "MetroTRK/Processor/ppc/Generic/__exception.s"),
            Object(Matching, "MetroTRK/Processor/ppc/Export/targsupp.c"),
            Object(Matching, "MetroTRK/gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main.c"),
            Object(Matching, "MetroTRK/gamedev/cust_connection/utils/gc/MWCriticalSection_gc.c"),
            Object(Matching, "MetroTRK/gamedev/cust_connection/utils/common/CircleBuffer.c"),
            Object(Matching, "MetroTRK/Processor/ppc/Generic/flush_cache.c"),
            Object(Matching, "MetroTRK/Portable/main_TRK.c"),
            Object(Matching, "MetroTRK/Portable/mainloop.c"),
            Object(Matching, "MetroTRK/Portable/mem_TRK.c"),
            Object(Matching, "MetroTRK/Portable/dispatch.c"),
            Object(Matching, "MetroTRK/Os/dolphin/dolphin_trk.c"),
            Object(Matching, "MetroTRK/Os/dolphin/dolphin_trk_glue.c"),
            Object(Matching, "MetroTRK/Portable/notify.c"),
            Object(Matching, "MetroTRK/Portable/nubevent.c"),
            Object(Matching, "MetroTRK/Portable/nubinit.c"),
            Object(Matching, "MetroTRK/Portable/serpoll.c"),
            Object(Matching, "MetroTRK/Portable/string_TRK.c"),
            Object(Matching, "MetroTRK/Portable/support.c"),
            Object(Matching, "MetroTRK/Os/dolphin/targcont.c"),
            Object(Matching, "MetroTRK/Processor/ppc/Generic/mpc_7xx_603e.c"),
            Object(Matching, "MetroTRK/Portable/msg.c"),
            Object(Matching, "MetroTRK/Portable/msgbuf.c"),
            Object(Matching, "MetroTRK/Portable/msghndlr.c"),
            Object(Matching, "MetroTRK/Export/mslsupp.c"),
            Object(NonMatching, "MetroTRK/Processor/ppc/Generic/targimpl.c"),
            Object(Matching, "MetroTRK/Os/dolphin/target_options.c"),
        ],
    },
    {
        "lib": "MSL_C",
        "mw_version": config.linker_version,
        "cflags": cflags_msl,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "MSL_C/MSL_Common/Src/alloc.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/ansi_files.c"),
            # ansi_fp.c: __two_exp's digit strings are one pooled .rodata block (-str pool), unlike string.c.
            Object(Matching, "MSL_C/MSL_Common/Src/ansi_fp.c", extra_cflags=["-str reuse,pool,readonly"]),
            Object(Matching, "MSL_C/MSL_Common/Src/abs.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/buffer_io.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/direct_io.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/file_io.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/FILE_POS.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/mbstring.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/mem.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/mem_funcs.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/fpclassify.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/misc_io.c"),
            # printf.c: the infinity/NaN spellings and "" are one pooled .rodata block (-str pool), as ansi_fp.c.
            Object(Matching, "MSL_C/MSL_Common/Src/printf.c", extra_cflags=["-str reuse,pool,readonly"]),
            Object(Matching, "MSL_C/MSL_Common/Src/qsort.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/rand.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/scanf.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/string.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/strtold.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/float.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/strtoul.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/wmem.c"),
            # wprintf.c: the wide spellings and L"" are one pooled .data @wstringBase0, "" a pooled .rodata @stringBase0.
            Object(Matching, "MSL_C/MSL_Common/Src/wprintf.c", extra_cflags=["-str reuse,pool,readonly"]),
            Object(Matching, "MSL_C/MSL_Common/Src/wstring.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/wchar_io.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/environment.c"),
            Object(Matching, "MSL_C/PPC_EABI/Src/uart_console_io_gcn.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/secure_error.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Src/math_sun.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_acos.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_asin.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_atan2.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_exp.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_fmod.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_log.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_log10.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_pow.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_rem_pio2.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/k_cos.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/k_rem_pio2.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/k_sin.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/k_tan.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_atan.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_ceil.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_copysign.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_cos.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_floor.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_frexp.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_ldexp.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_sin.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_tan.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_acos.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_asin.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_atan2.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_exp.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_fmod.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_log.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_log10.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_pow.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_sqrt.c"),
            Object(Matching, "MSL_C/PPC_EABI/Src/math_ppc.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_sqrt.c"),
            Object(Matching, "MSL_C/MSL_Common/Src/extras.c"),
        ],
    },
    RVLSDKLib(
        "thp",
        [
            Object(Matching, "revolution/thp/THPDec.c"),
            Object(Matching, "revolution/thp/THPAudio.c"),
        ],
    ),
    RVLSDKLib(
        "exi",
        [
            # EXIBios was built without instruction scheduling (parameter moves and loads stay in source order);
            # EXIUart and EXICommon match with scheduling on.
            Object(Matching, "revolution/exi/EXIBios.c", extra_cflags=["-schedule off"]),
            Object(Matching, "revolution/exi/EXIUart.c"),
            Object(Matching, "revolution/exi/EXICommon.c"),
        ],
    ),
    RVLSDKLib(
        "si",
        [
            Object(Matching, "revolution/si/SIBios.c"),
            Object(Matching, "revolution/si/SISamplingRate.c"),
        ],
    ),
    RVLSDKLib(
        "db",
        [
            Object(Matching, "revolution/db/db.c"),
        ],
    ),
    RVLSDKLib(
        "dvd",
        [
            Object(Matching, "revolution/dvd/dvdfs.c"),
            Object(NonMatching, "revolution/dvd/dvd.c"),
            Object(Matching, "revolution/dvd/dvdqueue.c"),
            Object(Matching, "revolution/dvd/dvderror.c"),
            Object(Matching, "revolution/dvd/dvdidutils.c"),
            Object(Matching, "revolution/dvd/dvdFatal.c"),
            Object(Matching, "revolution/dvd/dvdDeviceError.c"),
            Object(Matching, "revolution/dvd/dvd_broadway.c"),
        ],
    ),
    RVLSDKLib(
        "base",
        [
            Object(Matching, "revolution/base/PPCArch.c"),
        ],
    ),
    RVLSDKLib(
        "os",
        [
            Object(Matching, "revolution/os/OS.c"),
            Object(Matching, "revolution/os/OSAlarm.c"),
            Object(Matching, "revolution/os/OSAlloc.c"),
            Object(Matching, "revolution/os/OSArena.c"),
            Object(Matching, "revolution/os/OSAudioSystem.c"),
            Object(Matching, "revolution/os/OSCache.c"),
            Object(Matching, "revolution/os/OSContext.c"),
            Object(Matching, "revolution/os/OSError.c"),
            Object(Matching, "revolution/os/OSExec.c"),
            Object(Matching, "revolution/os/OSFatal.c", extra_cflags=["-fp_contract off"]),
            Object(Matching, "revolution/os/OSFont.c"),
            Object(Matching, "revolution/os/OSInterrupt.c"),
            Object(Matching, "revolution/os/OSLink.c"),
            Object(Matching, "revolution/os/OSMessage.c"),
            Object(Matching, "revolution/os/OSMemory.c"),
            Object(Matching, "revolution/os/OSMutex.c"),
            Object(Matching, "revolution/os/OSReboot.c"),
            Object(Matching, "revolution/os/OSReset.c"),
            Object(Matching, "revolution/os/OSRtc.c"),
            # OSWaitSemaphore 89%: retail reloads sem->count for the decrement after the wait loop.
            Object(Matching, "revolution/os/OSSemaphore.c"),
            Object(Matching, "revolution/os/OSSync.c"),
            Object(Matching, "revolution/os/OSThread.c"),
            Object(Matching, "revolution/os/OSTime.c"),
            Object(Matching, "revolution/os/OSUtf.c"),
            Object(Matching, "revolution/os/OSIpc.c"),
            Object(Matching, "revolution/os/OSStateTM.c"),
            Object(Matching, "revolution/os/__start.c"),
            Object(Matching, "revolution/os/OSPlayRecord.c"),
            Object(Matching, "revolution/os/OSStateFlags.c"),
            # OSNet.c: only functions marked inline are expanded (-inline noauto); the NWC24 entry points are out-of-line in retail.
            Object(NonMatching, "revolution/os/OSNet.c", extra_cflags=["-inline noauto"]),
            Object(Matching, "revolution/os/OSNandbootInfo.c"),
            Object(Matching, "revolution/os/OSPlayTime.c"),
            Object(Matching, "revolution/os/OSLaunch.c"),
            Object(Matching, "revolution/os/__ppc_eabi_init.c"),
        ],
    ),
    RVLSDKLib(
        "pad",
        [
            Object(Matching, "revolution/pad/Pad.c"),
        ],
    ),
    RVLSDKLib(
        "ai",
        [
            Object(Matching, "revolution/ai/ai.c"),
        ],
    ),
    RVLSDKLib(
        "ax",
        [
            Object(Matching, "revolution/ax/AX.c"),
            Object(Matching, "revolution/ax/AXAlloc.c"),
            Object(Matching, "revolution/ax/AXAux.c"),
            Object(Matching, "revolution/ax/AXCL.c"),
            Object(Matching, "revolution/ax/AXOut.c"),
            Object(Matching, "revolution/ax/AXSPB.c"),
            Object(Matching, "revolution/ax/AXVPB.c", extra_cflags=["-fp_contract off"]),
            Object(Matching, "revolution/ax/AXProf.c"),
            Object(Matching, "revolution/ax/AXComp.c"),
            Object(Matching, "revolution/ax/DSPCode.c"),
        ],
    ),
    RVLSDKLib(
        "axfx",
        [
            Object(Matching, "revolution/axfx/AXFXReverbHi.c"),
            Object(Matching, "revolution/axfx/AXFXReverbHiDpl2.c"),
            Object(Matching, "revolution/axfx/AXFXReverbHiExp.c", extra_cflags=["-fp_contract off"]),
            Object(Matching, "revolution/axfx/AXFXReverbHiExpDpl2.c", extra_cflags=["-fp_contract off"]),
            Object(Matching, "revolution/axfx/AXFXReverbStd.c"),
            Object(Matching, "revolution/axfx/AXFXReverbStdExp.c", extra_cflags=["-fp_contract off"]),
            Object(Matching, "revolution/axfx/AXFXHooks.c"),
        ],
    ),
    RVLSDKLib(
        "mix",
        [
            Object(Matching, "revolution/mix/mix.c"),
            Object(Matching, "revolution/mix/mix_rmt.c"),
        ],
    ),
    RVLSDKLib(
        "dsp",
        [
            Object(Matching, "revolution/dsp/dsp.c"),
            Object(Matching, "revolution/dsp/dsp_debug.c"),
            Object(Matching, "revolution/dsp/dsp_task.c"),
        ],
    ),
    RVLSDKLib(
        "gx",
        [
            Object(Matching, "revolution/gx/GXInit.c", extra_cflags=cflags_gx),
            Object(Matching, "revolution/gx/GXFifo.c", extra_cflags=cflags_gx),
            Object(Matching, "revolution/gx/GXAttr.c", extra_cflags=cflags_gx),
            Object(Matching, "revolution/gx/GXMisc.c", extra_cflags=cflags_gx),
            Object(Matching, "revolution/gx/GXGeometry.c", extra_cflags=cflags_gx),
            Object(Matching, "revolution/gx/GXFrameBuf.c", extra_cflags=cflags_gx),
            Object(Matching, "revolution/gx/GXLight.c", extra_cflags=cflags_gx),
            Object(Matching, "revolution/gx/GXTexture.c", extra_cflags=cflags_gx),
            Object(Matching, "revolution/gx/GXBump.c", extra_cflags=cflags_gx),
            Object(Matching, "revolution/gx/GXTev.c", extra_cflags=cflags_gx),
            Object(Matching, "revolution/gx/GXPixel.c", extra_cflags=cflags_gx),
            Object(Matching, "revolution/gx/GXDisplayList.c", extra_cflags=cflags_gx),
            Object(Matching, "revolution/gx/GXTransform.c", extra_cflags=cflags_gx),
            Object(Matching, "revolution/gx/GXPerf.c", extra_cflags=cflags_gx),
        ],
    ),
    RVLSDKLib(
        "mem",
        [
            Object(Matching, "revolution/mem/mem_heapCommon.c"),
            Object(Matching, "revolution/mem/mem_expHeap.c"),
            Object(Matching, "revolution/mem/mem_allocator.c"),
            Object(Matching, "revolution/mem/mem_list.c"),
        ],
    ),
    RVLSDKLib(
        "euart",
        [
            Object(Matching, "revolution/euart/euart.c"),
        ],
    ),
    RVLSDKLib(
        "fs",
        [
            Object(Matching, "revolution/fs/fs.c"),
        ],
    ),
    RVLSDKLib(
        "ipc",
        [
            Object(Matching, "revolution/ipc/ipcMain.c"),
            Object(Matching, "revolution/ipc/ipcclt.c"),
            Object(Matching, "revolution/ipc/memory.c"),
            Object(Matching, "revolution/ipc/ipcProfile.c"),
        ],
    ),
    RVLSDKLib(
        "nand",
        [
            Object(Matching, "revolution/nand/nand.c"),
            Object(Matching, "revolution/nand/NANDOpenClose.c"),
            Object(Matching, "revolution/nand/NANDCore.c"),
            Object(Matching, "revolution/nand/NANDCheck.c"),
            Object(Matching, "revolution/nand/NANDLogging.c"),
        ],
    ),
    RVLSDKLib(
        "sc",
        [
            Object(Matching, "revolution/sc/scsystem.c"),
            Object(Matching, "revolution/sc/scapi.c"),
            Object(Matching, "revolution/sc/scapi_prdinfo.c"),
        ],
    ),
    RVLSDKLib(
        "esp",
        [
            Object(Matching, "revolution/esp/esp.c"),
        ],
    ),
    RVLSDKLib(
        "arc",
        [
            Object(Matching, "revolution/arc/arc.c"),
        ],
    ),
    BTELib(
        [
            Object(Matching, "revolution/bte/bta_dm_cfg.c"),
            Object(Matching, "revolution/bte/bta_hh_cfg.c"),
            Object(Matching, "revolution/bte/bta_sys_cfg.c"),
            Object(Matching, "revolution/bte/gki_buffer.c", mw_version="Wii/1.0"),
            Object(Matching, "revolution/bte/gki_time.c", mw_version="Wii/1.0"),
            Object(Matching, "revolution/bte/gki_ppc.c", mw_version="Wii/1.0"),
            Object(Matching, "revolution/bte/hcisu_h2.c", mw_version="Wii/1.0"),
            Object(Matching, "revolution/bte/uusb_ppc.c", mw_version="Wii/1.0"),
            Object(Matching, "revolution/bte/bte_hcisu.c", mw_version="Wii/1.0"),
            Object(Matching, "revolution/bte/bte_init.c", mw_version="Wii/1.0"),
            Object(Matching, "revolution/bte/bte_logmsg.c", mw_version="Wii/1.0"),
            Object(Matching, "revolution/bte/bte_main.c", mw_version="Wii/1.0"),
            Object(Matching, "revolution/bte/btu_task1.c", mw_version="Wii/1.0"),
            Object(Matching, "revolution/bte/bd.c"),
            Object(Matching, "revolution/bte/bta_sys_conn.c"),
            Object(Matching, "revolution/bte/bta_sys_main.c"),
            Object(Matching, "revolution/bte/ptim.c"),
            Object(Matching, "revolution/bte/utl.c"),
            Object(Matching, "revolution/bte/bta_dm_act.c"),
            Object(Matching, "revolution/bte/bta_dm_api.c"),
            Object(Matching, "revolution/bte/bta_dm_main.c"),
            Object(Matching, "revolution/bte/bta_dm_pm.c"),
            Object(Matching, "revolution/bte/bta_hh_act.c"),
            Object(Matching, "revolution/bte/bta_hh_api.c"),
            Object(Matching, "revolution/bte/bta_hh_main.c"),
            Object(Matching, "revolution/bte/bta_hh_utils.c"),
            Object(Matching, "revolution/bte/btm_acl.c"),
            Object(Matching, "revolution/bte/btm_dev.c"),
            Object(Matching, "revolution/bte/btm_devctl.c"),
            Object(Matching, "revolution/bte/btm_discovery.c"),
            Object(Matching, "revolution/bte/btm_inq.c"),
            Object(Matching, "revolution/bte/btm_main.c"),
            Object(Matching, "revolution/bte/btm_pm.c"),
            Object(Matching, "revolution/bte/btu_hcif.c"),
            Object(Matching, "revolution/bte/btm_sco.c"),
            Object(Matching, "revolution/bte/btm_sec.c"),
            Object(Matching, "revolution/bte/btu_init.c"),
            Object(Matching, "revolution/bte/wbt_ext.c"),
            Object(Matching, "revolution/bte/gap_api.c"),
            Object(Matching, "revolution/bte/gap_conn.c"),
            Object(Matching, "revolution/bte/gap_utils.c"),
            Object(Matching, "revolution/bte/hcicmds.c"),
            Object(Matching, "revolution/bte/hidd_api.c"),
            Object(Matching, "revolution/bte/hidd_conn.c"),
            Object(Matching, "revolution/bte/hidd_mgmt.c"),
            Object(Matching, "revolution/bte/hidd_pm.c"),
            Object(Matching, "revolution/bte/hidh_api.c"),
            Object(Matching, "revolution/bte/hidh_conn.c"),
            Object(Matching, "revolution/bte/l2c_api.c"),
            Object(Matching, "revolution/bte/l2c_csm.c"),
            Object(Matching, "revolution/bte/l2c_link.c"),
            Object(Matching, "revolution/bte/l2c_main.c"),
            Object(Matching, "revolution/bte/l2c_utils.c"),
            Object(Matching, "revolution/bte/port_api.c"),
            Object(Matching, "revolution/bte/port_rfc.c"),
            Object(Matching, "revolution/bte/port_utils.c"),
            Object(Matching, "revolution/bte/rfc_l2cap_if.c"),
            Object(Matching, "revolution/bte/rfc_mx_fsm.c"),
            Object(Matching, "revolution/bte/rfc_port_fsm.c"),
            Object(Matching, "revolution/bte/rfc_port_if.c"),
            Object(Matching, "revolution/bte/rfc_ts_frames.c"),
            Object(Matching, "revolution/bte/rfc_utils.c"),
            Object(Matching, "revolution/bte/sdp_api.c"),
            Object(Matching, "revolution/bte/sdp_db.c"),
            Object(Matching, "revolution/bte/sdp_discovery.c"),
            Object(Matching, "revolution/bte/sdp_main.c"),
            Object(Matching, "revolution/bte/sdp_server.c"),
            Object(Matching, "revolution/bte/sdp_utils.c"),
        ],
    ),
    RVLSDKLib(
        "vi",
        [
            Object(Matching, "revolution/vi/vi.c"),
            Object(Matching, "revolution/vi/i2c.c"),
            Object(Matching, "revolution/vi/vi3in1.c"),
        ],
    ),
    RVLSDKLib(
        "tpl",
        [
            Object(Matching, "revolution/tpl/TPL.c"),
        ],
    ),
    RVLSDKLib(
        "kpad",
        [
            Object(Matching, "revolution/kpad/KPAD.c", extra_cflags=["-fp_contract off"]),
        ],
    ),
    RVLSDKLib(
        "usb",
        [
            # Parked: invented dead-stripped bodies carry retail strings (ruling pending; docs/exceptions.md, sdk-usb-1).
            Object(Matching, "revolution/usb/usb.c"),
        ],
    ),
    RVLSDKLib(
        "wpad",
        [
            Object(Matching, "revolution/wpad/WPAD.c"),
            Object(Matching, "revolution/wpad/WPADHIDParser.c", extra_cflags=["-fp_contract off"]),
            Object(Matching, "revolution/wpad/WPADEncrypt.c"),
            Object(Matching, "revolution/wpad/WPADMem.c"),
            Object(Matching, "revolution/wpad/wpad_debug_msg.c"),
        ],
    ),
    RVLSDKLib(
        "wud",
        [
            Object(Matching, "revolution/wud/WUD.c"),
            Object(Matching, "revolution/wud/WUDHidHost.c"),
            Object(Matching, "revolution/wud/debug_msg.c"),
        ],
    ),
    JPEGLib(
        [
            Object(Matching, "libjpeg/jcomapi.c"),
            Object(Matching, "libjpeg/jerror.c"),
            Object(Matching, "libjpeg/jutils.c"),
            Object(Matching, "libjpeg/jmemnobs.c"),
            Object(Matching, "libjpeg/jdapimin.c"),
            Object(Matching, "libjpeg/jdapistd.c"),
            Object(Matching, "libjpeg/jdcoefct.c"),
            Object(Matching, "libjpeg/jdcolor.c"),
            Object(Matching, "libjpeg/jddctmgr.c"),
            Object(Matching, "libjpeg/jdhuff.c"),
            # Needs the library's -ipa file: retail inlines latch_quant_tables into start_input_pass before
            # start_input_pass is inlined into consume_markers (local numbering, docs/compiler_regalloc.md).
            Object(Matching, "libjpeg/jdinput.c"),
            Object(Matching, "libjpeg/jdmainct.c"),
            Object(Matching, "libjpeg/jdmarker.c"),
            Object(Matching, "libjpeg/jdmaster.c"),
            Object(Matching, "libjpeg/jdmerge.c"),
            Object(Matching, "libjpeg/jdphuff.c"),
            Object(Matching, "libjpeg/jdpostct.c"),
            Object(Matching, "libjpeg/jdsample.c"),
            Object(Matching, "libjpeg/jidctflt.c"),
            Object(Matching, "libjpeg/jidctfst.c"),
            Object(Matching, "libjpeg/jidctint.c"),
            Object(Matching, "libjpeg/jidctred.c"),
            # alloc_small 93.9% (sum min_request+slop kept in a register); jinit_memory_mgr calls
            # getenv, still named fn_801A1CA8 in symbols.txt (MSL lane).
            Object(NonMatching, "libjpeg/jmemmgr.c"),
            Object(Matching, "libjpeg/jquant1.c"),
            Object(Matching, "libjpeg/jquant2.c"),
            Object(Matching, "libjpeg/jdatasrc.c"),
        ],
    ),
    ZlibLib(
        [
            Object(Matching, "zlib/adler32.c"),
            Object(Matching, "zlib/crc32.c"),
            Object(Matching, "zlib/inffast.c"),
            Object(Matching, "zlib/inflate.c"),
            Object(Matching, "zlib/inftrees.c"),
            Object(Matching, "zlib/zutil.c"),
        ],
    ),
    FMODLib(
        "fmod",
        [
            Object(Matching, "fmod/fmod.cpp"),
            Object(Matching, "fmod/fmod_async.cpp"),
            Object(Matching, "fmod/fmod_channel_emulated.cpp"),
            Object(Matching, "fmod/fmod_channel_real.cpp"),
            Object(Matching, "fmod/fmod_channelgroup.cpp"),
            Object(Matching, "fmod/fmod_channelgroupi.cpp"),
            Object(Matching, "fmod/fmod_channelpool.cpp"),
            Object(Matching, "fmod/fmod_globals.cpp"),
            Object(Matching, "fmod/fmod_listener.cpp"),
            Object(Matching, "fmod/fmod_metadata.cpp"),
            Object(Matching, "fmod/fmod_music.cpp"),
            Object(Matching, "fmod/fmod_plugin.cpp"),
            Object(Matching, "fmod/fmod_reverb.cpp"),
            Object(Matching, "fmod/fmod_reverbi.cpp"),
            Object(Matching, "fmod/fmod_sample_software.cpp"),
            Object(Matching, "fmod/fmod_sound_sample.cpp"),
            Object(Matching, "fmod/fmod_sound_stream.cpp"),
            Object(Matching, "fmod/fmod_soundgroupi.cpp"),
            Object(Matching, "fmod/fmod_speakerlevels_pool.cpp"),
            Object(NonMatching, "fmod/fmod_codec_dsp.cpp"),
            Object(NonMatching, "fmod/fmod_dsp_connection_wii.cpp"),
            Object(NonMatching, "fmod/fmod_dsp_convert_wii.cpp"),
            Object(Matching, "fmod/fmod_os_misc.cpp"),
            Object(Matching, "fmod/fmod_os_output.cpp"),
            Object(Matching, "fmod/fmod_sample_revolution.cpp"),
            Object(Matching, "fmod/fmod_dsp_codec.cpp"),
            Object(NonMatching, "fmod/fmod_dsp_connectioni.cpp"),
            Object(Matching, "fmod/fmod_dsp_connectionpool.cpp"),
            Object(Matching, "fmod/fmod_dsp_filter.cpp"),
            Object(NonMatching, "fmod/fmod_dsp_resampler.cpp"),
            Object(Matching, "fmod/fmod_dsp_soundcard.cpp"),
            Object(Matching, "fmod/fmod_dsp_wavetable.cpp"),
            Object(Matching, "fmod/fmod_codec.cpp"),
            Object(Matching, "fmod/fmod_codec_raw.cpp"),
            Object(Matching, "fmod/fmod_codec_user.cpp"),
            Object(Matching, "fmod/fmod_output_emulated.cpp"),
            Object(Matching, "fmod/fmod_output_polled.cpp"),
            Object(Matching, "fmod/fmod_output_software.cpp"),
            Object(NonMatching, "fmod/fmod_file.cpp"),
            Object(Matching, "fmod/fmod_file_disk.cpp"),
            Object(Matching, "fmod/fmod_file_memory.cpp"),
            Object(Matching, "fmod/fmod_file_null.cpp"),
            Object(Matching, "fmod/fmod_file_user.cpp"),
            Object(NonMatching, "fmod/fmod_memory.cpp"),
            Object(Matching, "fmod/fmod_string.cpp"),
            Object(Matching, "fmod/fmod_stringw.cpp"),
            Object(Matching, "fmod/fmod_thread.cpp"),
            Object(Matching, "fmod/fmod_time.cpp"),
            Object(Matching, "fmod/dlmalloc.cpp"),
            Object(Matching, "fmod/fmod_channel.cpp"),
            Object(Matching, "fmod/fmod_dsp.cpp"),
            Object(Matching, "fmod/fmod_output.cpp"),
            Object(Matching, "fmod/fmod_sound.cpp"),
            Object(Matching, "fmod/fmod_system.cpp"),
            Object(Matching, "fmod/fmod_buckethash.cpp"),
            Object(Matching, "fmod/fmod_compositionconditions.cpp"),
            Object(Matching, "fmod/fmod_event.cpp"),
            Object(Matching, "fmod/fmod_eventcategoryi.cpp"),
            Object(Matching, "fmod/fmod_eventenvelope.cpp"),
            Object(NonMatching, "fmod/fmod_eventgroupi.cpp"),
            Object(NonMatching, "fmod/fmod_eventimpl_simple.cpp"),
            Object(Matching, "fmod/fmod_eventinstancepool.cpp"),
            Object(Matching, "fmod/fmod_eventlayer.cpp"),
            Object(NonMatching, "fmod/fmod_eventprojecti.cpp"),
            Object(Matching, "fmod/fmod_eventreverb.cpp"),
            Object(Matching, "fmod/fmod_eventuserproperty.cpp"),
            Object(Matching, "fmod/fmod_musicsystemi.cpp"),
            Object(Matching, "fmod/fmod_simplemempool.cpp"),
            Object(Matching, "fmod/fmod_sounddef.cpp"),
        ],
    ),
    RenderWareLib(
        "renderware",
        [
            Object(Matching, "rwsdk/src/babbox.c"),
            Object(Matching, "rwsdk/src/babincam.c"),
            Object(Matching, "rwsdk/src/babinfrm.c"),
            Object(Matching, "rwsdk/src/babintex.c"),
            Object(NonMatching, "rwsdk/src/bacamera.c", extra_cflags=["-inline noauto"]),  # retail inlines no call (docs/exceptions.md)
            Object(Matching, "rwsdk/src/badevice.c"),
            Object(Matching, "rwsdk/src/baframe.c", extra_cflags=["-inline noauto"]),  # retail inlines no call (docs/exceptions.md)
            Object(Matching, "rwsdk/src/baimage.c"),
            Object(Matching, "rwsdk/src/baimras.c"),
            Object(Matching, "rwsdk/src/baraster.c"),
            Object(Matching, "rwsdk/src/baresamp.c"),
            Object(Matching, "rwsdk/src/basync.c", extra_cflags=["-inline noauto"]),  # retail inlines no call (docs/exceptions.md)
            Object(Matching, "rwsdk/src/batextur.c"),
            Object(Matching, "rwsdk/src/batypehf.c"),
            Object(Matching, "rwsdk/src/pipe/p2/baim3d.c"),
            Object(Matching, "rwsdk/src/pipe/p2/bapipe.c"),
            Object(Matching, "rwsdk/src/pipe/p2/p2altmdl.c"),
            Object(Matching, "rwsdk/src/pipe/p2/p2core.c"),
            Object(NonMatching, "rwsdk/src/pipe/p2/p2define.c"),
            Object(NonMatching, "rwsdk/src/pipe/p2/p2dep.c"),
            Object(Matching, "rwsdk/src/pipe/p2/p2heap.c"),
            Object(Matching, "rwsdk/src/pipe/p2/p2renderstate.c"),
            Object(Matching, "rwsdk/src/pipe/p2/p2resort.c"),
            Object(Matching, "rwsdk/src/pipe/p2/wii/im3dpipe.c"),
            Object(Matching, "rwsdk/src/plcore/babinary.c"),
            Object(Matching, "rwsdk/src/plcore/bacolor.c"),
            Object(Matching, "rwsdk/src/plcore/baerr.c"),
            Object(Matching, "rwsdk/src/plcore/balist.c"),
            Object(Matching, "rwsdk/src/plcore/bamatrix.c"),
            Object(Matching, "rwsdk/src/plcore/bamemory.c"),
            Object(Matching, "rwsdk/src/plcore/baresour.c"),
            Object(Matching, "rwsdk/src/plcore/bastream.c"),
            Object(Matching, "rwsdk/src/plcore/batkbin.c"),
            Object(Matching, "rwsdk/src/plcore/batkreg.c"),
            Object(Matching, "rwsdk/src/plcore/bavector.c"),
            Object(Matching, "rwsdk/src/plcore/resmem.c"),
            Object(Matching, "rwsdk/src/plcore/rwchunkgroup.c"),
            Object(Matching, "rwsdk/src/plcore/rwstring.c"),
            Object(NonMatching, "rwsdk/src/driver/wii/dlsprite.c"),
            Object(Matching, "rwsdk/src/driver/wii/dltexdic.c"),
            Object(Matching, "rwsdk/src/driver/wii/dltoken.c"),
            Object(Matching, "rwsdk/plugin/dmorph/rpdmorph.c"),
            Object(Matching, "rwsdk/plugin/matfx/multiTex.c"),
            Object(Matching, "rwsdk/plugin/collis/colldata.c"),
            Object(Matching, "rwsdk/plugin/collis/ctdata.c"),
            Object(Matching, "rwsdk/plugin/dmorph/dmphanim.c"),
            Object(Matching, "rwsdk/plugin/dmorph/dmphgeom.c"),
            Object(Matching, "rwsdk/plugin/dmorph/dmphintp.c"),
            Object(Matching, "rwsdk/plugin/matfx/gcn/multiTexGcn.c"),
            Object(Matching, "rwsdk/plugin/matfx/gcn/multiTexGcnData.c"),
            Object(NonMatching, "rwsdk/plugin/matfx/gcn/multiTexGcnPipe.c"),
            Object(NonMatching, "rwsdk/plugin/matfx/gcn/nbtGen.c"),
            Object(Matching, "rwsdk/plugin/hanim/rphanim.c"),
            Object(Matching, "rwsdk/plugin/hanim/stdkey.c"),
            Object(Matching, "rwsdk/plugin/pvs/rppvs.c"),
            Object(Matching, "rwsdk/plugin/pvs/rppvsaux.c"),
            Object(Matching, "rwsdk/plugin/random/mertwist.c"),
            Object(Matching, "rwsdk/plugin/random/rprandom.c"),
            Object(Matching, "rwsdk/plugin/userdata/rpusrdat.c"),
            Object(Matching, "rwsdk/plugin/uvanim/linear.c"),
            Object(Matching, "rwsdk/plugin/uvanim/param.c"),
            Object(Matching, "rwsdk/plugin/uvanim/rpuvanim.c"),
            Object(Matching, "rwsdk/plugin/uvanim/stdkey.c"),
            Object(Matching, "rwsdk/world/babinwor.c"),
            Object(Matching, "rwsdk/world/baclump.c"),
            Object(Matching, "rwsdk/world/bageomet.c"),
            Object(Matching, "rwsdk/world/balight.c"),
            Object(Matching, "rwsdk/world/bamateri.c"),
            Object(Matching, "rwsdk/world/bamatlst.c"),
            Object(Matching, "rwsdk/world/bamesh.c"),
            Object(Matching, "rwsdk/world/bameshop.c"),
            Object(Matching, "rwsdk/world/basector.c"),
            Object(Matching, "rwsdk/world/baworld.c"),
            Object(Matching, "rwsdk/world/baworobj.c"),
            Object(Matching, "rwsdk/world/climaxcommon.c"),
            Object(Matching, "rwsdk/world/bapipew.c"),
            Object(Matching, "rwsdk/world/climaxplatform.c"),
            Object(NonMatching, "rwsdk/world/gclights.c"),
            Object(Matching, "rwsdk/world/gcmorph.c"),
            Object(Matching, "rwsdk/world/gcpipe.c"),
            Object(Matching, "rwsdk/world/native.c"),
            Object(NonMatching, "rwsdk/world/nodeGameCubeAtomicAllInOne.c"),
            Object(Matching, "rwsdk/world/nodeGameCubeWorldSectorAllInOne.c"),
            Object(Matching, "rwsdk/world/setup.c"),
            Object(NonMatching, "rwsdk/world/vtxfmt.c"),
            Object(Matching, "rwsdk/world/wrldpipe.c"),
            Object(Matching, "rwsdk/world/geomcond.c"),
            Object(Matching, "rwsdk/world/ibuffer.c"),
            Object(Matching, "rwsdk/world/instancegeom.c"),
            Object(Matching, "rwsdk/world/instanceworld.c"),
            Object(Matching, "rwsdk/world/itools.c"),
            Object(Matching, "rwsdk/world/vbuffer.c"),
            Object(Matching, "rwsdk/world/vtxdesc.c"),
            Object(Matching, "rwsdk/plugin/skin2/bsplit.c"),
            Object(NonMatching, "rwsdk/plugin/skin2/rpskin.c"),
            Object(Matching, "rwsdk/plugin/skin2/rescache.c"),
            Object(NonMatching, "rwsdk/plugin/skin2/skingcn.c"),
            Object(Matching, "rwsdk/plugin/skin2/skingcnasm.c"),
            Object(Matching, "rwsdk/plugin/skin2/skinmatrixblend.c"),
            Object(Matching, "rwsdk/plugin/skin2/skinstream.c"),
            Object(Matching, "rwsdk/tool/skinsplit/rtskinsp.c"),
            Object(Matching, "rwsdk/tool/slerp/rtslerp.c"),
            Object(Matching, "rwsdk/tool/fsyst/rtfsmgr.c"),
            Object(Matching, "rwsdk/tool/fsyst/rtfsyst.c"),
            Object(Matching, "rwsdk/tool/anim/rtanim.c"),
            Object(Matching, "rwsdk/tool/quat/rtquat.c"),
            Object(Matching, "rwsdk/tool/dict/rtdict.c"),
        ],
    ),
    HBMLib(
        [
            Object(Matching, "revolution/hbm/HBMFrameController.cpp"),
            Object(Matching, "revolution/hbm/HBMAnmController.cpp"),
            Object(Matching, "revolution/hbm/HBMGUIManager.cpp"),
            Object(Matching, "revolution/hbm/HBMController.cpp"),
            Object(Matching, "revolution/hbm/HBMRemoteSpk.cpp"),
            Object(Matching, "revolution/hbm/HBMAxSound.cpp"),
            Object(Matching, "revolution/hbm/HBMCommon.cpp"),
            Object(Matching, "revolution/hbm/HBMBase.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/lyt/lyt_animation.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/lyt/lyt_arcResourceAccessor.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/lyt/lyt_bounding.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/lyt/lyt_common.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/lyt/lyt_drawInfo.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/lyt/lyt_group.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/lyt/lyt_layout.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/lyt/lyt_material.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/lyt/lyt_pane.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/lyt/lyt_picture.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/lyt/lyt_resourceAccessor.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/lyt/lyt_textBox.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/lyt/lyt_window.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/math/math_triangular.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/ut/ut_binaryFileFormat.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/ut/ut_CharStrmReader.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/ut/ut_CharWriter.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/ut/ut_Font.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/ut/ut_LinkList.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/ut/ut_list.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/ut/ut_ResFont.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/ut/ut_ResFontBase.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/ut/ut_TagProcessorBase.cpp"),
            Object(Matching, "revolution/hbm/nw4hbm/ut/ut_TextWriterBase.cpp"),
            Object(Matching, "revolution/hbm/sound/mix.cpp"),
            Object(Matching, "revolution/hbm/sound/syn.cpp"),
            Object(Matching, "revolution/hbm/sound/synctrl.cpp"),
            Object(Matching, "revolution/hbm/sound/synenv.cpp"),
            Object(Matching, "revolution/hbm/sound/synmix.cpp"),
            Object(Matching, "revolution/hbm/sound/synpitch.cpp"),
            Object(Matching, "revolution/hbm/sound/synsample.cpp"),
            Object(Matching, "revolution/hbm/sound/synvoice.cpp"),
            Object(Matching, "revolution/hbm/sound/seq.cpp"),
        ],
    ),
    GameLib(
        "game",
        [
            Object(Matching, "game/PadRumble.cpp"),
            Object(NonMatching, "game/unk_8000A040.cpp"),
            Object(Matching, "game/CCollisionCapsule.cpp"),
            Object(Matching, "game/CPreCache.cpp"),
            Object(Matching, "game/CActorTest.cpp"),
            Object(Matching, "game/CPlayerCostume.cpp"),
            Object(Matching, "game/CCopCar.cpp"),
            Object(Matching, "game/CHitEffectManager.cpp"),
            Object(Matching, "game/CBasicMount.cpp"),
            Object(Matching, "game/CClumpSwap.cpp"),
            Object(Matching, "game/CScalingObject.cpp"),
            Object(Matching, "game/CSceneObject.cpp"),
            Object(Matching, "game/CIGCPlayerMover.cpp"),
            Object(NonMatching, "game/CAmbientInterferencePoint.cpp"),
            Object(Matching, "game/CAmbientTextMessage.cpp"),
            Object(Matching, "game/CAmbientVoicemail.cpp"),
            Object(NonMatching, "game/CPointOfInterest.cpp"),
            Object(NonMatching, "game/unk_PointOfInterestManager.cpp"),
            Object(Matching, "game/CPointTrigger.cpp"),
            Object(NonMatching, "game/CPsyPointOfInterest.cpp"),
            Object(Matching, "game/CGenericSpawner.cpp"),
            Object(Matching, "game/unk_800C8598.cpp"),
            Object(Matching, "game/CColdTrigger.cpp"),
            Object(Matching, "game/CCombatZoneSetEnemyCountTrigger.cpp"),
            Object(Matching, "game/CCounter.cpp"),
            Object(Matching, "game/DataTrigger.cpp"),
            Object(Matching, "game/CDesignerDebugMsg.cpp"),
            Object(Matching, "game/CFadeTrigger.cpp"),
            Object(Matching, "game/CFlickerTrigger.cpp"),
            Object(Matching, "game/FMVTrigger.cpp"),
            Object(Matching, "game/CKeyHUDTrigger.cpp"),
            Object(Matching, "game/CMessageRelay.cpp"),
            Object(Matching, "game/CModalTutorialMessageTrigger.cpp"),
            Object(Matching, "game/MusicTrigger.cpp"),
            Object(Matching, "game/COnScreenPromptTrigger.cpp"),
            Object(Matching, "game/CScreenShakeTrigger.cpp"),
            Object(Matching, "game/CPadRumbleTrigger.cpp"),
            Object(Matching, "game/CPhoneCallTrigger.cpp"),
            Object(Matching, "game/CPhoneMessageTrigger.cpp"),
            Object(Matching, "game/PlaneTrigger.cpp"),
            Object(Matching, "game/CPlayerStateTrigger.cpp"),
            Object(Matching, "game/CPsycologyZoneTrigger.cpp"),
            Object(Matching, "game/CPsyTrigger.cpp"),
            Object(Matching, "game/RespotZoneTrigger.cpp"),
            Object(Matching, "game/CSlowMoTrigger.cpp"),
            Object(Matching, "game/CSplashScreenTrigger.cpp"),
            Object(Matching, "game/CStaticInit.cpp"),
            Object(Matching, "game/CSwimStaminaTrigger.cpp"),
            Object(Matching, "game/CVoicemailMessageTrigger.cpp"),
            Object(Matching, "game/AreaTriggerBox.cpp"),
            Object(NonMatching, "game/CAIFrozenLakeEnemyRespot.cpp"),
            Object(NonMatching, "game/CButtonPuzzle.cpp"),
            Object(NonMatching, "game/CSequencePuzzleLogic.cpp"),
            Object(NonMatching, "game/CDrawingTest.cpp"),
            Object(Matching, "game/CMapLayer.cpp"),
            Object(Matching, "game/CMapWaypoint.cpp"),
            Object(Matching, "game/CMapWaypointPurge.cpp"),
        ],
    ),
    GameLib(
        "climax",
        [
            Object(Matching, "climax/cattributehandler.cpp"),
            Object(NonMatching, "climax/EventHandler.cpp"),
            Object(Matching, "climax/FullHeapAllocator.cpp"),
            Object(Matching, "climax/ZStream.cpp"),
            Object(Matching, "climax/Utils.cpp"),
            Object(Matching, "climax/ImageCodec.cpp"),
            Object(NonMatching, "climax/Localisation.cpp"),
            Object(Matching, "climax/GeometryData.cpp"),
            Object(Matching, "climax/GeomTypes.cpp"),
            Object(NonMatching, "climax/ClimaxS1Lib.cpp"),
            Object(Matching, "climax/ClimaxT1Material.cpp"),
            Object(NonMatching, "climax/CompAnim.cpp"),
            Object(Matching, "climax/scalekey.cpp"),
            Object(Matching, "climax/Audio/Helper.cpp"),
            Object(Matching, "climax/Audio/Listener.cpp"),
            Object(Matching, "climax/Audio/Source.cpp"),
            Object(NonMatching, "climax/Audio/System.cpp"),
            Object(Matching, "climax/Time.cpp"),
            Object(Matching, "climax/ClimaxTrailKeyFrame.cpp"),
            Object(Matching, "climax/RwPlugin.cpp"),
            Object(Matching, "climax/TevCBMgr.cpp"),
            Object(Matching, "climax/Audio/AsyncEventSource.cpp"),
            Object(Matching, "climax/ClimaxT1Dictionary.cpp"),
            Object(NonMatching, "climax/ClimaxT1Effect.cpp"),
            Object(Matching, "climax/ClimaxT1Frame.cpp"),
            Object(Matching, "climax/ClimaxT1Geometry.cpp"),
            Object(Matching, "climax/ClimaxT1KeyFrame.cpp"),
        ],
    ),
]


# Optional callback to adjust link order. This can be used to add, remove, or reorder objects.
# This is called once per module, with the module ID and the current link order.
#
# For example, this adds "dummy.c" to the end of the DOL link order if configured with --non-matching.
# "dummy.c" *must* be configured as a Matching (or Equivalent) object in order to be linked.
def link_order_callback(module_id: int, objects: List[str]) -> List[str]:
    # Don't modify the link order for matching builds
    if not config.non_matching:
        return objects
    if module_id == 0:  # DOL
        return objects + ["dummy.c"]
    return objects


# Uncomment to enable the link order callback.
# config.link_order_callback = link_order_callback


# Optional extra categories for progress tracking
# Adjust as desired for your project
config.progress_categories = [
    ProgressCategory("game", "Game Code"),
    ProgressCategory("sdk", "SDK Code"),
    ProgressCategory("middleware", "Middleware (FMOD, RenderWare, libjpeg)"),
]
config.progress_each_module = args.verbose
# Optional extra arguments to `objdiff-cli report generate`
config.progress_report_args = [
    # Marks relocations as mismatching if the target value is different
    # Default is "functionRelocDiffs=none", which is most lenient
    # "--config functionRelocDiffs=data_value",
]

if args.mode == "configure":
    # Write build.ninja and objdiff.json
    generate_build(config)
elif args.mode == "progress":
    # Print progress information
    calculate_progress(config)
else:
    sys.exit("Unknown mode: " + args.mode)
