/* shsm_child: the child process shsm_cli_test runs through shsm_run.
 *   shsm_child args ARG...   prints the working directory and the arguments as a JSON array of strings
 *   shsm_child fail          prints "out" on stdout, "bad thing" and "more" on stderr and exits 3
 *   shsm_child slow          prints "early" (flushed) and sleeps five minutes
 *   shsm_child -t deps       (as tools/bin/ninja.exe) prints ninja_deps.txt of the working directory */
#include "avp/avp.h"

#include <direct.h>
#include <windows.h>

static void put_json_string(const char *s) {
    putchar('"');
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') putchar('\\');
        putchar(*p);
    }
    putchar('"');
}

static int run(int argc, char **argv) {
    if (argc < 2) return 2;
    if (!strcmp(argv[1], "args")) {
        char cwd[MAX_PATH * 2];
        if (!_getcwd(cwd, (int)sizeof cwd)) return 2;
        putchar('[');
        put_json_string(cwd);
        for (int i = 2; i < argc; i++) {
            putchar(',');
            put_json_string(argv[i]);
        }
        puts("]");
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "-t") && !strcmp(argv[2], "deps")) { /* a stand-in for `ninja -t deps` */
        FILE *f = fopen("ninja_deps.txt", "rb");
        char buf[4096];
        size_t n;
        if (!f) return 1;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) fwrite(buf, 1, n, stdout);
        fclose(f);
        return 0;
    }
    if (!strcmp(argv[1], "fail")) {
        fputs("out\n", stdout);
        fflush(stdout);
        fputs("bad thing\nmore", stderr);
        return 3;
    }
    if (!strcmp(argv[1], "slow")) {
        fputs("early\n", stdout);
        fflush(stdout);
        Sleep(300000);
        return 0;
    }
    return 2;
}

AVP_MAIN(run, "shsm_child")
