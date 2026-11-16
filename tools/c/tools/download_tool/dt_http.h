/* download_tool: an HTTP(S) GET through WinHTTP (the part of urllib.request.urlopen the tool uses). */
#pragma once
#include "avp/avp.h"

typedef struct {
    int status;        /* the HTTP status of the final response; 0 when no response arrived */
    char reason[128];  /* its reason phrase */
    char error[600];   /* for a transport failure: str(URLError.reason), e.g. "[WinError 10061] ..." */
} DtHttpResult;

typedef struct {
    /* Called once, after a successful (2xx) response header and before the first body bytes. False aborts. */
    bool (*begin)(void *ctx);
    /* A chunk of the body. False aborts. */
    bool (*data)(void *ctx, const void *p, size_t n);
    void *ctx;
} DtSink;

/* GET url (http or https, redirects followed) with `User-Agent: Mozilla/5.0`. True when the response is 2xx and the
 * body was handed to the sink in full. False with r->status != 0 for another status (nothing was given to the sink),
 * with r->error set for a failure before a response or while reading it, or with both clear when the sink aborted.
 * no_proxy: do not use the system proxy settings. */
bool dt_http_get(const char *url, bool no_proxy, const DtSink *sink, DtHttpResult *r);
