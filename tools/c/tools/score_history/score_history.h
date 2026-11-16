/* score_history: per-function current / best / historical scores (tools/score_history.py), library
 * tool_score_history. main.c is a thin wrapper over score_history_run; other tools (pick) call it in-process. */
#pragma once
#include "avp/arena.h"
#include "avp/str.h"

/* The whole score_history CLI run in-process: argv excludes the program name; stdout text and stderr text go to the
   two buffers instead of the console; returns the exit code the exe would return (0/1/2). */
int score_history_run(AvpArena *a, int argc, const char *const *argv, AvpBuf *out, AvpBuf *err);
