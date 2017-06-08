#ifndef MY_TRACE_TOOL_H
#define MY_TRACE_TOOL_H

#include <pthread.h>
#include <time.h>
#include <sys/types.h>

enum Operation  { MUTEX_LOCK,
                  MUTEX_UNLOCK,
                  CV_WAIT,
                  CV_BROADCAST,
                  CV_SIGNAL,
                  QUEUE_ENQUEUE,
                  QUEUE_DEQUEUE,
                  MESSAGE_SEND,
                  MESSAGE_RECEIVE };

typedef struct timespec timespec;
typedef enum Operation Operation;

#ifdef __cplusplus
extern "C" {
#endif

void TARGET_PATH_SET(int pathCount);

void SESSION_START(const char *SIID);

void SWITCH_SI(const char *SIID);

void SESSION_END(int successful);

void TARGET_PATH_SET(int pathCount);

int PATH_GET();

void PATH_INC(int expectedCount);

void PATH_DEC(int expectedCount);

void TRACE_FUNCTION_START(int numFuncs);

void TRACE_FUNCTION_END();

int TRACE_START();

int TRACE_END(int index);

/********************************************************************//**
These functions are called by the generated wrappers. */
void SYNCHRONIZATION_CALL_START(Operation op, void* obj);
void SYNCHRONIZATION_CALL_END();

void ON_MKNOD(const char *path, mode_t mode);
void ON_OPEN(const char *path, int fd);
size_t ON_READ(int fd, void *buf, size_t nbytes);
size_t ON_WRITE(int fd, const void *buf, size_t nbytes);
void ON_CLOSE(int fd);
void ON_PIPE(int pipefd[2]);
void ON_MSGGET(int msqid);
int ON_MSGSND(int fd, const void *msgp, size_t msgsz, int msgflg);
ssize_t ON_MSGRCV(int fd, void *msgp, size_t msgsz, long msgtyp, int msgflg);

#ifdef __cplusplus
}
#endif

#endif
