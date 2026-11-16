/* classes_scan.c: the scan engine (run_scan's rescan branch, tools/classes.py 2355-2361): the data regions from
 * splits.txt, Program.from_dir, Analysis and Analysis.to_json. */
#include "classes_scan.h"

#include "classes_analysis.h"
#include "shsm_cli.h"

AvpJson *classes_scan(AvpArena *a, const ClassesScanOpts *opts, AvpJson *inputs) {
    ClassesRegions regions = {0};
    if (opts->splits_path != NULL) {
        AvpStr text;
        if (!avp_read_text(a, opts->splits_path, AVP_TM_UTF8, &text)) shsm_raise_io();
        regions = classes_parse_data_regions(a, text);
    }
    ClassesProgram *prog = classes_program_from_dir(a, opts->asm_dir, opts->symbols_path);
    if (opts->note) *opts->note = prog->note;
    ClassesAnalysis *an = classes_analysis_new_rule(a, prog, &regions, opts->base_rule);
    return classes_analysis_to_json(an, inputs);
}
