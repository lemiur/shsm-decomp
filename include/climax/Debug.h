#ifndef CLIMAX_DEBUG_H
#define CLIMAX_DEBUG_H

/*
 * The Climax engine's debug-build warning. Gormiti's Debug build writes a file-and-line warning to
 * the debug stream RWS::cdbg() at such a site (CAttributePacket::CreateEntity, cattributehandler.cpp
 * line 778: "WARNING: cattributehandler.cpp(778): InstanceID alraedy exists." when the instance is
 * already registered). Gormiti's Final build and retail carry none of it, so in the retail build the
 * macro expands to nothing, as RWASSERT does (rwsdk/plcore/badebug.h). The condition is the test of
 * the warning and the text is printed after the file and line.
 *
 * The macro's name is invented, after the stream the Debug build writes to (the macro itself does
 * not survive in a binary); its two-argument shape follows the Debug build, whose test and stream
 * output share one source line (778).
 *
 * Use: only where Gormiti's Debug build shows such a warning, with the comment
 * "// Debug build: warning at this point (Gormiti Debug)" and the text that build prints, spelling
 * included.
 */
#ifndef RWS_WARN
#define RWS_WARN(_condition, _text) /* no-op in a release build */
#endif

/*
 * The Climax engine's debug-build assertion failure. Gormiti's Debug build reports it on screen
 * (DoOnscreenAssert with the file, the line and the text, guarded by a re-entry flag, then a break
 * through the MSR) at a source line of its own inside an ordinary `if` whose test is the failing
 * case (System::LoadEventGroup, System.cpp lines 1174 and 1176: "Invalid parameters"). Gormiti's
 * Final build and retail carry none of it: the macro expands to nothing, and the `if` around it
 * stays in the source with an empty body (the compiler drops it, but it still counts towards the
 * function's size when the compiler decides whether to inline it automatically).
 *
 * The macro's name is invented (the macro itself does not survive in a binary); it takes the text
 * only, as the Debug build tests nothing but its re-entry flag on the assertion's line.
 *
 * Use: only where Gormiti's Debug build shows such an assertion, with the comment
 * "// Debug build: assertion at this point (Gormiti Debug)" and the text that build prints.
 */
#ifndef RWS_ASSERT_FAIL
#define RWS_ASSERT_FAIL(_text) /* no-op in a release build */
#endif

#endif
