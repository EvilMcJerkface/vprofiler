



#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <ngx_trace.h>


static ngx_int_t ngx_http_static_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_static_init(ngx_conf_t *cf);


ngx_http_module_t  ngx_http_static_module_ctx = {
    NULL,                                  
    ngx_http_static_init,                  

    NULL,                                  
    NULL,                                  

    NULL,                                  
    NULL,                                  

    NULL,                                  
    NULL                                   
};


ngx_module_t  ngx_http_static_module = {
    NGX_MODULE_V1,
    &ngx_http_static_module_ctx,           
    NULL,                                  
    NGX_HTTP_MODULE,                       
    NULL,                                  
    NULL,                                  
    NULL,                                  
    NULL,                                  
    NULL,                                  
    NULL,                                  
    NULL,                                  
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_static_handler(ngx_http_request_t *r)
{
    
	TRACE_FUNCTION_START();
	ngx_int_t
 resultReturn;
u_char                    *last, *location;
    size_t                     root, len;
    ngx_str_t                  path;
    ngx_int_t                  rc;
    ngx_uint_t                 level;
    ngx_log_t                 *log;
    ngx_buf_t                 *b;
    ngx_chain_t                out;
    ngx_open_file_info_t       of;
    ngx_http_core_loc_conf_t  *clcf;

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD|NGX_HTTP_POST))) {
        TRACE_FUNCTION_END();
        return NGX_HTTP_NOT_ALLOWED;
    }

    if (r->uri.data[r->uri.len - 1] == '/') {
        TRACE_FUNCTION_END();
        return NGX_DECLINED;
    }

    log = r->connection->log;

    

    TRACE_START();
    last = ngx_http_map_uri_to_path(r, &path, &root, 0);
    TRACE_END(1);
    if (last == NULL) {
        TRACE_FUNCTION_END();
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    path.len = last - path.data;

    TRACE_START();
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "http filename: \"%s\"", path.data);
    TRACE_END(2);

    TRACE_START();
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    TRACE_END(3);

    TRACE_START();
    ngx_memzero(&of, sizeof(ngx_open_file_info_t));
    TRACE_END(4);

    of.read_ahead = clcf->read_ahead;
    of.directio = clcf->directio;
    of.valid = clcf->open_file_cache_valid;
    of.min_uses = clcf->open_file_cache_min_uses;
    of.errors = clcf->open_file_cache_errors;
    of.events = clcf->open_file_cache_events;

    if (TRACE_S_E( ngx_http_set_disable_symlinks(r, clcf, &path, &of) != NGX_OK ,5)) {
        TRACE_FUNCTION_END();
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (TRACE_S_E( ngx_open_cached_file(clcf->open_file_cache, &path, &of, r->pool)
        != NGX_OK ,6))
    {
        switch (of.err) {

        case 0:
            TRACE_FUNCTION_END();
            return NGX_HTTP_INTERNAL_SERVER_ERROR;

        case NGX_ENOENT:
        case NGX_ENOTDIR:
        case NGX_ENAMETOOLONG:

            level = NGX_LOG_ERR;
            rc = NGX_HTTP_NOT_FOUND;
            break;

        case NGX_EACCES:
#if (NGX_HAVE_OPENAT)
        case NGX_EMLINK:
        case NGX_ELOOP:
#endif

            level = NGX_LOG_ERR;
            rc = NGX_HTTP_FORBIDDEN;
            break;

        default:

            level = NGX_LOG_CRIT;
            rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
            break;
        }

        if (rc != NGX_HTTP_NOT_FOUND || clcf->log_not_found) {
            TRACE_START();
            ngx_log_error(level, log, of.err,
                          "%s \"%s\" failed", of.failed, path.data);
            TRACE_END(7);
        }

        TRACE_FUNCTION_END();
        return rc;
    }

    r->root_tested = !r->error_page;

    TRACE_START();
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "http static fd: %d", of.fd);
    TRACE_END(8);

    if (of.is_dir) {

        TRACE_START();
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0, "http dir");
        TRACE_END(9);

        TRACE_START();
        ngx_http_clear_location(r);
        TRACE_END(10);

        TRACE_START();
        r->headers_out.location = ngx_palloc(r->pool, sizeof(ngx_table_elt_t));
        TRACE_END(11);
        if (r->headers_out.location == NULL) {
            TRACE_FUNCTION_END();
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        len = r->uri.len + 1;

        if (!clcf->alias && clcf->root_lengths == NULL && r->args.len == 0) {
            location = path.data + clcf->root.len;

            *last = '/';

        } else {
            if (r->args.len) {
                len += r->args.len + 1;
            }

            TRACE_START();
            location = ngx_pnalloc(r->pool, len);
            TRACE_END(12);
            if (location == NULL) {
                TRACE_FUNCTION_END();
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            TRACE_START();
            last = ngx_copy(location, r->uri.data, r->uri.len);
            TRACE_END(13);

            *last = '/';

            if (r->args.len) {
                *++last = '?';
                TRACE_START();
                ngx_memcpy(++last, r->args.data, r->args.len);
                TRACE_END(14);
            }
        }

        

        r->headers_out.location->value.len = len;
        r->headers_out.location->value.data = location;

        TRACE_FUNCTION_END();
        return NGX_HTTP_MOVED_PERMANENTLY;
    }

#if !(NGX_WIN32) 

    if (!of.is_file) {
        TRACE_START();
        ngx_log_error(NGX_LOG_CRIT, log, 0,
                      "\"%s\" is not a regular file", path.data);
        TRACE_END(15);

        TRACE_FUNCTION_END();
        return NGX_HTTP_NOT_FOUND;
    }

#endif

    if (r->method == NGX_HTTP_POST) {
        TRACE_FUNCTION_END();
        return NGX_HTTP_NOT_ALLOWED;
    }

    TRACE_START();
    rc = ngx_http_discard_request_body(r);
    TRACE_END(16);

    if (rc != NGX_OK) {
        TRACE_FUNCTION_END();
        return rc;
    }

    log->action = "sending response to client";

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = of.size;
    r->headers_out.last_modified_time = of.mtime;

    if (TRACE_S_E( ngx_http_set_etag(r) != NGX_OK ,17)) {
        TRACE_FUNCTION_END();
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (TRACE_S_E( ngx_http_set_content_type(r) != NGX_OK ,18)) {
        TRACE_FUNCTION_END();
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (r != r->main && of.size == 0) {
        TRACE_START();
        resultReturn = ngx_http_send_header(r);
        TRACE_END(19);

        TRACE_FUNCTION_END();
        return resultReturn;
    }

    r->allow_ranges = 1;

    

    TRACE_START();
    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    TRACE_END(20);
    if (b == NULL) {
        TRACE_FUNCTION_END();
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    TRACE_START();
    b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
    TRACE_END(21);
    if (b->file == NULL) {
        TRACE_FUNCTION_END();
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    TRACE_START();
    rc = ngx_http_send_header(r);
    TRACE_END(22);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        TRACE_FUNCTION_END();
        return rc;
    }

    b->file_pos = 0;
    b->file_last = of.size;

    b->in_file = b->file_last ? 1: 0;
    b->last_buf = (r == r->main) ? 1: 0;
    b->last_in_chain = 1;

    b->file->fd = of.fd;
    b->file->name = path;
    b->file->log = log;
    b->file->directio = of.is_directio;

    out.buf = b;
    out.next = NULL;

    TRACE_START();
    resultReturn = ngx_http_output_filter(r, &out);
    TRACE_END(23);

    TRACE_FUNCTION_END();
    return resultReturn;


	TRACE_FUNCTION_END();
}


static ngx_int_t
ngx_http_static_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_static_handler;

    return NGX_OK;
}
