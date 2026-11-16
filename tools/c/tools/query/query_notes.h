/* query.py's notes search, importable by other tools (brief and others call query.doc_notes): passages of docs/*.md
 * and source comment blocks that mention a term, with the helpers it is built from (note_terms, term_regex,
 * doc_passages, comment_blocks, source_hits). Everything is allocated from ctx->a (or the arena given); the vectors
 * are heap vectors (release with avp_vec_free). Errors are raised as the Python raised them. */
#pragma once
#include "query.h"

/* One hit: {file, line, heading, text}; heading p NULL for a source hit (None), "" for a docs passage under no
 * heading. file is repository-relative with forward slashes. */
typedef struct {
    AvpStr file;
    i64 line;
    AvpStr heading;
    AvpStr text;
} QueryNoteHit;
typedef AVP_VEC(QueryNoteHit) QueryNoteHits;

/* note_terms(ctx, term): the search patterns for term (term itself first; a symbol adds its name, its qualified and
 * plain base names and its address; a symbols.txt name its address), duplicates and empty strings dropped. *sym
 * (may be NULL) receives the symbol the term resolved to, or NULL. */
AvpStrVec query_note_terms(QueryCtx *ctx, AvpStr term, const ShsmSymRow **sym);

/* term_regex(terms): word-bounded alternatives for word-like terms ((?<![\w@$])t(?![\w@$])), plain escaped ones
 * for the rest; IGNORECASE only when no term is word-like (also for no terms: the empty pattern). */
ShsmRe *query_term_regex(AvpArena *a, const AvpStr *terms, size_t n);

/* doc_passages(path, rx): (line, heading path "A > B", text) of every paragraph, table row (not the header row),
 * list item and heading of a markdown file that rx finds. An unreadable file gives none; a file that is not UTF-8
 * raises ValueError (as open().read() does). */
typedef struct {
    i64 line;
    AvpStr heading, text;
} QueryPassage;
typedef AVP_VEC(QueryPassage) QueryPassages;
QueryPassages query_doc_passages(AvpArena *a, const char *path, const ShsmRe *rx);

/* comment_blocks(lines): (first line, last line) of the `//` line and slash-star block comment runs of lines[0..n), 1-based, as the
 * Python computes them (an unterminated block ends one past n). */
typedef struct {
    i64 first, last;
} QueryBlock;
typedef AVP_VEC(QueryBlock) QueryBlocks;
QueryBlocks query_comment_blocks(const AvpStr *lines, size_t n);

/* source_hits(ctx, rx, addr_terms): comment blocks of src/ and include/ (.c .cpp .cp .h .hpp .inc) that rx finds
 * (with the definition line after the block when there is one), plus the header comment of each function whose
 * closing line is `} // <addr term>`. heading is p NULL. */
QueryNoteHits query_source_hits(QueryCtx *ctx, const ShsmRe *rx, const AvpStr *addr_terms, size_t n);

/* doc_notes(ctx, term, docs_only, source_only, exclude, expand). opts NULL: the Python defaults (docs_only True,
 * source_only False, no exclusions, expand True). *terms (may be NULL) receives the search terms. */
typedef struct {
    bool docs_only, source_only;
    const char *const *exclude; /* docs file names to skip ("progress.md") */
    size_t nexclude;
    bool no_expand;             /* expand=False: search for the term alone */
} QueryNotesOpts;
QueryNoteHits query_doc_notes(QueryCtx *ctx, AvpStr term, const QueryNotesOpts *opts, AvpStrVec *terms);
/* A hit as the Python dict {file, line, heading, text}. */
AvpJson *query_note_hit_json(AvpArena *a, const QueryNoteHit *h);
