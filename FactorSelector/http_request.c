



#include "apr_strings.h"
#include "apr_file_io.h"
#include "apr_fnmatch.h"

#define APR_WANT_STRFUNC
#include "apr_want.h"

#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_log.h"
#include "http_main.h"
#include "util_filter.h"
#include "util_charset.h"
#include "scoreboard.h"

#include "mod_core.h"

#include "trace_tool.h"

#if APR_HAVE_STDARG_H
#include <stdarg.h>
#endif

APLOG_USE_MODULE(http);




static void update_r_in_filters(ap_filter_t *f,
                                request_rec *from,
                                request_rec *to)
{
    while (f) {
        if (f->r == from) {
            f->r = to;
        }
        f = f->next;
    }
}

static void ap_die_r(int type, request_rec *r, int recursive_error)
{
    char *custom_response;
    request_rec *r_1st_err = r;

    if (type == OK || type == DONE) {
        ap_finalize_request_protocol(r);
        return;
    }

    if (!ap_is_HTTP_VALID_RESPONSE(type)) {
        ap_filter_t *next;

        
        next = r->output_filters;
        while (next && (next->frec != ap_http_header_filter_handle)) {
               next = next->next;
        }

        
        if (next) {
            if (type != AP_FILTER_ERROR) {
                ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01579)
                              "Invalid response status %i", type);
            }
            else {
                ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02831)
                              "Response from AP_FILTER_ERROR");
            }
            type = HTTP_INTERNAL_SERVER_ERROR;
        }
        else {
            return;
        }
    }

    
    if (recursive_error != HTTP_OK) {
        while (r_1st_err->prev && (r_1st_err->prev->status != HTTP_OK))
            r_1st_err = r_1st_err->prev;  

        if (r_1st_err != r) {
            
            update_r_in_filters(r_1st_err->proto_output_filters, r, r_1st_err);
            update_r_in_filters(r_1st_err->input_filters, r, r_1st_err);
        }

        custom_response = NULL; 
    }
    else {
        int error_index = ap_index_of_response(type);
        custom_response = ap_response_code_string(r, error_index);
        recursive_error = 0;
    }

    r->status = type;

    
    if (HTTP_UNAUTHORIZED == r->status && PROXYREQ_PROXY == r->proxyreq) {
        r->status = HTTP_PROXY_AUTHENTICATION_REQUIRED;
    }

    
    if (ap_status_drops_connection(r->status)) {
        r->connection->keepalive = AP_CONN_CLOSE;
    }

    

    if (custom_response && custom_response[0] != '"') {

        if (ap_is_url(custom_response)) {
            
            r->status = HTTP_MOVED_TEMPORARILY;
            apr_table_setn(r->headers_out, "Location", custom_response);
        }
        else if (custom_response[0] == '/') {
            const char *error_notes;
            r->no_local_copy = 1;       
            
            apr_table_setn(r->subprocess_env, "REQUEST_METHOD", r->method);

            
            if ((error_notes = apr_table_get(r->notes,
                                             "error-notes")) != NULL) {
                apr_table_setn(r->subprocess_env, "ERROR_NOTES", error_notes);
            }
            r->method = "GET";
            r->method_number = M_GET;
            ap_internal_redirect(custom_response, r);
            return;
        }
        else {
            
            recursive_error = HTTP_INTERNAL_SERVER_ERROR;
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01580)
                        "Invalid error redirection directive: %s",
                        custom_response);
        }
    }
    ap_send_error_response(r_1st_err, recursive_error);
}

AP_DECLARE(void) ap_die(int type, request_rec *r)
{
    ap_die_r(type, r, r->status);
}

static void check_pipeline(conn_rec *c, apr_bucket_brigade *bb)
{
    apr_status_t rv;
    int num_blank_lines = DEFAULT_LIMIT_BLANK_LINES;
    ap_input_mode_t mode = AP_MODE_SPECULATIVE;
    apr_size_t cr = 0;
    char buf[2];

    c->data_in_input_filters = 0;
    while (c->keepalive != AP_CONN_CLOSE && !c->aborted) {
        apr_size_t len = cr + 1;

        apr_brigade_cleanup(bb);
        rv = ap_get_brigade(c->input_filters, bb, mode,
                            APR_NONBLOCK_READ, len);
        if (rv != APR_SUCCESS || APR_BRIGADE_EMPTY(bb)) {
            
            if (mode == AP_MODE_READBYTES) {
                
                ap_log_cerror(APLOG_MARK, APLOG_ERR, rv, c, APLOGNO(02967)
                              "Can't consume pipelined empty lines");
                c->keepalive = AP_CONN_CLOSE;
            }
            break;
        }

        
        rv = apr_brigade_flatten(bb, buf, &len);
        if (rv != APR_SUCCESS || len != cr + 1) {
            int log_level;
            if (mode == AP_MODE_READBYTES) {
                
                c->keepalive = AP_CONN_CLOSE;
                log_level = APLOG_ERR;
            }
            else {
                
                c->data_in_input_filters = 1;
                log_level = APLOG_DEBUG;
            }
            ap_log_cerror(APLOG_MARK, log_level, rv, c, APLOGNO(02968)
                          "Can't check pipelined data");
            break;
        }

        if (mode == AP_MODE_READBYTES) {
            mode = AP_MODE_SPECULATIVE;
            cr = 0;
        }
        else if (cr) {
            AP_DEBUG_ASSERT(len == 2 && buf[0] == APR_ASCII_CR);
            if (buf[1] == APR_ASCII_LF) {
                mode = AP_MODE_READBYTES;
                num_blank_lines--;
            }
            else {
                c->data_in_input_filters = 1;
                break;
            }
        }
        else {
            if (buf[0] == APR_ASCII_LF) {
                mode = AP_MODE_READBYTES;
                num_blank_lines--;
            }
            else if (buf[0] == APR_ASCII_CR) {
                cr = 1;
            }
            else {
                c->data_in_input_filters = 1;
                break;
            }
        }
        
        if (num_blank_lines < 0) {
            c->keepalive = AP_CONN_CLOSE;
        }
    }
}


AP_DECLARE(void) ap_process_request_after_handler(request_rec *r)
{
    apr_bucket_brigade *bb;
    apr_bucket *b;
    conn_rec *c = r->connection;

    
    bb = apr_brigade_create(c->pool, c->bucket_alloc);
    b = ap_bucket_eor_create(c->bucket_alloc, r);
    APR_BRIGADE_INSERT_HEAD(bb, b);

    ap_pass_brigade(c->output_filters, bb);
    
    
    apr_brigade_cleanup(bb);

    

    check_pipeline(c, bb);
    apr_brigade_destroy(bb);
    if (c->cs)
        c->cs->state = (c->aborted) ? CONN_STATE_LINGER
                                    : CONN_STATE_WRITE_COMPLETION;
    AP_PROCESS_REQUEST_RETURN((uintptr_t)r, r->uri, r->status);
    if (ap_extended_status) {
        ap_time_process_request(c->sbh, STOP_PREQUEST);
    }
}

void ap_process_async_request(request_rec *r)
{
    
	TRACE_FUNCTION_START();
conn_rec *c = r->connection;
    int access_status;

    
    TRACE_START();
    AP_PROCESS_REQUEST_ENTRY((uintptr_t)r, r->uri);
    TRACE_END(1);
    if (ap_extended_status) {
        TRACE_START();
        ap_time_process_request(r->connection->sbh, START_PREQUEST);
        TRACE_END(2);
    }

    if (TRACE_S_E( APLOGrtrace4(r) ,3)) {
        int i;
        TRACE_START();
        const apr_array_header_t *t_h = apr_table_elts(r->headers_in);
        TRACE_END(4);
        const apr_table_entry_t *t_elt = (apr_table_entry_t *)t_h->elts;
        TRACE_START();
        ap_log_rerror(APLOG_MARK, APLOG_TRACE4, 0, r,
                      "Headers received from client:");
        TRACE_END(5);
        for (i = 0; i < t_h->nelts ; i++, t_elt++) {
            TRACE_START();
            ap_log_rerror(APLOG_MARK, APLOG_TRACE4, 0, r, "  %s: %s",
                          ap_escape_logitem(r->pool, t_elt->key),
                          ap_escape_logitem(r->pool, t_elt->val));
            TRACE_END(6);
        }
    }

#if APR_HAS_THREADS
    TRACE_START();
    apr_thread_mutex_create(&r->invoke_mtx, APR_THREAD_MUTEX_DEFAULT, r->pool);
    TRACE_END(7);
    TRACE_START();
    apr_thread_mutex_lock(r->invoke_mtx);
    TRACE_END(8);
#endif
    TRACE_START();
    access_status = ap_run_quick_handler(r, 0);
    TRACE_END(9);  
    if (access_status == DECLINED) {
        TRACE_START();
        access_status = ap_process_request_internal(r);
        TRACE_END(10);
        if (access_status == OK) {
            TRACE_START();
            access_status = ap_invoke_handler(r);
            TRACE_END(11);
        }
    }

    if (access_status == SUSPENDED) {
        
        TRACE_START();
        AP_PROCESS_REQUEST_RETURN((uintptr_t)r, r->uri, access_status);
        TRACE_END(12);
        if (ap_extended_status) {
            TRACE_START();
            ap_time_process_request(c->sbh, STOP_PREQUEST);
            TRACE_END(13);
        }
        if (c->cs)
            c->cs->state = CONN_STATE_SUSPENDED;
#if APR_HAS_THREADS
        TRACE_START();
        apr_thread_mutex_unlock(r->invoke_mtx);
        TRACE_END(14);
#endif
        TRACE_FUNCTION_END();
        return ;
    }
#if APR_HAS_THREADS
    TRACE_START();
    apr_thread_mutex_unlock(r->invoke_mtx);
    TRACE_END(15);
#endif

    TRACE_START();
    ap_die_r(access_status, r, HTTP_OK);
    TRACE_END(16);

    TRACE_START();
    ap_process_request_after_handler(r);
    TRACE_END(17);


	TRACE_FUNCTION_END();
}

AP_DECLARE(void) ap_process_request(request_rec *r)
{
    apr_bucket_brigade *bb;
    apr_bucket *b;
    conn_rec *c = r->connection;
    apr_status_t rv;

    ap_process_async_request(r);

    if (!c->data_in_input_filters) {
        bb = apr_brigade_create(c->pool, c->bucket_alloc);
        b = apr_bucket_flush_create(c->bucket_alloc);
        APR_BRIGADE_INSERT_HEAD(bb, b);
        rv = ap_pass_brigade(c->output_filters, bb);
        if (APR_STATUS_IS_TIMEUP(rv)) {
            
            ap_log_rerror(APLOG_MARK, APLOG_INFO, rv, r, APLOGNO(01581)
                          "Timeout while writing data for URI %s to the"
                          " client", r->unparsed_uri);
        }
    }
    if (ap_extended_status) {
        ap_time_process_request(c->sbh, STOP_PREQUEST);
    }
}

static apr_table_t *rename_original_env(apr_pool_t *p, apr_table_t *t)
{
    const apr_array_header_t *env_arr = apr_table_elts(t);
    const apr_table_entry_t *elts = (const apr_table_entry_t *) env_arr->elts;
    apr_table_t *new = apr_table_make(p, env_arr->nalloc);
    int i;

    for (i = 0; i < env_arr->nelts; ++i) {
        if (!elts[i].key)
            continue;
        apr_table_setn(new, apr_pstrcat(p, "REDIRECT_", elts[i].key, NULL),
                  elts[i].val);
    }

    return new;
}

static request_rec *internal_internal_redirect(const char *new_uri,
                                               request_rec *r) {
    int access_status;
    request_rec *new;

    if (ap_is_recursion_limit_exceeded(r)) {
        ap_die(HTTP_INTERNAL_SERVER_ERROR, r);
        return NULL;
    }

    new = (request_rec *) apr_pcalloc(r->pool, sizeof(request_rec));

    new->connection = r->connection;
    new->server     = r->server;
    new->pool       = r->pool;

    

    new->method          = r->method;
    new->method_number   = r->method_number;
    new->allowed_methods = ap_make_method_list(new->pool, 2);
    ap_parse_uri(new, new_uri);
    new->parsed_uri.port_str = r->parsed_uri.port_str;
    new->parsed_uri.port = r->parsed_uri.port;

    new->request_config = ap_create_request_config(r->pool);

    new->per_dir_config = r->server->lookup_defaults;

    new->prev = r;
    r->next   = new;

    new->useragent_addr = r->useragent_addr;
    new->useragent_ip = r->useragent_ip;

    
    ap_run_create_request(new);

    

    new->the_request = r->the_request;

    new->allowed         = r->allowed;

    new->status          = r->status;
    new->assbackwards    = r->assbackwards;
    new->header_only     = r->header_only;
    new->protocol        = r->protocol;
    new->proto_num       = r->proto_num;
    new->hostname        = r->hostname;
    new->request_time    = r->request_time;
    new->main            = r->main;

    new->headers_in      = r->headers_in;
    new->trailers_in     = r->trailers_in;
    new->headers_out     = apr_table_make(r->pool, 12);
    if (ap_is_HTTP_REDIRECT(new->status)) {
        const char *location = apr_table_get(r->headers_out, "Location");
        if (location)
            apr_table_setn(new->headers_out, "Location", location);
    }
    new->err_headers_out = r->err_headers_out;
    new->trailers_out    = apr_table_make(r->pool, 5);
    new->subprocess_env  = rename_original_env(r->pool, r->subprocess_env);
    new->notes           = apr_table_make(r->pool, 5);

    new->htaccess        = r->htaccess;
    new->no_cache        = r->no_cache;
    new->expecting_100   = r->expecting_100;
    new->no_local_copy   = r->no_local_copy;
    new->read_length     = r->read_length;     
    new->vlist_validator = r->vlist_validator;

    new->proto_output_filters  = r->proto_output_filters;
    new->proto_input_filters   = r->proto_input_filters;

    new->input_filters   = new->proto_input_filters;

    if (new->main) {
        ap_filter_t *f, *nextf;

        
        new->output_filters = r->output_filters;

        f = new->output_filters;
        do {
            nextf = f->next;

            if (f->r == r && f->frec != ap_subreq_core_filter_handle) {
                ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01582)
                              "dropping filter '%s' in internal redirect from %s to %s",
                              f->frec->name, r->unparsed_uri, new_uri);

                
                f->r = new;
                ap_remove_output_filter(f);
            }

            f = nextf;

            
        } while (f && f != new->proto_output_filters);
    }
    else {
        
        new->output_filters  = new->proto_output_filters;
    }

    update_r_in_filters(new->input_filters, r, new);
    update_r_in_filters(new->output_filters, r, new);

    apr_table_setn(new->subprocess_env, "REDIRECT_STATUS",
                   apr_itoa(r->pool, r->status));

    
    new->used_path_info = AP_REQ_DEFAULT_PATH_INFO;

#if APR_HAS_THREADS
    new->invoke_mtx = r->invoke_mtx;
#endif

    
    if ((access_status = ap_run_post_read_request(new))) {
        ap_die(access_status, new);
        return NULL;
    }

    return new;
}


AP_DECLARE(void) ap_internal_fast_redirect(request_rec *rr, request_rec *r)
{
    
    apr_pool_join(r->pool, rr->pool);
    r->proxyreq = rr->proxyreq;
    r->no_cache = (r->no_cache && rr->no_cache);
    r->no_local_copy = (r->no_local_copy && rr->no_local_copy);
    r->mtime = rr->mtime;
    r->uri = rr->uri;
    r->filename = rr->filename;
    r->canonical_filename = rr->canonical_filename;
    r->path_info = rr->path_info;
    r->args = rr->args;
    r->finfo = rr->finfo;
    r->handler = rr->handler;
    ap_set_content_type(r, rr->content_type);
    r->content_encoding = rr->content_encoding;
    r->content_languages = rr->content_languages;
    r->per_dir_config = rr->per_dir_config;
    
    r->notes = apr_table_overlay(r->pool, rr->notes, r->notes);
    r->headers_out = apr_table_overlay(r->pool, rr->headers_out,
                                       r->headers_out);
    r->err_headers_out = apr_table_overlay(r->pool, rr->err_headers_out,
                                           r->err_headers_out);
    r->trailers_out = apr_table_overlay(r->pool, rr->trailers_out,
                                           r->trailers_out);
    r->subprocess_env = apr_table_overlay(r->pool, rr->subprocess_env,
                                          r->subprocess_env);

    r->output_filters = rr->output_filters;
    r->input_filters = rr->input_filters;

    
    update_r_in_filters(r->input_filters, rr, r);
    update_r_in_filters(r->output_filters, rr, r);

    if (r->main) {
        ap_add_output_filter_handle(ap_subreq_core_filter_handle,
                                    NULL, r, r->connection);
    }
    else {
        
        ap_filter_t *next;

        next = r->output_filters;
        while (next && (next->frec != ap_subreq_core_filter_handle)
               && (next != r->proto_output_filters)) {
                next = next->next;
        }
        if (next && (next->frec == ap_subreq_core_filter_handle)) {
            ap_remove_output_filter(next);
        }
    }
}

AP_DECLARE(void) ap_internal_redirect(const char *new_uri, request_rec *r)
{
    int access_status;
    request_rec *new = internal_internal_redirect(new_uri, r);

    AP_INTERNAL_REDIRECT(r->uri, new_uri);

    
    if (!new) {
        return;
    }

    access_status = ap_run_quick_handler(new, 0);  
    if (access_status == DECLINED) {
        access_status = ap_process_request_internal(new);
        if (access_status == OK) {
            access_status = ap_invoke_handler(new);
        }
    }
    ap_die(access_status, new);
}


AP_DECLARE(void) ap_internal_redirect_handler(const char *new_uri, request_rec *r)
{
    int access_status;
    request_rec *new = internal_internal_redirect(new_uri, r);

    
    if (!new) {
        return;
    }

    if (r->handler)
        ap_set_content_type(new, r->content_type);
    access_status = ap_process_request_internal(new);
    if (access_status == OK) {
        access_status = ap_invoke_handler(new);
    }
    ap_die(access_status, new);
}

AP_DECLARE(void) ap_allow_methods(request_rec *r, int reset, ...)
{
    const char *method;
    va_list methods;

    
    if (reset) {
        ap_clear_method_list(r->allowed_methods);
    }

    va_start(methods, reset);
    while ((method = va_arg(methods, const char *)) != NULL) {
        ap_method_list_add(r->allowed_methods, method);
    }
    va_end(methods);
}

AP_DECLARE(void) ap_allow_standard_methods(request_rec *r, int reset, ...)
{
    int method;
    va_list methods;
    apr_int64_t mask;

    
    if (reset) {
        ap_clear_method_list(r->allowed_methods);
    }

    mask = 0;
    va_start(methods, reset);
    while ((method = va_arg(methods, int)) != -1) {
        mask |= (AP_METHOD_BIT << method);
    }
    va_end(methods);

    r->allowed_methods->method_mask |= mask;
}
