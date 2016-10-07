



#include "apr.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_lib.h"
#include "apr_signal.h"

#define APR_WANT_STDIO          
#define APR_WANT_STRFUNC
#define APR_WANT_MEMFUNC
#include "apr_want.h"

#include "util_filter.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_request.h"
#include "http_vhost.h"
#include "http_connection.h"
#include "http_log.h"           
#include "apr_date.h"           
#include "util_charset.h"
#include "util_ebcdic.h"
#include "util_time.h"

#include "mod_core.h"

#include "trace_tool.h"

#if APR_HAVE_STDARG_H
#include <stdarg.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#include <trace_tool.h>

#endif

APLOG_USE_MODULE(http);

typedef struct http_filter_ctx
{
    apr_off_t remaining;
    apr_off_t limit;
    apr_off_t limit_used;
    apr_int32_t chunk_used;
    apr_int32_t chunk_bws;
    apr_int32_t chunkbits;
    enum
    {
        BODY_NONE, 
        BODY_LENGTH, 
        BODY_CHUNK, 
        BODY_CHUNK_PART, 
        BODY_CHUNK_EXT, 
        BODY_CHUNK_CR, 
        BODY_CHUNK_LF, 
        BODY_CHUNK_DATA, 
        BODY_CHUNK_END, 
        BODY_CHUNK_END_LF, 
        BODY_CHUNK_TRAILER 
    } state;
    unsigned int eos_sent :1;
    apr_bucket_brigade *bb;
} http_ctx_t;


static apr_status_t bail_out_on_error(http_ctx_t *ctx,
                                      ap_filter_t *f,
                                      int http_error)
{
    apr_bucket *e;
    apr_bucket_brigade *bb = ctx->bb;

    apr_brigade_cleanup(bb);

    if (f->r->proxyreq == PROXYREQ_RESPONSE) {
        switch (http_error) {
        case HTTP_REQUEST_ENTITY_TOO_LARGE:
            return APR_ENOSPC;

        case HTTP_REQUEST_TIME_OUT:
            return APR_INCOMPLETE;

        case HTTP_NOT_IMPLEMENTED:
            return APR_ENOTIMPL;

        default:
            return APR_EGENERAL;
        }
    }

    e = ap_bucket_error_create(http_error,
                               NULL, f->r->pool,
                               f->c->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(bb, e);
    e = apr_bucket_eos_create(f->c->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(bb, e);
    ctx->eos_sent = 1;
    
    f->r->connection->keepalive = AP_CONN_CLOSE;
    return ap_pass_brigade(f->r->output_filters, bb);
}


static apr_status_t parse_chunk_size(http_ctx_t *ctx, const char *buffer,
                                     apr_size_t len, int linelimit)
{
    apr_size_t i = 0;

    while (i < len) {
        char c = buffer[i];

        ap_xlate_proto_from_ascii(&c, 1);

        
        if (ctx->state == BODY_CHUNK_END
                || ctx->state == BODY_CHUNK_END_LF) {
            if (c == LF) {
                ctx->state = BODY_CHUNK;
            }
            else if (c == CR && ctx->state == BODY_CHUNK_END) {
                ctx->state = BODY_CHUNK_END_LF;
            }
            else {
                
                return APR_EINVAL;
            }
            i++;
            continue;
        }

        
        if (ctx->state == BODY_CHUNK) {
            if (!apr_isxdigit(c)) {
                
                return APR_EINVAL;
            }
            else {
                ctx->state = BODY_CHUNK_PART;
            }
            ctx->remaining = 0;
            ctx->chunkbits = sizeof(apr_off_t) * 8;
            ctx->chunk_used = 0;
            ctx->chunk_bws = 0;
        }

        if (c == LF) {
            if (ctx->remaining) {
                ctx->state = BODY_CHUNK_DATA;
            }
            else {
                ctx->state = BODY_CHUNK_TRAILER;
            }
        }
        else if (ctx->state == BODY_CHUNK_LF) {
            
            return APR_EINVAL;
        }
        else if (c == CR) {
            ctx->state = BODY_CHUNK_LF;
        }
        else if (c == ';') {
            ctx->state = BODY_CHUNK_EXT;
        }
        else if (ctx->state == BODY_CHUNK_EXT) {
            
            if (c != '\t' && apr_iscntrl(c)) {
                return APR_EINVAL;
            }
        }
        else if (c == ' ' || c == '\t') {
            
            ctx->state = BODY_CHUNK_CR;
            if (++ctx->chunk_bws > 10) {
                return APR_EINVAL;
            }
        }
        else if (ctx->state == BODY_CHUNK_CR) {
            
            return APR_EINVAL;
        }
        else if (ctx->state == BODY_CHUNK_PART) {
            int xvalue;

            
            if (!ctx->remaining && c == '0') {
                i++;
                continue;
            }

            ctx->chunkbits -= 4;
            if (ctx->chunkbits < 0) {
                
                return APR_ENOSPC;
            }

            if (c >= '0' && c <= '9') {
                xvalue = c - '0';
            }
            else if (c >= 'A' && c <= 'F') {
                xvalue = c - 'A' + 0xa;
            }
            else if (c >= 'a' && c <= 'f') {
                xvalue = c - 'a' + 0xa;
            }
            else {
                
                return APR_EINVAL;
            }

            ctx->remaining = (ctx->remaining << 4) | xvalue;
            if (ctx->remaining < 0) {
                
                return APR_ENOSPC;
            }
        }
        else {
            
            return APR_EGENERAL;
        }

        i++;
    }

    
    ctx->chunk_used += len;
    if (ctx->chunk_used < 0 || ctx->chunk_used > linelimit) {
        return APR_ENOSPC;
    }

    return APR_SUCCESS;
}

static apr_status_t read_chunked_trailers(http_ctx_t *ctx, ap_filter_t *f,
                                          apr_bucket_brigade *b, int merge)
{
    int rv;
    apr_bucket *e;
    request_rec *r = f->r;
    apr_table_t *saved_headers_in = r->headers_in;
    int saved_status = r->status;

    r->status = HTTP_OK;
    r->headers_in = r->trailers_in;
    apr_table_clear(r->headers_in);
    ap_get_mime_headers(r);

    if(r->status == HTTP_OK) {
        r->status = saved_status;
        e = apr_bucket_eos_create(f->c->bucket_alloc);
        APR_BRIGADE_INSERT_TAIL(b, e);
        ctx->eos_sent = 1;
        rv = APR_SUCCESS;
    }
    else {
        const char *error_notes = apr_table_get(r->notes,
                                                "error-notes");
        ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(02656)
                      "Error while reading HTTP trailer: %i%s%s",
                      r->status, error_notes ? ": " : "",
                      error_notes ? error_notes : "");
        rv = APR_EINVAL;
    }

    if(!merge) {
        r->headers_in = saved_headers_in;
    }
    else {
        r->headers_in = apr_table_overlay(r->pool, saved_headers_in,
                r->trailers_in);
    }

    return rv;
}


apr_status_t ap_http_filter(ap_filter_t *f, apr_bucket_brigade *b,
                            ap_input_mode_t mode, apr_read_type_e block,
                            apr_off_t readbytes)
{
    core_server_config *conf;
    apr_bucket *e;
    http_ctx_t *ctx = f->ctx;
    apr_status_t rv;
    int http_error = HTTP_REQUEST_ENTITY_TOO_LARGE;
    apr_bucket_brigade *bb;
    int again;

    conf = (core_server_config *)
        ap_get_module_config(f->r->server->module_config, &core_module);

    
    if (mode != AP_MODE_READBYTES && mode != AP_MODE_GETLINE) {
        return ap_get_brigade(f->next, b, mode, block, readbytes);
    }

    if (!ctx) {
        const char *tenc, *lenp;
        f->ctx = ctx = apr_pcalloc(f->r->pool, sizeof(*ctx));
        ctx->state = BODY_NONE;
        ctx->bb = apr_brigade_create(f->r->pool, f->c->bucket_alloc);
        bb = ctx->bb;

        
        if (!f->r->proxyreq) {
            ctx->limit = ap_get_limit_req_body(f->r);
        }
        else {
            ctx->limit = 0;
        }

        tenc = apr_table_get(f->r->headers_in, "Transfer-Encoding");
        lenp = apr_table_get(f->r->headers_in, "Content-Length");

        if (tenc) {
            if (strcasecmp(tenc, "chunked") == 0 
                    || ap_find_last_token(f->r->pool, tenc, "chunked")) {
                ctx->state = BODY_CHUNK;
            }
            else if (f->r->proxyreq == PROXYREQ_RESPONSE) {
                
                ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, f->r, APLOGNO(02555)
                              "Unknown Transfer-Encoding: %s; "
                              "using read-until-close", tenc);
                tenc = NULL;
            }
            else {
                
                ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, f->r, APLOGNO(01585)
                              "Unknown Transfer-Encoding: %s", tenc);
                return bail_out_on_error(ctx, f, HTTP_BAD_REQUEST);
            }
            lenp = NULL;
        }
        if (lenp) {
            char *endstr;

            ctx->state = BODY_LENGTH;

            
            if (apr_strtoff(&ctx->remaining, lenp, &endstr, 10)
                || endstr == lenp || *endstr || ctx->remaining < 0) {

                ctx->remaining = 0;
                ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, f->r, APLOGNO(01587)
                              "Invalid Content-Length");

                return bail_out_on_error(ctx, f, HTTP_BAD_REQUEST);
            }

            
            if (ctx->limit && ctx->limit < ctx->remaining) {
                ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, f->r, APLOGNO(01588)
                          "Requested content-length of %" APR_OFF_T_FMT
                          " is larger than the configured limit"
                          " of %" APR_OFF_T_FMT, ctx->remaining, ctx->limit);
                return bail_out_on_error(ctx, f, HTTP_REQUEST_ENTITY_TOO_LARGE);
            }
        }

        
        if (ctx->state == BODY_NONE && f->r->proxyreq != PROXYREQ_RESPONSE) {
            e = apr_bucket_eos_create(f->c->bucket_alloc);
            APR_BRIGADE_INSERT_TAIL(b, e);
            ctx->eos_sent = 1;
            return APR_SUCCESS;
        }

        
        if ((ctx->state == BODY_CHUNK ||
            (ctx->state == BODY_LENGTH && ctx->remaining > 0)) &&
            f->r->expecting_100 && f->r->proto_num >= HTTP_VERSION(1,1) &&
            !(f->r->eos_sent || f->r->bytes_sent)) {
            if (!ap_is_HTTP_SUCCESS(f->r->status)) {
                ctx->state = BODY_NONE;
                ctx->eos_sent = 1;
            }
            else {
                char *tmp;
                int len;

                
                f->r->expecting_100 = 0;
                tmp = apr_pstrcat(f->r->pool, AP_SERVER_PROTOCOL " ",
                                  ap_get_status_line(HTTP_CONTINUE), CRLF CRLF,
                                  NULL);
                len = strlen(tmp);
                ap_xlate_proto_to_ascii(tmp, len);
                apr_brigade_cleanup(bb);
                e = apr_bucket_pool_create(tmp, len, f->r->pool,
                                           f->c->bucket_alloc);
                APR_BRIGADE_INSERT_HEAD(bb, e);
                e = apr_bucket_flush_create(f->c->bucket_alloc);
                APR_BRIGADE_INSERT_TAIL(bb, e);

                rv = ap_pass_brigade(f->c->output_filters, bb);
                if (rv != APR_SUCCESS) {
                    return AP_FILTER_ERROR;
                }
            }
        }
    }

    
    if (ctx->eos_sent) {
        e = apr_bucket_eos_create(f->c->bucket_alloc);
        APR_BRIGADE_INSERT_TAIL(b, e);
        return APR_SUCCESS;
    }

    do {
        apr_brigade_cleanup(b);
        again = 0; 

        
        switch (ctx->state) {
        case BODY_CHUNK:
        case BODY_CHUNK_PART:
        case BODY_CHUNK_EXT:
        case BODY_CHUNK_CR:
        case BODY_CHUNK_LF:
        case BODY_CHUNK_END:
        case BODY_CHUNK_END_LF: {

            rv = ap_get_brigade(f->next, b, AP_MODE_GETLINE, block, 0);

            
            if (block == APR_NONBLOCK_READ
                    && ((rv == APR_SUCCESS && APR_BRIGADE_EMPTY(b))
                            || (APR_STATUS_IS_EAGAIN(rv)))) {
                return APR_EAGAIN;
            }

            if (rv == APR_EOF) {
                return APR_INCOMPLETE;
            }

            if (rv != APR_SUCCESS) {
                return rv;
            }

            e = APR_BRIGADE_FIRST(b);
            while (e != APR_BRIGADE_SENTINEL(b)) {
                const char *buffer;
                apr_size_t len;

                if (!APR_BUCKET_IS_METADATA(e)) {
                    int parsing = 0;

                    rv = apr_bucket_read(e, &buffer, &len, APR_BLOCK_READ);

                    if (rv == APR_SUCCESS) {
                        parsing = 1;
                        rv = parse_chunk_size(ctx, buffer, len,
                                f->r->server->limit_req_fieldsize);
                    }
                    if (rv != APR_SUCCESS) {
                        ap_log_rerror(APLOG_MARK, APLOG_INFO, rv, f->r, APLOGNO(01590)
                                      "Error reading/parsing chunk %s ",
                                      (APR_ENOSPC == rv) ? "(overflow)" : "");
                        if (parsing) {
                            if (rv != APR_ENOSPC) {
                                http_error = HTTP_BAD_REQUEST;
                            }
                            return bail_out_on_error(ctx, f, http_error);
                        }
                        return rv;
                    }
                }

                apr_bucket_delete(e);
                e = APR_BRIGADE_FIRST(b);
            }
            again = 1; 

            if (ctx->state == BODY_CHUNK_TRAILER) {
                
                return read_chunked_trailers(ctx, f, b,
                            conf->merge_trailers == AP_MERGE_TRAILERS_ENABLE);
            }

            break;
        }
        case BODY_NONE:
        case BODY_LENGTH:
        case BODY_CHUNK_DATA: {

            
            if (ctx->state != BODY_NONE && ctx->remaining < readbytes) {
                readbytes = ctx->remaining;
            }
            if (readbytes > 0) {
                apr_off_t totalread;

                rv = ap_get_brigade(f->next, b, mode, block, readbytes);

                
                if (block == APR_NONBLOCK_READ
                        && ((rv == APR_SUCCESS && APR_BRIGADE_EMPTY(b))
                                || (APR_STATUS_IS_EAGAIN(rv)))) {
                    return APR_EAGAIN;
                }

                if (rv == APR_EOF && ctx->state != BODY_NONE
                        && ctx->remaining > 0) {
                    return APR_INCOMPLETE;
                }

                if (rv != APR_SUCCESS) {
                    return rv;
                }

                
                apr_brigade_length(b, 0, &totalread);

                
                AP_DEBUG_ASSERT(totalread >= 0);

                if (ctx->state != BODY_NONE) {
                    ctx->remaining -= totalread;
                    if (ctx->remaining > 0) {
                        e = APR_BRIGADE_LAST(b);
                        if (APR_BUCKET_IS_EOS(e)) {
                            apr_bucket_delete(e);
                            return APR_INCOMPLETE;
                        }
                    }
                    else if (ctx->state == BODY_CHUNK_DATA) {
                        
                        ctx->state = BODY_CHUNK_END;
                        ctx->chunk_used = 0;
                    }
                }

                
                if (ctx->limit) {
                    
                    ctx->limit_used += totalread;
                    if (ctx->limit < ctx->limit_used) {
                        ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, f->r,
                                      APLOGNO(01591) "Read content length of "
                                      "%" APR_OFF_T_FMT " is larger than the "
                                      "configured limit of %" APR_OFF_T_FMT,
                                      ctx->limit_used, ctx->limit);
                        return bail_out_on_error(ctx, f,
                                                 HTTP_REQUEST_ENTITY_TOO_LARGE);
                    }
                }
            }

            
            if (ctx->state == BODY_LENGTH && ctx->remaining == 0) {
                e = apr_bucket_eos_create(f->c->bucket_alloc);
                APR_BRIGADE_INSERT_TAIL(b, e);
                ctx->eos_sent = 1;
            }

            break;
        }
        case BODY_CHUNK_TRAILER: {

            rv = ap_get_brigade(f->next, b, mode, block, readbytes);

            
            if (block == APR_NONBLOCK_READ
                    && ((rv == APR_SUCCESS && APR_BRIGADE_EMPTY(b))
                            || (APR_STATUS_IS_EAGAIN(rv)))) {
                return APR_EAGAIN;
            }

            if (rv != APR_SUCCESS) {
                return rv;
            }

            break;
        }
        default: {
            
            ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, f->r, APLOGNO(02901)
                          "Unexpected body state (%i)", (int)ctx->state);
            return APR_EGENERAL;
        }
        }

    } while (again);

    return APR_SUCCESS;
}

typedef struct header_struct {
    apr_pool_t *pool;
    apr_bucket_brigade *bb;
} header_struct;


static int form_header_field(header_struct *h,
                             const char *fieldname, const char *fieldval)
{
#if APR_CHARSET_EBCDIC
    char *headfield;
    apr_size_t len;

    headfield = apr_pstrcat(h->pool, fieldname, ": ", fieldval, CRLF, NULL);
    len = strlen(headfield);

    ap_xlate_proto_to_ascii(headfield, len);
    apr_brigade_write(h->bb, NULL, NULL, headfield, len);
#else
    struct iovec vec[4];
    struct iovec *v = vec;
    v->iov_base = (void *)fieldname;
    v->iov_len = strlen(fieldname);
    v++;
    v->iov_base = ": ";
    v->iov_len = sizeof(": ") - 1;
    v++;
    v->iov_base = (void *)fieldval;
    v->iov_len = strlen(fieldval);
    v++;
    v->iov_base = CRLF;
    v->iov_len = sizeof(CRLF) - 1;
    apr_brigade_writev(h->bb, NULL, NULL, vec, 4);
#endif 
    return 1;
}


static int uniq_field_values(void *d, const char *key, const char *val)
{
    apr_array_header_t *values;
    char *start;
    char *e;
    char **strpp;
    int  i;

    values = (apr_array_header_t *)d;

    e = apr_pstrdup(values->pool, val);

    do {
        

        while (*e == ',' || apr_isspace(*e)) {
            ++e;
        }
        if (*e == '\0') {
            break;
        }
        start = e;
        while (*e != '\0' && *e != ',' && !apr_isspace(*e)) {
            ++e;
        }
        if (*e != '\0') {
            *e++ = '\0';
        }

        
        for (i = 0, strpp = (char **) values->elts; i < values->nelts;
             ++i, ++strpp) {
            if (*strpp && strcasecmp(*strpp, start) == 0) {
                break;
            }
        }
        if (i == values->nelts) {  
            *(char **)apr_array_push(values) = start;
        }
    } while (*e != '\0');

    return 1;
}


static void fixup_vary(request_rec *r)
{
    apr_array_header_t *varies;

    varies = apr_array_make(r->pool, 5, sizeof(char *));

    
    apr_table_do((int (*)(void *, const char *, const char *))uniq_field_values,
                 (void *) varies, r->headers_out, "Vary", NULL);

    

    if (varies->nelts > 0) {
        apr_table_setn(r->headers_out, "Vary",
                       apr_array_pstrcat(r->pool, varies, ','));
    }
}


static apr_status_t send_all_header_fields(header_struct *h,
                                           const request_rec *r)
{
    const apr_array_header_t *elts;
    const apr_table_entry_t *t_elt;
    const apr_table_entry_t *t_end;
    struct iovec *vec;
    struct iovec *vec_next;

    elts = apr_table_elts(r->headers_out);
    if (elts->nelts == 0) {
        return APR_SUCCESS;
    }
    t_elt = (const apr_table_entry_t *)(elts->elts);
    t_end = t_elt + elts->nelts;
    vec = (struct iovec *)apr_palloc(h->pool, 4 * elts->nelts *
                                     sizeof(struct iovec));
    vec_next = vec;

    
    do {
        vec_next->iov_base = (void*)(t_elt->key);
        vec_next->iov_len = strlen(t_elt->key);
        vec_next++;
        vec_next->iov_base = ": ";
        vec_next->iov_len = sizeof(": ") - 1;
        vec_next++;
        vec_next->iov_base = (void*)(t_elt->val);
        vec_next->iov_len = strlen(t_elt->val);
        vec_next++;
        vec_next->iov_base = CRLF;
        vec_next->iov_len = sizeof(CRLF) - 1;
        vec_next++;
        t_elt++;
    } while (t_elt < t_end);

    if (APLOGrtrace4(r)) {
        t_elt = (const apr_table_entry_t *)(elts->elts);
        do {
            ap_log_rerror(APLOG_MARK, APLOG_TRACE4, 0, r, "  %s: %s",
                          ap_escape_logitem(r->pool, t_elt->key),
                          ap_escape_logitem(r->pool, t_elt->val));
            t_elt++;
        } while (t_elt < t_end);
    }

#if APR_CHARSET_EBCDIC
    {
        apr_size_t len;
        char *tmp = apr_pstrcatv(r->pool, vec, vec_next - vec, &len);
        ap_xlate_proto_to_ascii(tmp, len);
        return apr_brigade_write(h->bb, NULL, NULL, tmp, len);
    }
#else
    return apr_brigade_writev(h->bb, NULL, NULL, vec, vec_next - vec);
#endif
}


static apr_status_t validate_status_line(request_rec *r)
{
    char *end;

    if (r->status_line) {
        int len = strlen(r->status_line);
        if (len < 3
            || apr_strtoi64(r->status_line, &end, 10) != r->status
            || (end - 3) != r->status_line
            || (len >= 4 && ! apr_isspace(r->status_line[3]))) {
            r->status_line = NULL;
            return APR_EGENERAL;
        }
        
        if (len == 3) {
            r->status_line = apr_pstrcat(r->pool, r->status_line, " ", NULL);
            return APR_EGENERAL;
        }
        return APR_SUCCESS;
    }
    return APR_EGENERAL;
}


static void basic_http_header_check(request_rec *r,
                                    const char **protocol)
{
    apr_status_t rv;

    if (r->assbackwards) {
        
        return;
    }

    rv = validate_status_line(r);

    if (!r->status_line) {
        r->status_line = ap_get_status_line(r->status);
    } else if (rv != APR_SUCCESS) {
        
        const char *tmp = ap_get_status_line(r->status);
        if (!strncmp(tmp, r->status_line, 3)) {
            r->status_line = tmp;
        }
    }

    
    if (r->proto_num > HTTP_VERSION(1,0)
        && apr_table_get(r->subprocess_env, "downgrade-1.0")) {
        r->proto_num = HTTP_VERSION(1,0);
    }

    
    if (r->proto_num == HTTP_VERSION(1,0)
        && apr_table_get(r->subprocess_env, "force-response-1.0")) {
        *protocol = "HTTP/1.0";
        r->connection->keepalive = AP_CONN_CLOSE;
    }
    else {
        *protocol = AP_SERVER_PROTOCOL;
    }

}


static void basic_http_header(request_rec *r, apr_bucket_brigade *bb,
                              const char *protocol)
{
    
	TRACE_FUNCTION_START();
char *date = NULL;
    const char *proxy_date = NULL;
    const char *server = NULL;
    TRACE_START();
    const char *us = ap_get_server_banner();
    TRACE_END(1);
    header_struct h;
    struct iovec vec[4];

    if (r->assbackwards) {
        
        TRACE_FUNCTION_END();
        return ;
    }

    

    vec[0].iov_base = (void *)protocol;
    TRACE_START();
    vec[0].iov_len  = strlen(protocol);
    TRACE_END(2);
    vec[1].iov_base = (void *)" ";
    TRACE_START();
    vec[1].iov_len  = sizeof(" ") - 1;
    TRACE_END(3);
    vec[2].iov_base = (void *)(r->status_line);
    TRACE_START();
    vec[2].iov_len  = strlen(r->status_line);
    TRACE_END(4);
    vec[3].iov_base = (void *)CRLF;
    TRACE_START();
    vec[3].iov_len  = sizeof(CRLF) - 1;
    TRACE_END(5);
#if APR_CHARSET_EBCDIC
    {
        char *tmp;
        apr_size_t len;
        TRACE_START();
        tmp = apr_pstrcatv(r->pool, vec, 4, &len);
        TRACE_END(6);
        TRACE_START();
        ap_xlate_proto_to_ascii(tmp, len);
        TRACE_END(7);
        TRACE_START();
        apr_brigade_write(bb, NULL, NULL, tmp, len);
        TRACE_END(8);
    }
#else
    TRACE_START();
    apr_brigade_writev(bb, NULL, NULL, vec, 4);
    TRACE_END(9);
#endif

    h.pool = r->pool;
    h.bb = bb;

    
    if (r->proxyreq != PROXYREQ_NONE) {
        TRACE_START();
        proxy_date = apr_table_get(r->headers_out, "Date");
        TRACE_END(10);
        if (!proxy_date) {
            
            TRACE_START();
            date = apr_palloc(r->pool, APR_RFC822_DATE_LEN);
            TRACE_END(11);
            TRACE_START();
            ap_recent_rfc822_date(date, r->request_time);
            TRACE_END(12);
        }
        TRACE_START();
        server = apr_table_get(r->headers_out, "Server");
        TRACE_END(13);
    }
    else {
        TRACE_START();
        date = apr_palloc(r->pool, APR_RFC822_DATE_LEN);
        TRACE_END(14);
        TRACE_START();
        ap_recent_rfc822_date(date, r->request_time);
        TRACE_END(15);
    }

    form_header_field(&h, "Date", proxy_date ? proxy_date : date );

    if (!server && *us)
        server = us;
    if (server)
        form_header_field(&h, "Server", server);

    if (TRACE_S_E( APLOGrtrace3(r) ,16)) {
        TRACE_START();
        ap_log_rerror(APLOG_MARK, APLOG_TRACE3, 0, r,
                      "Response sent with status %d%s",
                      r->status,
                      APLOGrtrace4(r) ? ", headers:" : "");
        TRACE_END(17);

        
        ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, "  Date: %s",
                      proxy_date ? proxy_date : date );
        if (server)
            ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, "  Server: %s",
                          server);
    }


    
    TRACE_START();
    apr_table_unset(r->headers_out, "Date");
    TRACE_END(18);        
    if (server) {
        TRACE_START();
        apr_table_unset(r->headers_out, "Server");
        TRACE_END(19);
    }


	TRACE_FUNCTION_END();
}

AP_DECLARE(void) ap_basic_http_header(request_rec *r, apr_bucket_brigade *bb)
{
    const char *protocol = NULL;

    basic_http_header_check(r, &protocol);
    basic_http_header(r, bb, protocol);
}

static void terminate_header(apr_bucket_brigade *bb)
{
    char crlf[] = CRLF;
    apr_size_t buflen;

    buflen = strlen(crlf);
    ap_xlate_proto_to_ascii(crlf, buflen);
    apr_brigade_write(bb, NULL, NULL, crlf, buflen);
}

AP_DECLARE_NONSTD(int) ap_send_http_trace(request_rec *r)
{
    core_server_config *conf;
    int rv;
    apr_bucket_brigade *bb;
    header_struct h;
    apr_bucket *b;
    int body;
    char *bodyread = NULL, *bodyoff;
    apr_size_t bodylen = 0;
    apr_size_t bodybuf;
    long res = -1; 

    if (r->method_number != M_TRACE) {
        return DECLINED;
    }

    
    while (r->prev) {
        r = r->prev;
    }
    conf = ap_get_core_module_config(r->server->module_config);

    if (conf->trace_enable == AP_TRACE_DISABLE) {
        apr_table_setn(r->notes, "error-notes",
                      "TRACE denied by server configuration");
        return HTTP_METHOD_NOT_ALLOWED;
    }

    if (conf->trace_enable == AP_TRACE_EXTENDED)
        
        body = REQUEST_CHUNKED_DECHUNK;
    else
        body = REQUEST_NO_BODY;

    if ((rv = ap_setup_client_block(r, body))) {
        if (rv == HTTP_REQUEST_ENTITY_TOO_LARGE)
            apr_table_setn(r->notes, "error-notes",
                          "TRACE with a request body is not allowed");
        return rv;
    }

    if (ap_should_client_block(r)) {

        if (r->remaining > 0) {
            if (r->remaining > 65536) {
                apr_table_setn(r->notes, "error-notes",
                       "Extended TRACE request bodies cannot exceed 64k\n");
                return HTTP_REQUEST_ENTITY_TOO_LARGE;
            }
            
            bodybuf = (apr_size_t)r->remaining + 32;
        }
        else {
            
            bodybuf = 73730;
        }

        bodyoff = bodyread = apr_palloc(r->pool, bodybuf);

        
        while ((!bodylen || bodybuf >= 32) &&
               (res = ap_get_client_block(r, bodyoff, bodybuf)) > 0) {
            bodylen += res;
            bodybuf -= res;
            bodyoff += res;
        }
        if (res > 0 && bodybuf < 32) {
            
            while (ap_get_client_block(r, bodyread, bodylen) > 0)
                ;
            apr_table_setn(r->notes, "error-notes",
                   "Extended TRACE request bodies cannot exceed 64k\n");
            return HTTP_REQUEST_ENTITY_TOO_LARGE;
        }

        if (res < 0) {
            return HTTP_BAD_REQUEST;
        }
    }

    ap_set_content_type(r, "message/http");

    

    bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
#if APR_CHARSET_EBCDIC
    {
        char *tmp;
        apr_size_t len;
        len = strlen(r->the_request);
        tmp = apr_pmemdup(r->pool, r->the_request, len);
        ap_xlate_proto_to_ascii(tmp, len);
        apr_brigade_putstrs(bb, NULL, NULL, tmp, CRLF_ASCII, NULL);
    }
#else
    apr_brigade_putstrs(bb, NULL, NULL, r->the_request, CRLF, NULL);
#endif
    h.pool = r->pool;
    h.bb = bb;
    apr_table_do((int (*) (void *, const char *, const char *))
                 form_header_field, (void *) &h, r->headers_in, NULL);
    apr_brigade_puts(bb, NULL, NULL, CRLF_ASCII);

    
    if (bodylen) {
        b = apr_bucket_pool_create(bodyread, bodylen,
                                   r->pool, bb->bucket_alloc);
        APR_BRIGADE_INSERT_TAIL(bb, b);
    }

    ap_pass_brigade(r->output_filters,  bb);

    return DONE;
}

typedef struct header_filter_ctx {
    int headers_sent;
} header_filter_ctx;

AP_CORE_DECLARE_NONSTD(apr_status_t) ap_http_header_filter(ap_filter_t *f,
                                                           apr_bucket_brigade *b)
{
    request_rec *r = f->r;
    conn_rec *c = r->connection;
    const char *clheader;
    const char *protocol = NULL;
    apr_bucket *e;
    apr_bucket_brigade *b2;
    header_struct h;
    header_filter_ctx *ctx = f->ctx;
    const char *ctype;
    ap_bucket_error *eb = NULL;

    AP_DEBUG_ASSERT(!r->main);

    if (r->header_only) {
        if (!ctx) {
            ctx = f->ctx = apr_pcalloc(r->pool, sizeof(header_filter_ctx));
        }
        else if (ctx->headers_sent) {
            apr_brigade_cleanup(b);
            return OK;
        }
    }

    for (e = APR_BRIGADE_FIRST(b);
         e != APR_BRIGADE_SENTINEL(b);
         e = APR_BUCKET_NEXT(e))
    {
        if (AP_BUCKET_IS_ERROR(e) && !eb) {
            eb = e->data;
            continue;
        }
        
        if (AP_BUCKET_IS_EOC(e)) {
            ap_remove_output_filter(f);
            return ap_pass_brigade(f->next, b);
        }
    }
    if (eb) {
        int status;

        status = eb->status;
        apr_brigade_cleanup(b);
        ap_die(status, r);
        return AP_FILTER_ERROR;
    }

    if (r->assbackwards) {
        r->sent_bodyct = 1;
        ap_remove_output_filter(f);
        return ap_pass_brigade(f->next, b);
    }

    
    if (!apr_is_empty_table(r->err_headers_out)) {
        r->headers_out = apr_table_overlay(r->pool, r->err_headers_out,
                                           r->headers_out);
    }

    
    if (apr_table_get(r->subprocess_env, "force-no-vary") != NULL) {
        apr_table_unset(r->headers_out, "Vary");
        r->proto_num = HTTP_VERSION(1,0);
        apr_table_setn(r->subprocess_env, "force-response-1.0", "1");
    }
    else {
        fixup_vary(r);
    }

    
    if (apr_table_get(r->notes, "no-etag") != NULL) {
        apr_table_unset(r->headers_out, "ETag");
    }

    
    basic_http_header_check(r, &protocol);
    ap_set_keepalive(r);

    if (r->chunked) {
        apr_table_mergen(r->headers_out, "Transfer-Encoding", "chunked");
        apr_table_unset(r->headers_out, "Content-Length");
    }

    ctype = ap_make_content_type(r, r->content_type);
    if (ctype) {
        apr_table_setn(r->headers_out, "Content-Type", ctype);
    }

    if (r->content_encoding) {
        apr_table_setn(r->headers_out, "Content-Encoding",
                       r->content_encoding);
    }

    if (!apr_is_empty_array(r->content_languages)) {
        int i;
        char *token;
        char **languages = (char **)(r->content_languages->elts);
        const char *field = apr_table_get(r->headers_out, "Content-Language");

        while (field && (token = ap_get_list_item(r->pool, &field)) != NULL) {
            for (i = 0; i < r->content_languages->nelts; ++i) {
                if (!strcasecmp(token, languages[i]))
                    break;
            }
            if (i == r->content_languages->nelts) {
                *((char **) apr_array_push(r->content_languages)) = token;
            }
        }

        field = apr_array_pstrcat(r->pool, r->content_languages, ',');
        apr_table_setn(r->headers_out, "Content-Language", field);
    }

    
    if (r->no_cache && !apr_table_get(r->headers_out, "Expires")) {
        char *date = apr_palloc(r->pool, APR_RFC822_DATE_LEN);
        ap_recent_rfc822_date(date, r->request_time);
        apr_table_addn(r->headers_out, "Expires", date);
    }

    
    if (r->header_only
        && (clheader = apr_table_get(r->headers_out, "Content-Length"))
        && !strcmp(clheader, "0")) {
        apr_table_unset(r->headers_out, "Content-Length");
    }

    b2 = apr_brigade_create(r->pool, c->bucket_alloc);
    basic_http_header(r, b2, protocol);

    h.pool = r->pool;
    h.bb = b2;

    if (r->status == HTTP_NOT_MODIFIED) {
        apr_table_do((int (*)(void *, const char *, const char *)) form_header_field,
                     (void *) &h, r->headers_out,
                     "Connection",
                     "Keep-Alive",
                     "ETag",
                     "Content-Location",
                     "Expires",
                     "Cache-Control",
                     "Vary",
                     "Warning",
                     "WWW-Authenticate",
                     "Proxy-Authenticate",
                     "Set-Cookie",
                     "Set-Cookie2",
                     NULL);
    }
    else {
        send_all_header_fields(&h, r);
    }

    terminate_header(b2);

    PATH_INC();
    ap_pass_brigade(f->next, b2);
    PATH_DEC();

    if (r->header_only) {
        apr_brigade_cleanup(b);
        ctx->headers_sent = 1;
        return OK;
    }

    r->sent_bodyct = 1;         

    if (r->chunked) {
        
        ap_add_output_filter("CHUNK", NULL, r, r->connection);
    }

    
    ap_remove_output_filter(f);
    return ap_pass_brigade(f->next, b);
}


AP_DECLARE(int) ap_map_http_request_error(apr_status_t rv, int status)
{
    switch (rv) {
    case AP_FILTER_ERROR: {
        return AP_FILTER_ERROR;
    }
    case APR_ENOSPC: {
        return HTTP_REQUEST_ENTITY_TOO_LARGE;
    }
    case APR_ENOTIMPL: {
        return HTTP_NOT_IMPLEMENTED;
    }
    case APR_ETIMEDOUT: {
        return HTTP_REQUEST_TIME_OUT;
    }
    default: {
        return status;
    }
    }
}


AP_DECLARE(int) ap_discard_request_body(request_rec *r)
{
    apr_bucket_brigade *bb;
    int seen_eos;
    apr_status_t rv;

    
    if (r->main || r->connection->keepalive == AP_CONN_CLOSE ||
        ap_status_drops_connection(r->status)) {
        return OK;
    }

    bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
    seen_eos = 0;
    do {
        apr_bucket *bucket;

        rv = ap_get_brigade(r->input_filters, bb, AP_MODE_READBYTES,
                            APR_BLOCK_READ, HUGE_STRING_LEN);

        if (rv != APR_SUCCESS) {
            apr_brigade_destroy(bb);
            return ap_map_http_request_error(rv, HTTP_BAD_REQUEST);
        }

        for (bucket = APR_BRIGADE_FIRST(bb);
             bucket != APR_BRIGADE_SENTINEL(bb);
             bucket = APR_BUCKET_NEXT(bucket))
        {
            const char *data;
            apr_size_t len;

            if (APR_BUCKET_IS_EOS(bucket)) {
                seen_eos = 1;
                break;
            }

            
            if (bucket->length == 0) {
                continue;
            }

            
            rv = apr_bucket_read(bucket, &data, &len, APR_BLOCK_READ);
            if (rv != APR_SUCCESS) {
                apr_brigade_destroy(bb);
                return HTTP_BAD_REQUEST;
            }
        }
        apr_brigade_cleanup(bb);
    } while (!seen_eos);

    return OK;
}



AP_DECLARE(int) ap_setup_client_block(request_rec *r, int read_policy)
{
    const char *tenc = apr_table_get(r->headers_in, "Transfer-Encoding");
    const char *lenp = apr_table_get(r->headers_in, "Content-Length");

    r->read_body = read_policy;
    r->read_chunked = 0;
    r->remaining = 0;

    if (tenc) {
        if (strcasecmp(tenc, "chunked")) {
            ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(01592)
                          "Unknown Transfer-Encoding %s", tenc);
            return HTTP_NOT_IMPLEMENTED;
        }
        if (r->read_body == REQUEST_CHUNKED_ERROR) {
            ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(01593)
                          "chunked Transfer-Encoding forbidden: %s", r->uri);
            return (lenp) ? HTTP_BAD_REQUEST : HTTP_LENGTH_REQUIRED;
        }

        r->read_chunked = 1;
    }
    else if (lenp) {
        char *endstr;

        if (apr_strtoff(&r->remaining, lenp, &endstr, 10)
            || *endstr || r->remaining < 0) {
            r->remaining = 0;
            ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(01594)
                          "Invalid Content-Length");
            return HTTP_BAD_REQUEST;
        }
    }

    if ((r->read_body == REQUEST_NO_BODY)
        && (r->read_chunked || (r->remaining > 0))) {
        ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(01595)
                      "%s with body is not allowed for %s", r->method, r->uri);
        return HTTP_REQUEST_ENTITY_TOO_LARGE;
    }

#ifdef AP_DEBUG
    {
        
        core_request_config *req_cfg =
            (core_request_config *)ap_get_core_module_config(r->request_config);
        AP_DEBUG_ASSERT(APR_BRIGADE_EMPTY(req_cfg->bb));
    }
#endif

    return OK;
}

AP_DECLARE(int) ap_should_client_block(request_rec *r)
{
    

    if (r->read_length || (!r->read_chunked && (r->remaining <= 0))) {
        return 0;
    }

    return 1;
}


AP_DECLARE(long) ap_get_client_block(request_rec *r, char *buffer,
                                     apr_size_t bufsiz)
{
    apr_status_t rv;
    apr_bucket_brigade *bb;

    if (r->remaining < 0 || (!r->read_chunked && r->remaining == 0)) {
        return 0;
    }

    bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
    if (bb == NULL) {
        r->connection->keepalive = AP_CONN_CLOSE;
        return -1;
    }

    rv = ap_get_brigade(r->input_filters, bb, AP_MODE_READBYTES,
                        APR_BLOCK_READ, bufsiz);

    
    if (rv == AP_FILTER_ERROR) {
        
        apr_brigade_destroy(bb);
        return -1;
    }
    if (rv != APR_SUCCESS) {
        
        r->connection->keepalive = AP_CONN_CLOSE;
        apr_brigade_destroy(bb);
        return -1;
    }

    
    AP_DEBUG_ASSERT(!APR_BRIGADE_EMPTY(bb));

    
    if (APR_BUCKET_IS_EOS(APR_BRIGADE_LAST(bb))) {
        if (r->read_chunked) {
            r->remaining = -1;
        }
        else {
            r->remaining = 0;
        }
    }

    rv = apr_brigade_flatten(bb, buffer, &bufsiz);
    if (rv != APR_SUCCESS) {
        apr_brigade_destroy(bb);
        return -1;
    }

    
    r->read_length += bufsiz;

    apr_brigade_destroy(bb);
    return bufsiz;
}


typedef struct {
    int seen_eoc;
} outerror_filter_ctx_t;


apr_status_t ap_http_outerror_filter(ap_filter_t *f,
                                     apr_bucket_brigade *b)
{
    request_rec *r = f->r;
    outerror_filter_ctx_t *ctx = (outerror_filter_ctx_t *)(f->ctx);
    apr_bucket *e;

    
    if (!ctx) {
        ctx = apr_pcalloc(r->pool, sizeof(outerror_filter_ctx_t));
        f->ctx = ctx;
    }
    for (e = APR_BRIGADE_FIRST(b);
         e != APR_BRIGADE_SENTINEL(b);
         e = APR_BUCKET_NEXT(e))
    {
        if (AP_BUCKET_IS_ERROR(e)) {
            
            if (((ap_bucket_error *)(e->data))->status == HTTP_BAD_GATEWAY) {
                
                r->connection->keepalive = AP_CONN_CLOSE;
            }
            continue;
        }
        
        if (AP_BUCKET_IS_EOC(e)) {
            ctx->seen_eoc = 1;
        }
    }
    
    if (ctx->seen_eoc) {
        for (e = APR_BRIGADE_FIRST(b);
             e != APR_BRIGADE_SENTINEL(b);
             e = APR_BUCKET_NEXT(e))
        {
            if (!APR_BUCKET_IS_METADATA(e)) {
                APR_BUCKET_REMOVE(e);
            }
        }
    }

    apr_status_t result = ap_pass_brigade(f->next, b);
    return result;
}
