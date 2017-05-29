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

/** This macro is used for tracing the running time of
    a function call which appears inside an if statement*/
#define TRACE_S_E(function_call, index) (TRACE_START()|(function_call)|TRACE_END(index))

typedef unsigned long int ulint;

/** The global transaction id counter */
extern ulint transaction_id;

pthread_t get_thread();

void set_should_shutdown(int shutdown);

void set_id(int id);

int get_thread_id();

void log_command(const char *command);

void SESSION_START();

void SESSION_END(int successful);

void PATH_SET(int path_count);

int PATH_GET();

void PATH_INC();

void PATH_DEC();

timespec get_trx_start();

/********************************************************************//**
This function marks the start of a function call */
void TRACE_FUNCTION_START();

/********************************************************************//**
This function marks the end of a function call */
void TRACE_FUNCTION_END();

/********************************************************************//**
This function marks the start of a child function call. */
int TRACE_START();

/********************************************************************//**
This function marks the end of a child function call. */
int TRACE_END(
  int index);   /*!< Index of the child function call. */

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
