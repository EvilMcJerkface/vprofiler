int mknod_vprof(const char *pathname, mode_t mode, dev_t dev) {
    int result;
    result = mknod(pathname, mode, dev);
    mknod_cache_data(pathname, mode);
    return result;
}

int open_vprof(const char *path, int oflags) {
    int result;
    result = open(path, oflags);

    maybe_cache_file_des(path, result);

    return result;
}

int open_vprof(const char *path, int oflags, mode_t mode) {
    int result;
    result = open(path, oflags, mode);

    maybe_cache_file_des(path, result);

    return result;
}

int msgget_vprof(key_t key, int msgflg) {
    int result;
    
    result = msgget(key, msgflg);

    cache_msgqueue_id(result);

    return result;
}

int close_vprof(int filedes) {
    int result;
    result = close(filedes);

    maybe_remove_file_des_from_cache(filedes);

    return result;
}

int pipe_vprof(int pipefd[2]) {
    int result = pipe(pipefd);

    cache_pipe_data(pipefd);

    return result;
}

int pipe2_vprof(int pipefd[2], int flags) {
    int result = pipe2(pipefd, flags);

    cache_pipe_data(pipefd);

    return result;
}

ssize_t read_vprof(int filedes, void *buf, size_t nbytes) {
    ssize_t result;

    result = read(filedes, buf, nbytes);

    maybe_record_read(filedes);

    return result;
}

ssize_t write_vprof(int filedes, void *buf, size_t nbytes) {
    ssize_t result;
    
    result = write(filedes, buf, nbytes);

    maybe_record_read(filedes);

    return result;
}

int msgrcv_vprof(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg) {
    int result;

    result = msgrcv(msqid, msgp, msgsz, msgtyp, msgflg);
    
    record_msq_push(msqid);

    return result;
}

int msgsnd_vprof(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg) {
    int result;

    result = msgsnd(msqid, msgp, msgsz, msgtyp, msgflg);
    
    record_msq_pop(msqid);

    return result;
}
