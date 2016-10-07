

#include "apr_strings.h"
#include "apr_thread_proc.h"    

#define APR_WANT_STRFUNC
#include "apr_want.h"

#include "httpd.h"
#include "http_config.h"
#include "http_connection.h"
#include "http_core.h"
#include "http_protocol.h"   
#include "http_request.h"
#include "http_log.h"

#include "util_filter.h"
#include "util_ebcdic.h"
#include "ap_mpm.h"
#include "scoreboard.h"

#include "mod_core.h"

#include "trace_tool.h"


AP_DECLARE_DATA ap_filter_rec_t *ap_http_input_filter_handle;
AP_DECLARE_DATA ap_filter_rec_t *ap_http_header_filter_handle;
AP_DECLARE_DATA ap_filter_rec_t *ap_chunk_filter_handle;
AP_DECLARE_DATA ap_filter_rec_t *ap_http_outerror_filter_handle;
AP_DECLARE_DATA ap_filter_rec_t *ap_byterange_filter_handle;

AP_DECLARE_DATA const char *ap_multipart_boundary;


static int async_mpm = 0;

static const char *set_keep_alive_timeout(cmd_parms *cmd, void *dummy,
                                          const char *arg)
{
    apr_interval_time_t timeout;
    const char *err = ap_check_cmd_context(cmd, NOT_IN_DIR_LOC_FILE);
    if (err != NULL) {
        return err;
    }

    
    if (ap_timeout_parameter_parse(arg, &timeout, "s") != APR_SUCCESS)
        return "KeepAliveTimeout has wrong format";
    cmd->server->keep_alive_timeout = timeout;

    
    if (cmd->server->is_virtual) {
        cmd->server->keep_alive_timeout_set = 1;
    }
    return NULL;
}

static const char *set_keep_alive(cmd_parms *cmd, void *dummy,
                                  int arg)
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_DIR_LOC_FILE);
    if (err != NULL) {
        return err;
    }

    cmd->server->keep_alive = arg;
    return NULL;
}

static const char *set_keep_alive_max(cmd_parms *cmd, void *dummy,
                                      const char *arg)
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_DIR_LOC_FILE);
    if (err != NULL) {
        return err;
    }

    cmd->server->keep_alive_max = atoi(arg);
    return NULL;
}

static const command_rec http_cmds[] = {
    AP_INIT_TAKE1("KeepAliveTimeout", set_keep_alive_timeout, NULL, RSRC_CONF,
                  "Keep-Alive timeout duration (sec)"),
    AP_INIT_TAKE1("MaxKeepAliveRequests", set_keep_alive_max, NULL, RSRC_CONF,
                  "Maximum number of Keep-Alive requests per connection, "
                  "or 0 for infinite"),
    AP_INIT_FLAG("KeepAlive", set_keep_alive, NULL, RSRC_CONF,
                  "Whether persistent connections should be On or Off"),
    { NULL }
};

static const char *http_scheme(const request_rec *r)
{
    
    if (r->server->server_scheme &&
        (strcmp(r->server->server_scheme, "https") == 0))
        return "https";

    return "http";
}

static apr_port_t http_port(const request_rec *r)
{
    if (r->server->server_scheme &&
        (strcmp(r->server->server_scheme, "https") == 0))
        return DEFAULT_HTTPS_PORT;

    return DEFAULT_HTTP_PORT;
}

static int ap_process_http_async_connection(conn_rec *c)
{
    
	SESSION_START();
request_rec *r;
    conn_state_t *cs = c->cs;

    TRACE_START();
    AP_DEBUG_ASSERT(cs != NULL);
    TRACE_END(1);
    TRACE_START();
    AP_DEBUG_ASSERT(cs->state == CONN_STATE_READ_REQUEST_LINE);
    TRACE_END(2);

    while (cs->state == CONN_STATE_READ_REQUEST_LINE) {
        TRACE_START();
        ap_update_child_status_from_conn(c->sbh, SERVER_BUSY_READ, c);
        TRACE_END(3);

        if (TRACE_S_E( ((r = ap_read_request(c)) != NULL) ,4)) {

            c->keepalive = AP_CONN_UNKNOWN;
            

            if (r->status == HTTP_OK) {
                cs->state = CONN_STATE_HANDLER;
                TRACE_START();
                ap_update_child_status(c->sbh, SERVER_BUSY_WRITE, r);
                TRACE_END(5);
                TRACE_START();
                ap_process_async_request(r);
                TRACE_END(6);
                
                r = NULL;
            }

            if (cs->state != CONN_STATE_WRITE_COMPLETION &&
                cs->state != CONN_STATE_SUSPENDED) {
                
                cs->state = CONN_STATE_LINGER;
            }
        }
        else {   
            cs->state = CONN_STATE_LINGER;
        }
    }

    SESSION_END();
    return OK;


	SESSION_END();
}

static int ap_process_http_sync_connection(conn_rec *c)
{
    request_rec *r;
    conn_state_t *cs = c->cs;
    apr_socket_t *csd = NULL;
    int mpm_state = 0;

    

    ap_update_child_status_from_conn(c->sbh, SERVER_BUSY_READ, c);
    while ((r = ap_read_request(c)) != NULL) {
        apr_interval_time_t keep_alive_timeout = r->server->keep_alive_timeout;

        
        if (!r->server->keep_alive_timeout_set) {
            keep_alive_timeout = c->base_server->keep_alive_timeout;
        }

        c->keepalive = AP_CONN_UNKNOWN;
        

        if (r->status == HTTP_OK) {
            if (cs)
                cs->state = CONN_STATE_HANDLER;
            ap_update_child_status(c->sbh, SERVER_BUSY_WRITE, r);
            ap_process_request(r);
            
            r = NULL;
        }

        if (c->keepalive != AP_CONN_KEEPALIVE || c->aborted)
            break;

        ap_update_child_status(c->sbh, SERVER_BUSY_KEEPALIVE, NULL);

        if (ap_mpm_query(AP_MPMQ_MPM_STATE, &mpm_state)) {
            break;
        }

        if (mpm_state == AP_MPMQ_STOPPING) {
          break;
        }

        if (!csd) {
            csd = ap_get_conn_socket(c);
        }
        apr_socket_opt_set(csd, APR_INCOMPLETE_READ, 1);
        apr_socket_timeout_set(csd, keep_alive_timeout);
        
    }

    return OK;
}

static int ap_process_http_connection(conn_rec *c)
{
    if (async_mpm && !c->clogging_input_filters) {
        return ap_process_http_async_connection(c);
    }
    else {
        return ap_process_http_sync_connection(c);
    }
}

static int http_create_request(request_rec *r)
{
    if (!r->main && !r->prev) {
        ap_add_output_filter_handle(ap_byterange_filter_handle,
                                    NULL, r, r->connection);
        ap_add_output_filter_handle(ap_content_length_filter_handle,
                                    NULL, r, r->connection);
        ap_add_output_filter_handle(ap_http_header_filter_handle,
                                    NULL, r, r->connection);
        ap_add_output_filter_handle(ap_http_outerror_filter_handle,
                                    NULL, r, r->connection);
    }

    return OK;
}

static int http_send_options(request_rec *r)
{
    if ((r->method_number == M_OPTIONS) && r->uri && (r->uri[0] == '*') &&
         (r->uri[1] == '\0')) {
        return DONE;           
    }
    return DECLINED;
}

static int http_post_config(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s)
{
    apr_uint64_t val;
    if (ap_mpm_query(AP_MPMQ_IS_ASYNC, &async_mpm) != APR_SUCCESS) {
        async_mpm = 0;
    }
    ap_random_insecure_bytes(&val, sizeof(val));
    ap_multipart_boundary = apr_psprintf(p, "%0" APR_UINT64_T_HEX_FMT, val);

    return OK;
}

static void register_hooks(apr_pool_t *p)
{
    ap_hook_post_config(http_post_config, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_process_connection(ap_process_http_connection, NULL, NULL,
                               APR_HOOK_REALLY_LAST);
    ap_hook_map_to_storage(ap_send_http_trace,NULL,NULL,APR_HOOK_MIDDLE);
    ap_hook_map_to_storage(http_send_options,NULL,NULL,APR_HOOK_MIDDLE);
    ap_hook_http_scheme(http_scheme,NULL,NULL,APR_HOOK_REALLY_LAST);
    ap_hook_default_port(http_port,NULL,NULL,APR_HOOK_REALLY_LAST);
    ap_hook_create_request(http_create_request, NULL, NULL, APR_HOOK_REALLY_LAST);
    ap_http_input_filter_handle =
        ap_register_input_filter("HTTP_IN", ap_http_filter,
                                 NULL, AP_FTYPE_PROTOCOL);
    ap_http_header_filter_handle =
        ap_register_output_filter("HTTP_HEADER", ap_http_header_filter,
                                  NULL, AP_FTYPE_PROTOCOL);
    ap_chunk_filter_handle =
        ap_register_output_filter("CHUNK", ap_http_chunk_filter,
                                  NULL, AP_FTYPE_TRANSCODE);
    ap_http_outerror_filter_handle =
        ap_register_output_filter("HTTP_OUTERROR", ap_http_outerror_filter,
                                  NULL, AP_FTYPE_PROTOCOL);
    ap_byterange_filter_handle =
        ap_register_output_filter("BYTERANGE", ap_byterange_filter,
                                  NULL, AP_FTYPE_PROTOCOL);
    ap_method_registry_init(p);
}

AP_DECLARE_MODULE(http) = {
    STANDARD20_MODULE_STUFF,
    NULL,              
    NULL,              
    NULL,              
    NULL,              
    http_cmds,         
    register_hooks     
};
