



#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_trace.h>

#include <ngx_trace.h>


typedef struct {
    ngx_str_t                name;
    ngx_array_t             *lengths;
    ngx_array_t             *values;
} ngx_http_index_t;


typedef struct {
    ngx_array_t             *indices;    
    size_t                   max_index_len;
} ngx_http_index_loc_conf_t;


#define NGX_HTTP_DEFAULT_INDEX   "index.html"


static ngx_int_t ngx_http_index_test_dir(ngx_http_request_t *r,
    ngx_http_core_loc_conf_t *clcf, u_char *path, u_char *last);
static ngx_int_t ngx_http_index_error(ngx_http_request_t *r,
    ngx_http_core_loc_conf_t *clcf, u_char *file, ngx_err_t err);

static ngx_int_t ngx_http_index_init(ngx_conf_t *cf);
static void *ngx_http_index_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_index_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);
static char *ngx_http_index_set_index(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


static ngx_command_t  ngx_http_index_commands[] = {

    { ngx_string("index"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_index_set_index,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_index_module_ctx = {
    NULL,                                  
    ngx_http_index_init,                   

    NULL,                                  
    NULL,                                  

    NULL,                                  
    NULL,                                  

    ngx_http_index_create_loc_conf,        
    ngx_http_index_merge_loc_conf          
};


ngx_module_t  ngx_http_index_module = {
    NGX_MODULE_V1,
    &ngx_http_index_module_ctx,            
    ngx_http_index_commands,               
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
ngx_http_index_handler(ngx_http_request_t *r)
{
    
	TRACE_FUNCTION_START();
	ngx_int_t
 resultReturn;
u_char                       *p, *name;
    size_t                        len, root, reserve, allocated;
    ngx_int_t                     rc;
    ngx_str_t                     path, uri;
    ngx_uint_t                    i, dir_tested;
    ngx_http_index_t             *index;
    ngx_open_file_info_t          of;
    ngx_http_script_code_pt       code;
    ngx_http_script_engine_t      e;
    ngx_http_core_loc_conf_t     *clcf;
    ngx_http_index_loc_conf_t    *ilcf;
    ngx_http_script_len_code_pt   lcode;


    if (r->uri.data[r->uri.len - 1] != '/') {
        TRACE_FUNCTION_END();
        return NGX_DECLINED;
    }

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD|NGX_HTTP_POST))) {
        TRACE_FUNCTION_END();
        return NGX_DECLINED;
    }

    TRACE_START();
    ilcf = ngx_http_get_module_loc_conf(r, ngx_http_index_module);
    TRACE_END(1);
    TRACE_START();
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    TRACE_END(2);

    allocated = 0;
    root = 0;
    dir_tested = 0;
    name = NULL;
    
    path.data = NULL;

    index = ilcf->indices->elts;
    for (i = 0; i < ilcf->indices->nelts ; i++) {

        if (index[i].lengths == NULL) {

            if (index[i].name.data[0] == '/') {
                TRACE_START();
                resultReturn = ngx_http_internal_redirect(r, &index[i].name, &r->args);
                TRACE_END(3);

                TRACE_FUNCTION_END();
                return resultReturn;
            }

            reserve = ilcf->max_index_len;
            len = index[i].name.len;

        } else {
            TRACE_START();
            ngx_memzero(&e, sizeof(ngx_http_script_engine_t));
            TRACE_END(4);

            e.ip = index[i].lengths->elts;
            e.request = r;
            e.flushed = 1;

            
            len = 1;

            while (*(uintptr_t *) e.ip) {
                lcode = *(ngx_http_script_len_code_pt *) e.ip;
                TRACE_START();
                len += lcode(&e);
                TRACE_END(5);
            }

            

            reserve = len + 16;
        }

        if (reserve > allocated) {

            TRACE_START();
            name = ngx_http_map_uri_to_path(r, &path, &root, reserve);
            TRACE_END(6);
            if (name == NULL) {
                TRACE_FUNCTION_END();
                return NGX_ERROR;
            }

            allocated = path.data + path.len - name;
        }

        if (index[i].values == NULL) {

            

            TRACE_START();
            ngx_memcpy(name, index[i].name.data, index[i].name.len);
            TRACE_END(7);

            path.len = (name + index[i].name.len - 1) - path.data;

        } else {
            e.ip = index[i].values->elts;
            e.pos = name;

            while (*(uintptr_t *) e.ip) {
                code = *(ngx_http_script_code_pt *) e.ip;
                TRACE_START();
                code((ngx_http_script_engine_t *) &e);
                TRACE_END(8);
            }

            if (*name == '/') {
                uri.len = len - 1;
                uri.data = name;
                TRACE_START();
                resultReturn = ngx_http_internal_redirect(r, &uri, &r->args);
                TRACE_END(9);

                TRACE_FUNCTION_END();
                return resultReturn;
            }

            path.len = e.pos - path.data;

            *e.pos = '\0';
        }

        TRACE_START();
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "open index \"%V\"", &path);
        TRACE_END(10);

        TRACE_START();
        ngx_memzero(&of, sizeof(ngx_open_file_info_t));
        TRACE_END(11);

        of.read_ahead = clcf->read_ahead;
        of.directio = clcf->directio;
        of.valid = clcf->open_file_cache_valid;
        of.min_uses = clcf->open_file_cache_min_uses;
        of.test_only = 1;
        of.errors = clcf->open_file_cache_errors;
        of.events = clcf->open_file_cache_events;

        if (TRACE_S_E( ngx_http_set_disable_symlinks(r, clcf, &path, &of) != NGX_OK ,12)) {
            TRACE_FUNCTION_END();
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        if (TRACE_S_E( ngx_open_cached_file(clcf->open_file_cache, &path, &of, r->pool)
            != NGX_OK ,13))
        {
            TRACE_START();
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, of.err,
                           "%s \"%s\" failed", of.failed, path.data);
            TRACE_END(14);

            if (of.err == 0) {
                TRACE_FUNCTION_END();
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

#if (NGX_HAVE_OPENAT)
            if (of.err == NGX_EMLINK
                || of.err == NGX_ELOOP)
            {
                TRACE_FUNCTION_END();
                return NGX_HTTP_FORBIDDEN;
            }
#endif

            if (of.err == NGX_ENOTDIR
                || of.err == NGX_ENAMETOOLONG
                || of.err == NGX_EACCES)
            {
                TRACE_START();
                resultReturn = ngx_http_index_error(r, clcf, path.data, of.err);
                TRACE_END(15);

                TRACE_FUNCTION_END();
                return resultReturn;
            }

            if (!dir_tested) {
                TRACE_START();
                rc = ngx_http_index_test_dir(r, clcf, path.data, name - 1);
                TRACE_END(16);

                if (rc != NGX_OK) {
                    TRACE_FUNCTION_END();
                    return rc;
                }

                dir_tested = 1;
            }

            if (of.err == NGX_ENOENT) {
                continue;
            }

            TRACE_START();
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, of.err,
                          "%s \"%s\" failed", of.failed, path.data);
            TRACE_END(17);

            TRACE_FUNCTION_END();
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        uri.len = r->uri.len + len - 1;

        if (!clcf->alias) {
            uri.data = path.data + root;

        } else {
            TRACE_START();
            uri.data = ngx_pnalloc(r->pool, uri.len);
            TRACE_END(18);
            if (uri.data == NULL) {
                TRACE_FUNCTION_END();
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            TRACE_START();
            p = ngx_copy(uri.data, r->uri.data, r->uri.len);
            TRACE_END(19);
            TRACE_START();
            ngx_memcpy(p, name, len - 1);
            TRACE_END(20);
        }

        PATH_INC();
        TRACE_START();
        rc = ngx_http_internal_redirect(r, &uri, &r->args);
        TRACE_END(21);
        PATH_DEC();
        TRACE_FUNCTION_END();
        return rc;
    }

    TRACE_FUNCTION_END();
    return NGX_DECLINED;


	TRACE_FUNCTION_END();
}


static ngx_int_t
ngx_http_index_test_dir(ngx_http_request_t *r, ngx_http_core_loc_conf_t *clcf,
    u_char *path, u_char *last)
{
    u_char                c;
    ngx_str_t             dir;
    ngx_open_file_info_t  of;

    c = *last;
    if (c != '/' || path == last) {
        
        c = *(++last);
    }
    *last = '\0';

    dir.len = last - path;
    dir.data = path;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http index check dir: \"%V\"", &dir);

    ngx_memzero(&of, sizeof(ngx_open_file_info_t));

    of.test_dir = 1;
    of.test_only = 1;
    of.valid = clcf->open_file_cache_valid;
    of.errors = clcf->open_file_cache_errors;

    if (ngx_http_set_disable_symlinks(r, clcf, &dir, &of) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_open_cached_file(clcf->open_file_cache, &dir, &of, r->pool)
        != NGX_OK)
    {
        if (of.err) {

#if (NGX_HAVE_OPENAT)
            if (of.err == NGX_EMLINK
                || of.err == NGX_ELOOP)
            {
                return NGX_HTTP_FORBIDDEN;
            }
#endif

            if (of.err == NGX_ENOENT) {
                *last = c;
                return ngx_http_index_error(r, clcf, dir.data, NGX_ENOENT);
            }

            if (of.err == NGX_EACCES) {

                *last = c;

                

                return NGX_OK;
            }

            ngx_log_error(NGX_LOG_CRIT, r->connection->log, of.err,
                          "%s \"%s\" failed", of.failed, dir.data);
        }

        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    *last = c;

    if (of.is_dir) {
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                  "\"%s\" is not a directory", dir.data);

    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}


static ngx_int_t
ngx_http_index_error(ngx_http_request_t *r, ngx_http_core_loc_conf_t  *clcf,
    u_char *file, ngx_err_t err)
{
    if (err == NGX_EACCES) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, err,
                      "\"%s\" is forbidden", file);

        return NGX_HTTP_FORBIDDEN;
    }

    if (clcf->log_not_found) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, err,
                      "\"%s\" is not found", file);
    }

    return NGX_HTTP_NOT_FOUND;
}


static void *
ngx_http_index_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_index_loc_conf_t  *conf;

    conf = ngx_palloc(cf->pool, sizeof(ngx_http_index_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->indices = NULL;
    conf->max_index_len = 0;

    return conf;
}


static char *
ngx_http_index_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_index_loc_conf_t  *prev = parent;
    ngx_http_index_loc_conf_t  *conf = child;

    ngx_http_index_t  *index;

    if (conf->indices == NULL) {
        conf->indices = prev->indices;
        conf->max_index_len = prev->max_index_len;
    }

    if (conf->indices == NULL) {
        conf->indices = ngx_array_create(cf->pool, 1, sizeof(ngx_http_index_t));
        if (conf->indices == NULL) {
            return NGX_CONF_ERROR;
        }

        index = ngx_array_push(conf->indices);
        if (index == NULL) {
            return NGX_CONF_ERROR;
        }

        index->name.len = sizeof(NGX_HTTP_DEFAULT_INDEX);
        index->name.data = (u_char *) NGX_HTTP_DEFAULT_INDEX;
        index->lengths = NULL;
        index->values = NULL;

        conf->max_index_len = sizeof(NGX_HTTP_DEFAULT_INDEX);

        return NGX_CONF_OK;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_index_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_index_handler;

    return NGX_OK;
}




static char *
ngx_http_index_set_index(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_index_loc_conf_t *ilcf = conf;

    ngx_str_t                  *value;
    ngx_uint_t                  i, n;
    ngx_http_index_t           *index;
    ngx_http_script_compile_t   sc;

    if (ilcf->indices == NULL) {
        ilcf->indices = ngx_array_create(cf->pool, 2, sizeof(ngx_http_index_t));
        if (ilcf->indices == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {

        if (value[i].data[0] == '/' && i != cf->args->nelts - 1) {
            ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                               "only the last index in \"index\" directive "
                               "should be absolute");
        }

        if (value[i].len == 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "index \"%V\" in \"index\" directive is invalid",
                               &value[1]);
            return NGX_CONF_ERROR;
        }

        index = ngx_array_push(ilcf->indices);
        if (index == NULL) {
            return NGX_CONF_ERROR;
        }

        index->name.len = value[i].len;
        index->name.data = value[i].data;
        index->lengths = NULL;
        index->values = NULL;

        n = ngx_http_script_variables_count(&value[i]);

        if (n == 0) {
            if (ilcf->max_index_len < index->name.len) {
                ilcf->max_index_len = index->name.len;
            }

            if (index->name.data[0] == '/') {
                continue;
            }

            
            index->name.len++;

            continue;
        }

        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

        sc.cf = cf;
        sc.source = &value[i];
        sc.lengths = &index->lengths;
        sc.values = &index->values;
        sc.variables = n;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}
