#ifndef MY_TRACE_TOOL_H
#define MY_TRACE_TOOL_H

#include <fstream>
#include <vector>
#include <pthread.h>
#include <time.h>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <memory>

#define TRX_TYPES 6

/** This macro is used for tracing the running time of
    a function call which appears inside an if statement*/
#define TRACE_S_E(function_call, index) (TRACE_START()|(function_call)|TRACE_END(index))

#define var_mutex_enter() pthread_mutex_lock(&TraceTool::var_mutex)
#define var_mutex_exit() pthread_mutex_unlock(&TraceTool::var_mutex)
#define estimate_mutex_enter() pthread_mutex_lock(&TraceTool::last_second_mutex); pthread_rwlock_rdlock(&TraceTool::data_lock);
#define estimate_mutex_exit() pthread_mutex_unlock(&TraceTool::last_second_mutex); pthread_rwlock_unlock(&TraceTool::data_lock)

typedef unsigned long int ulint;
typedef unsigned int uint;

using std::ofstream;
using std::vector;
using std::endl;
using std::string;

/** The global transaction id counter */
extern ulint transaction_id;

/********************************************************************//**
To break down the variance of a function, we need to trace the running
time of a function and the functions it calls. */

/********************************************************************//**
This function marks the start of a function call */
void TRACE_FUNCTION_START();

/********************************************************************//**
This function marks the end of a function call */
void TRACE_FUNCTION_END();

/********************************************************************//**
This function marks the start of a child function call. */
bool TRACE_START();

/********************************************************************//**
This function marks the end of a child function call. */
bool TRACE_END(
  int index);   /*!< Index of the child function call. */

void SESSION_START();
void SESSION_END(bool successfully);

/********************************************************************//**
These functions are called by the generated wrappers. */
void SYNCHRONIZATION_CALL_START(int op, void* obj);
void SYNCHRONIZATION_CALL_END();

/********************************************************************//**
Transaction types in TPCC workload. */
enum transaction_type
{
    NEW_ORDER, PAYMENT, ORDER_STATUS, DELIVERY, STOCK_LEVEL, NONE
};
typedef enum transaction_type transaction_type;

// NOTE we're keeping one of these for each synchronization and traced
// function instance.  In particular, for non-synchronization calls
// storing semIntervalID is redundant.  If we have problems with
// too much overhead, change this.
class FunctionLog {
    public:
        FunctionLog(unsigned int _semIntervalID):
        semIntervalID(_semIntervalID) {
            threadID = std::this_thread::get_id();
        }

        FunctionLog(unsigned int _semIntervalID, 
                    timespec _functionStart,
                    timespec _functionEnd): semIntervalID(_semIntervalID),
                    functionStart(_functionStart), functionEnd(_functionEnd) {
            threadID = std::this_thread::get_id();
        }

        void setFunctionStart(timespec val) {
            functionStart = val;
        }

        void setFunctionEnd(timespec val) {
            functionEnd = val;
        }

        friend std::ostream& operator<<(std::ostream &os, const FunctionLog &funcLog) {
            os << funcLog.threadID << ',' << std::to_string(funcLog.semIntervalID) 
               << ',' << (funcLog.functionStart.tv_sec * 1000000000) + funcLog.functionStart.tv_nsec << ',' 
               << (funcLog.functionEnd.tv_sec * 1000000000) + funcLog.functionEnd.tv_nsec << '\n';

            return os;
        }

    private:
        std::thread::id threadID;
        unsigned int semIntervalID;

        timespec functionStart;
        timespec functionEnd;
};

class TraceTool
{
private:
    static TraceTool *instance;             /*!< Instance for the Singleton pattern. */
    static pthread_mutex_t instance_mutex;  /*!< Mutex for protecting instance. */
    
    static timespec global_last_query;      /*!< Time when MySQL receives the most recent query. */
    static pthread_mutex_t last_query_mutex;/*!< Mutex for protecting global_last_query */
    
    static __thread timespec function_start;/*!< Time for the start of a function call. */
    static __thread timespec function_end;  /*!< Time for the end of a function call. */
    static __thread timespec call_start;    /*!< Time for the start of a child function call. */
    static __thread timespec call_end;      /*!< Time for the end of a child function call. */
    static __thread timespec trans_start;   /*!< Start time of the current transaction. */
    
    ofstream log_file;                      /*!< An log file for outputing debug messages. */
    
    vector<vector<vector<FunctionLog>>> function_times;  /*!< Stores the running time of the child functions
                                                              and also transaction latency (the last one). */
    vector<ulint> transaction_start_times;  /*!< Stores the start time of transactions. */
    vector<transaction_type> transaction_types;/*!< Stores the transaction types of transactions. */
    
    TraceTool();
    TraceTool(TraceTool const&){};
public:
    static pthread_rwlock_t data_lock;      /*!< A read-write lock for protecting function_times. */
    static __thread ulint current_transaction_id;   /*!< Each thread can execute only one transaction at
                                                         a time. This is the ID of the current transactions. */
    
    // This used to be private, but there was a compiler error where a 
    // non-member function was using this.  This is a temporary fix,
    // but I'm not sure if it's one that should necessarily stay.
    static __thread bool new_transaction;   /*!< True if we need to start a new transaction. */

    static __thread int path_count;         /*!< Number of node in the function call path. Used for
                                                 tracing running time of functions. */
    
    static __thread bool is_commit;         /*!< True if the current transactions commits. */
    static __thread bool query_is_commit;   /*!< True if the current query is a commit query (Note that
                                                 these two doesn't have to be true at the same time). */
    static __thread bool commit_successful; /*!< True if the current transaction successfully commits. */
    static __thread transaction_type type;  /*!< Type of the current transaction. */

    
    /********************************************************************//**
    The Singleton pattern. Used for getting the instance of this class. */
    static TraceTool *get_instance();
    
    /********************************************************************//**
    Check if we should trace the running time of function calls. */
    static bool should_monitor();
    
    /********************************************************************//**
    Calculate time interval in nanoseconds. */
    static long difftime(timespec start, timespec end);
    
    /********************************************************************//**
    Periodically checks if any query comes in in the last 5 second.
    If no then dump all logs to disk. */
    static void *check_write_log(void *);
    
    /********************************************************************//**
    Get the current time in nanosecond. */
    static timespec get_time();
    /********************************************************************//**
    Get the current tiem in microsecond. */
    static ulint now_micro();
    
    /********************************************************************//**
    Returns the log file for outputing debug information. */
    ofstream &get_log()
    {
        return log_file;
    }
    
    /********************************************************************//**
    Start a new query. This may also start a new transaction. */
    void start_new_query();
    /********************************************************************//**
    End a new query. This may also end the current transaction. */
    void end_query();
    /********************************************************************//**
    End the current transaction. */
    void end_transaction();
    
    /********************************************************************//**
    Analysis the current query to find out the transaction type. */
    void set_query(const char *query);
    /********************************************************************//**
    Dump data about function running time and latency to log file. */
    void write_latency(string dir);
    /********************************************************************//**
    Write necessary data to log files. */
    void write_log();
    
    /********************************************************************//**
    Record running time of a function. */
    void add_record(int function_index, timespec &start_time, timespec &end_time);
};

enum Operation  { MUTEX_LOCK,
                  MUTEX_UNLOCK,
                  CV_WAIT,
                  CV_BROADCAST,
                  CV_SIGNAL,
                  QUEUE_ENQUEUE,
                  QUEUE_DEQUEUE,
                  MESSAGE_SEND,
                  MESSAGE_RECEIVE };

class OperationLog {
    public:
        OperationLog(const void* _obj, Operation _op):
        semIntervalID(TraceTool::current_transaction_id), obj(_obj), op(_op) {
            threadID = std::this_thread::get_id();
        }

        std::thread::id getThreadID() const {
            return threadID;
        }

        unsigned int getSemIntervalID() {
            return semIntervalID;
        }

        const void* getObj() {
            return obj;
        }

        friend std::ostream& operator<<(std::ostream &os, const OperationLog &log) {
            os << log.threadID << ',' << log.semIntervalID << ',' << log.obj 
               << ',' << log.op << '\n';

            return os;
        }

    private:
        std::thread::id threadID;
        unsigned int semIntervalID;
        const void* obj;
        Operation op;
};

class SynchronizationTraceTool {
    public:
        static void SynchronizationCallStart(Operation op, void* obj);

        static void SynchronizationCallEnd();

        ~SynchronizationTraceTool();

    private:
        static std::unique_ptr<SynchronizationTraceTool> instance;
        static std::mutex singletonMutex;

        static thread_local FunctionLog currFuncLog;

        std::ofstream funcLogFile;
        std::ofstream opLogFile;

        std::vector<OperationLog> *opLogs;
        std::vector<FunctionLog> *funcLogs;
        static pthread_rwlock_t data_lock;      

        static void maybeCreateInstance();

        SynchronizationTraceTool();

        static void writeLogWorker();
        static void writeLogs(std::vector<OperationLog> *opLogs,
                             std::vector<FunctionLog> *funcLogs);
};

#endif
