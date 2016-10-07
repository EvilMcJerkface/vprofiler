



#include <trace_tool.h>
#include "apr.h"
#include "apr_strings.h"
#include "apr_lib.h"
#include "apr_fnmatch.h"
#include "apr_hash.h"
#include "apr_thread_proc.h"    

#define APR_WANT_IOVEC
#define APR_WANT_STRFUNC
#define APR_WANT_MEMFUNC
#include "apr_want.h"

#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_protocol.h" 
#include "http_request.h"
#include "http_vhost.h"
#include "http_main.h"     
#include "http_log.h"
#include "util_md5.h"
#include "http_connection.h"
#include "apr_buckets.h"
#include "util_filter.h"
#include "util_ebcdic.h"
#include "mpm_common.h"
#include "scoreboard.h"
#include "mod_core.h"
#include "ap_listen.h"

#include "mod_so.h" 

#include "trace_tool.h"

#define AP_MIN_SENDFILE_BYTES           (256)


#define BRIGADE_NORMALIZE(b) \
do { \
    apr_bucket *e = APR_BRIGADE_FIRST(b); \
    do {  \
        if (e->length == 0 && !APR_BUCKET_IS_METADATA(e)) { \
            apr_bucket *d; \
            d = APR_BUCKET_NEXT(e); \
            apr_bucket_delete(e); \
            e = d; \
        } \
        else { \
            e = APR_BUCKET_NEXT(e); \
        } \
    } while (!APR_BRIGADE_EMPTY(b) && (e != APR_BRIGADE_SENTINEL(b))); \
} while (0)


#undef APLOG_MODULE_INDEX
#define APLOG_MODULE_INDEX AP_CORE_MODULE_INDEX

struct core_output_filter_ctx {
    apr_bucket_brigade *buffered_bb;
    apr_bucket_brigade *tmp_flush_bb;
    apr_pool_t *deferred_write_pool;
    apr_size_t bytes_written;
};

struct core_filter_ctx {
    apr_bucket_brigade *b;
    apr_bucket_brigade *tmpbb;
};


apr_status_t ap_core_input_filter(ap_filter_t *f, apr_bucket_brigade *b,
                                  ap_input_mode_t mode, apr_read_type_e block,
                                  apr_off_t readbytes)
{
    apr_status_t rv;
    core_net_rec *net = f->ctx;
    core_ctx_t *ctx = net->in_ctx;
    const char *str;
    apr_size_t len;

    if (mode == AP_MODE_INIT) {
        
        return APR_SUCCESS;
    }

    if (!ctx)
    {
        net->in_ctx = ctx = apr_palloc(f->c->pool, sizeof(*ctx));
        ctx->b = apr_brigade_create(f->c->pool, f->c->bucket_alloc);
        ctx->tmpbb = apr_brigade_create(f->c->pool, f->c->bucket_alloc);
        
        rv = ap_run_insert_network_bucket(f->c, ctx->b, net->client_socket);
        if (rv != APR_SUCCESS)
            return rv;
    }
    else if (APR_BRIGADE_EMPTY(ctx->b)) {
        return APR_EOF;
    }

    
    BRIGADE_NORMALIZE(ctx->b);

    
    if (APR_BRIGADE_EMPTY(ctx->b)) {
        return APR_EOF;
    }

    if (mode == AP_MODE_GETLINE) {
        
        rv = apr_brigade_split_line(b, ctx->b, block, HUGE_STRING_LEN);
        
        if (APR_STATUS_IS_EAGAIN(rv) && block == APR_NONBLOCK_READ) {
            rv = APR_SUCCESS;
        }
        return rv;
    }

    
    if (mode == AP_MODE_EATCRLF) {
        apr_bucket *e;
        const char *c;

        
        while (1) {
            if (APR_BRIGADE_EMPTY(ctx->b))
                return APR_EOF;

            e = APR_BRIGADE_FIRST(ctx->b);

            rv = apr_bucket_read(e, &str, &len, APR_NONBLOCK_READ);

            if (rv != APR_SUCCESS)
                return rv;

            c = str;
            while (c < str + len) {
                if (*c == APR_ASCII_LF)
                    c++;
                else if (*c == APR_ASCII_CR && *(c + 1) == APR_ASCII_LF)
                    c += 2;
                else
                    return APR_SUCCESS;
            }

            
            
            apr_bucket_delete(e);
        }
        return APR_SUCCESS;
    }

    
    if (mode == AP_MODE_EXHAUSTIVE) {
        apr_bucket *e;

        
        APR_BRIGADE_CONCAT(b, ctx->b);

        
        
        e = apr_bucket_eos_create(f->c->bucket_alloc);
        APR_BRIGADE_INSERT_TAIL(b, e);
        return APR_SUCCESS;
    }

    
    if (mode == AP_MODE_READBYTES || mode == AP_MODE_SPECULATIVE) {
        apr_bucket *e;

        AP_DEBUG_ASSERT(readbytes > 0);

        e = APR_BRIGADE_FIRST(ctx->b);
        rv = apr_bucket_read(e, &str, &len, block);

        if (APR_STATUS_IS_EAGAIN(rv) && block == APR_NONBLOCK_READ) {
            
            return APR_SUCCESS;
        }
        else if (rv != APR_SUCCESS) {
            return rv;
        }
        else if (block == APR_BLOCK_READ && len == 0) {
            
            apr_bucket_delete(e);

            if (mode == AP_MODE_READBYTES) {
                e = apr_bucket_eos_create(f->c->bucket_alloc);
                APR_BRIGADE_INSERT_TAIL(b, e);
            }
            return APR_SUCCESS;
        }

        
        if (len < readbytes) {
            apr_size_t bucket_len;

            rv = APR_SUCCESS;
            
            e = APR_BUCKET_NEXT(e);
            while ((len < readbytes) && (rv == APR_SUCCESS)
                   && (e != APR_BRIGADE_SENTINEL(ctx->b))) {
                
                if (e->length != -1) {
                    len += e->length;
                    e = APR_BUCKET_NEXT(e);
                }
                else {
                    
                    rv = apr_bucket_read(e, &str, &bucket_len,
                                         APR_NONBLOCK_READ);
                    if (rv == APR_SUCCESS) {
                        len += bucket_len;
                        e = APR_BUCKET_NEXT(e);
                    }
                }
            }
        }

        
        if (len < readbytes) {
            readbytes = len;
        }

        rv = apr_brigade_partition(ctx->b, readbytes, &e);
        if (rv != APR_SUCCESS) {
            return rv;
        }

        
        ctx->tmpbb = apr_brigade_split_ex(ctx->b, e, ctx->tmpbb);

        if (mode == AP_MODE_READBYTES) {
            APR_BRIGADE_CONCAT(b, ctx->b);
        }
        else if (mode == AP_MODE_SPECULATIVE) {
            apr_bucket *copy_bucket;

            for (e = APR_BRIGADE_FIRST(ctx->b);
                 e != APR_BRIGADE_SENTINEL(ctx->b);
                 e = APR_BUCKET_NEXT(e))
            {
                rv = apr_bucket_copy(e, &copy_bucket);
                if (rv != APR_SUCCESS) {
                    return rv;
                }
                APR_BRIGADE_INSERT_TAIL(b, copy_bucket);
            }
        }

        
        APR_BRIGADE_CONCAT(ctx->b, ctx->tmpbb);
    }
    return APR_SUCCESS;
}

static void setaside_remaining_output(ap_filter_t *f,
                                      core_output_filter_ctx_t *ctx,
                                      apr_bucket_brigade *bb,
                                      conn_rec *c);

static apr_status_t send_brigade_nonblocking(apr_socket_t *s,
                                             apr_bucket_brigade *bb,
                                             apr_size_t *bytes_written,
                                             conn_rec *c);

static void remove_empty_buckets(apr_bucket_brigade *bb);

static apr_status_t send_brigade_blocking(apr_socket_t *s,
                                          apr_bucket_brigade *bb,
                                          apr_size_t *bytes_written,
                                          conn_rec *c);

static apr_status_t writev_nonblocking(apr_socket_t *s,
                                       struct iovec *vec, apr_size_t nvec,
                                       apr_bucket_brigade *bb,
                                       apr_size_t *cumulative_bytes_written,
                                       conn_rec *c);

#if APR_HAS_SENDFILE
static apr_status_t sendfile_nonblocking(apr_socket_t *s,
                                         apr_bucket *bucket,
                                         apr_size_t *cumulative_bytes_written,
                                         conn_rec *c);
#endif


#define THRESHOLD_MIN_WRITE 4096
#define THRESHOLD_MAX_BUFFER 65536
#define MAX_REQUESTS_IN_PIPELINE 5


extern APR_OPTIONAL_FN_TYPE(ap_logio_add_bytes_out) *ap__logio_add_bytes_out;

apr_status_t ap_core_output_filter(ap_filter_t *f, apr_bucket_brigade *new_bb)
{
    conn_rec *c = f->c;
    core_net_rec *net = f->ctx;
    core_output_filter_ctx_t *ctx = net->out_ctx;
    apr_bucket_brigade *bb = NULL;
    apr_bucket *bucket, *next, *flush_upto = NULL;
    apr_size_t bytes_in_brigade, non_file_bytes_in_brigade;
    int eor_buckets_in_brigade, morphing_bucket_in_brigade;
    apr_status_t rv;

    
    if (c->aborted) {
        if (new_bb != NULL) {
            apr_brigade_cleanup(new_bb);
        }
        return APR_ECONNABORTED;
    }

    if (ctx == NULL) {
        ctx = apr_pcalloc(c->pool, sizeof(*ctx));
        net->out_ctx = (core_output_filter_ctx_t *)ctx;
        
        ctx->tmp_flush_bb = apr_brigade_create(c->pool, c->bucket_alloc);
        
        ctx->buffered_bb = apr_brigade_create(c->pool, c->bucket_alloc);
    }

    if (new_bb != NULL)
        bb = new_bb;

    if ((ctx->buffered_bb != NULL) &&
        !APR_BRIGADE_EMPTY(ctx->buffered_bb)) {
        if (new_bb != NULL) {
            APR_BRIGADE_PREPEND(bb, ctx->buffered_bb);
        }
        else {
            bb = ctx->buffered_bb;
        }
        c->data_in_output_filters = 0;
    }
    else if (new_bb == NULL) {
        return APR_SUCCESS;
    }

    

    if (new_bb == NULL) {
        rv = send_brigade_nonblocking(net->client_socket, bb,
                                      &(ctx->bytes_written), c);
        if (rv != APR_SUCCESS && !APR_STATUS_IS_EAGAIN(rv)) {
            
            ap_log_cerror(APLOG_MARK, APLOG_TRACE1, rv, c,
                          "core_output_filter: writing data to the network");
            apr_brigade_cleanup(bb);
            c->aborted = 1;
            return rv;
        }
        setaside_remaining_output(f, ctx, bb, c);
        return APR_SUCCESS;
    }

    bytes_in_brigade = 0;
    non_file_bytes_in_brigade = 0;
    eor_buckets_in_brigade = 0;
    morphing_bucket_in_brigade = 0;

    for (bucket = APR_BRIGADE_FIRST(bb); bucket != APR_BRIGADE_SENTINEL(bb);
         bucket = next) {
        next = APR_BUCKET_NEXT(bucket);

        if (!APR_BUCKET_IS_METADATA(bucket)) {
            if (bucket->length == (apr_size_t)-1) {
                
                morphing_bucket_in_brigade = 1;
            }
            else {
                bytes_in_brigade += bucket->length;
                if (!APR_BUCKET_IS_FILE(bucket))
                    non_file_bytes_in_brigade += bucket->length;
            }
        }
        else if (AP_BUCKET_IS_EOR(bucket)) {
            eor_buckets_in_brigade++;
        }

        if (APR_BUCKET_IS_FLUSH(bucket)
            || non_file_bytes_in_brigade >= THRESHOLD_MAX_BUFFER
            || morphing_bucket_in_brigade
            || eor_buckets_in_brigade > MAX_REQUESTS_IN_PIPELINE) {
            

            if (APLOGctrace6(c)) {
                char *reason = APR_BUCKET_IS_FLUSH(bucket) ?
                               "FLUSH bucket" :
                               (non_file_bytes_in_brigade >= THRESHOLD_MAX_BUFFER) ?
                               "THRESHOLD_MAX_BUFFER" :
                               morphing_bucket_in_brigade ? "morphing bucket" :
                               "MAX_REQUESTS_IN_PIPELINE";
                ap_log_cerror(APLOG_MARK, APLOG_TRACE6, 0, c,
                              "core_output_filter: flushing because of %s",
                              reason);
            }
            
            flush_upto = next;

            bytes_in_brigade = 0;
            non_file_bytes_in_brigade = 0;
            eor_buckets_in_brigade = 0;
            morphing_bucket_in_brigade = 0;
        }
    }

    if (flush_upto != NULL) {
        ctx->tmp_flush_bb = apr_brigade_split_ex(bb, flush_upto,
                                                 ctx->tmp_flush_bb);
        rv = send_brigade_blocking(net->client_socket, bb,
                                   &(ctx->bytes_written), c);
        if (rv != APR_SUCCESS) {
            
            ap_log_cerror(APLOG_MARK, APLOG_TRACE1, rv, c,
                          "core_output_filter: writing data to the network");
            apr_brigade_cleanup(bb);
            c->aborted = 1;
            return rv;
        }
        APR_BRIGADE_CONCAT(bb, ctx->tmp_flush_bb);
    }

    if (bytes_in_brigade >= THRESHOLD_MIN_WRITE) {
        rv = send_brigade_nonblocking(net->client_socket, bb,
                                      &(ctx->bytes_written), c);
        if ((rv != APR_SUCCESS) && (!APR_STATUS_IS_EAGAIN(rv))) {
            
            ap_log_cerror(APLOG_MARK, APLOG_TRACE1, rv, c,
                          "core_output_filter: writing data to the network");
            apr_brigade_cleanup(bb);
            c->aborted = 1;
            return rv;
        }
    }

    setaside_remaining_output(f, ctx, bb, c);
    return APR_SUCCESS;
}


static void setaside_remaining_output(ap_filter_t *f,
                                      core_output_filter_ctx_t *ctx,
                                      apr_bucket_brigade *bb,
                                      conn_rec *c)
{
    
	TRACE_FUNCTION_START();
if (bb == NULL) {
        TRACE_FUNCTION_END();
        return ;
    }
    TRACE_START();
    remove_empty_buckets(bb);
    TRACE_END(1);
    if (TRACE_S_E( !APR_BRIGADE_EMPTY(bb) ,2)) {
        c->data_in_output_filters = 1;
        if (bb != ctx->buffered_bb) {
            if (!ctx->deferred_write_pool) {
                TRACE_START();
                apr_pool_create(&ctx->deferred_write_pool, c->pool);
                TRACE_END(3);
                TRACE_START();
                apr_pool_tag(ctx->deferred_write_pool, "deferred_write");
                TRACE_END(4);
            }
            TRACE_START();
            ap_save_brigade(f, &(ctx->buffered_bb), &bb,
                            ctx->deferred_write_pool);
            TRACE_END(5);
        }
    }
    else if (ctx->deferred_write_pool) {
        
        TRACE_START();
        apr_pool_clear(ctx->deferred_write_pool);
        TRACE_END(6);
    }


	TRACE_FUNCTION_END();
}

#ifndef APR_MAX_IOVEC_SIZE
#define MAX_IOVEC_TO_WRITE 16
#else
#if APR_MAX_IOVEC_SIZE > 16
#define MAX_IOVEC_TO_WRITE 16
#else
#define MAX_IOVEC_TO_WRITE APR_MAX_IOVEC_SIZE
#endif
#endif

static apr_status_t send_brigade_nonblocking(apr_socket_t *s,
                                             apr_bucket_brigade *bb,
                                             apr_size_t *bytes_written,
                                             conn_rec *c)
{
    apr_bucket *bucket, *next;
    apr_status_t rv;
    struct iovec vec[MAX_IOVEC_TO_WRITE];
    apr_size_t nvec = 0;

    remove_empty_buckets(bb);

    for (bucket = APR_BRIGADE_FIRST(bb);
         bucket != APR_BRIGADE_SENTINEL(bb);
         bucket = next) {
        next = APR_BUCKET_NEXT(bucket);
#if APR_HAS_SENDFILE
        if (APR_BUCKET_IS_FILE(bucket)) {
            apr_bucket_file *file_bucket = (apr_bucket_file *)(bucket->data);
            apr_file_t *fd = file_bucket->fd;
            

            if ((apr_file_flags_get(fd) & APR_SENDFILE_ENABLED) &&
                (bucket->length >= AP_MIN_SENDFILE_BYTES)) {
                if (nvec > 0) {
                    (void)apr_socket_opt_set(s, APR_TCP_NOPUSH, 1);
                    rv = writev_nonblocking(s, vec, nvec, bb, bytes_written, c);
                    if (rv != APR_SUCCESS) {
                        (void)apr_socket_opt_set(s, APR_TCP_NOPUSH, 0);
                        return rv;
                    }
                }
                rv = sendfile_nonblocking(s, bucket, bytes_written, c);
                if (nvec > 0) {
                    (void)apr_socket_opt_set(s, APR_TCP_NOPUSH, 0);
                    nvec = 0;
                }
                if (rv != APR_SUCCESS) {
                    return rv;
                }
                break;
            }
        }
#endif 
        
        if (!APR_BUCKET_IS_METADATA(bucket)) {
            const char *data;
            apr_size_t length;
            
            
            rv = apr_bucket_read(bucket, &data, &length, APR_NONBLOCK_READ);
            if (APR_STATUS_IS_EAGAIN(rv)) {
                
                if (nvec) {
                    rv = writev_nonblocking(s, vec, nvec, bb, bytes_written, c);
                    if (rv) {
                        return rv;
                    }
                    nvec = 0;
                }
                
                rv = apr_bucket_read(bucket, &data, &length, APR_BLOCK_READ);
            }
            if (rv != APR_SUCCESS) {
                return rv;
            }

            
            next = APR_BUCKET_NEXT(bucket);
            vec[nvec].iov_base = (char *)data;
            vec[nvec].iov_len = length;
            nvec++;
            if (nvec == MAX_IOVEC_TO_WRITE) {
                rv = writev_nonblocking(s, vec, nvec, bb, bytes_written, c);
                nvec = 0;
                if (rv != APR_SUCCESS) {
                    return rv;
                }
                break;
            }
        }
    }

    if (nvec > 0) {
        rv = writev_nonblocking(s, vec, nvec, bb, bytes_written, c);
        if (rv != APR_SUCCESS) {
            return rv;
        }
    }

    remove_empty_buckets(bb);

    return APR_SUCCESS;
}

static void remove_empty_buckets(apr_bucket_brigade *bb)
{
    apr_bucket *bucket;
    while (((bucket = APR_BRIGADE_FIRST(bb)) != APR_BRIGADE_SENTINEL(bb)) &&
           (APR_BUCKET_IS_METADATA(bucket) || (bucket->length == 0))) {
        apr_bucket_delete(bucket);
    }
}

static apr_status_t send_brigade_blocking(apr_socket_t *s,
                                          apr_bucket_brigade *bb,
                                          apr_size_t *bytes_written,
                                          conn_rec *c)
{
    apr_status_t rv;

    rv = APR_SUCCESS;
    while (!APR_BRIGADE_EMPTY(bb)) {
        rv = send_brigade_nonblocking(s, bb, bytes_written, c);
        if (rv != APR_SUCCESS) {
            if (APR_STATUS_IS_EAGAIN(rv)) {
                
                apr_int32_t nsds;
                apr_interval_time_t timeout;
                apr_pollfd_t pollset;

                pollset.p = c->pool;
                pollset.desc_type = APR_POLL_SOCKET;
                pollset.reqevents = APR_POLLOUT;
                pollset.desc.s = s;
                apr_socket_timeout_get(s, &timeout);
                do {
                    rv = apr_poll(&pollset, 1, &nsds, timeout);
                } while (APR_STATUS_IS_EINTR(rv));
                if (rv != APR_SUCCESS) {
                    break;
                }
            }
            else {
                break;
            }
        }
    }
    return rv;
}

static apr_status_t writev_nonblocking(apr_socket_t *s,
                                       struct iovec *vec, apr_size_t nvec,
                                       apr_bucket_brigade *bb,
                                       apr_size_t *cumulative_bytes_written,
                                       conn_rec *c)
{
    apr_status_t rv = APR_SUCCESS, arv;
    apr_size_t bytes_written = 0, bytes_to_write = 0;
    apr_size_t i, offset;
    apr_interval_time_t old_timeout;

    arv = apr_socket_timeout_get(s, &old_timeout);
    if (arv != APR_SUCCESS) {
        return arv;
    }
    arv = apr_socket_timeout_set(s, 0);
    if (arv != APR_SUCCESS) {
        return arv;
    }

    for (i = 0; i < nvec; i++) {
        bytes_to_write += vec[i].iov_len;
    }
    offset = 0;
    while (bytes_written < bytes_to_write) {
        apr_size_t n = 0;
        rv = apr_socket_sendv(s, vec + offset, nvec - offset, &n);
        if (n > 0) {
            bytes_written += n;
            for (i = offset; i < nvec; ) {
                apr_bucket *bucket = APR_BRIGADE_FIRST(bb);
                if (APR_BUCKET_IS_METADATA(bucket)) {
                    apr_bucket_delete(bucket);
                }
                else if (n >= vec[i].iov_len) {
                    apr_bucket_delete(bucket);
                    offset++;
                    n -= vec[i++].iov_len;
                }
                else {
                    apr_bucket_split(bucket, n);
                    apr_bucket_delete(bucket);
                    vec[i].iov_len -= n;
                    vec[i].iov_base = (char *) vec[i].iov_base + n;
                    break;
                }
            }
        }
        if (rv != APR_SUCCESS) {
            break;
        }
    }
    if ((ap__logio_add_bytes_out != NULL) && (bytes_written > 0)) {
        ap__logio_add_bytes_out(c, bytes_written);
    }
    *cumulative_bytes_written += bytes_written;

    arv = apr_socket_timeout_set(s, old_timeout);
    if ((arv != APR_SUCCESS) && (rv == APR_SUCCESS)) {
        return arv;
    }
    else {
        return rv;
    }
}

#if APR_HAS_SENDFILE

static apr_status_t sendfile_nonblocking(apr_socket_t *s,
                                         apr_bucket *bucket,
                                         apr_size_t *cumulative_bytes_written,
                                         conn_rec *c)
{
    apr_status_t rv = APR_SUCCESS;
    apr_bucket_file *file_bucket;
    apr_file_t *fd;
    apr_size_t file_length;
    apr_off_t file_offset;
    apr_size_t bytes_written = 0;

    if (!APR_BUCKET_IS_FILE(bucket)) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rv, c->base_server, APLOGNO(00006)
                     "core_filter: sendfile_nonblocking: "
                     "this should never happen");
        return APR_EGENERAL;
    }
    file_bucket = (apr_bucket_file *)(bucket->data);
    fd = file_bucket->fd;
    file_length = bucket->length;
    file_offset = bucket->start;

    if (bytes_written < file_length) {
        apr_size_t n = file_length - bytes_written;
        apr_status_t arv;
        apr_interval_time_t old_timeout;

        arv = apr_socket_timeout_get(s, &old_timeout);
        if (arv != APR_SUCCESS) {
            return arv;
        }
        arv = apr_socket_timeout_set(s, 0);
        if (arv != APR_SUCCESS) {
            return arv;
        }
        rv = apr_socket_sendfile(s, fd, NULL, &file_offset, &n, 0);
        if (rv == APR_SUCCESS) {
            bytes_written += n;
            file_offset += n;
        }
        arv = apr_socket_timeout_set(s, old_timeout);
        if ((arv != APR_SUCCESS) && (rv == APR_SUCCESS)) {
            rv = arv;
        }
    }
    if ((ap__logio_add_bytes_out != NULL) && (bytes_written > 0)) {
        ap__logio_add_bytes_out(c, bytes_written);
    }
    *cumulative_bytes_written += bytes_written;
    if ((bytes_written < file_length) && (bytes_written > 0)) {
        apr_bucket_split(bucket, bytes_written);
        apr_bucket_delete(bucket);
    }
    else if (bytes_written == file_length) {
        apr_bucket_delete(bucket);
    }
    return rv;
}

#endif
