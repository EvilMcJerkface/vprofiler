#ifndef MY_TRACE_TOOL_H
#define MY_TRACE_TOOL_H

// C libs
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

// C++ libs
#include <fstream>
#include <vector>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <atomic>

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
extern std::unique_ptr<std::atomic_uint_fast64_t> transaction_id;

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

enum Operation  { MUTEX_LOCK,
                  MUTEX_UNLOCK,
                  CV_WAIT,
                  CV_BROADCAST,
                  CV_SIGNAL,
                  QUEUE_ENQUEUE,
                  QUEUE_DEQUEUE,
                  MESSAGE_SEND,
                  MESSAGE_RECEIVE };
typedef enum Operation Operation;

// NOTE we're keeping one of these for each synchronization and traced
// function instance.  In particular, for non-synchronization calls
// storing semIntervalID is redundant.  If we have problems with
// too much overhead, change this.
class FunctionLog {
    public:
        FunctionLog():
        semIntervalID(-1), entityID(std::to_string(pthread_self()) + "_" + std::to_string(::getpid())) {}

        FunctionLog(unsigned int _semIntervalID):
        semIntervalID(_semIntervalID) {
            entityID = std::to_string(pthread_self()) + "_" + std::to_string(::getpid());
        }

        FunctionLog(unsigned int _semIntervalID, 
                    timespec _functionStart,
                    timespec _functionEnd): semIntervalID(_semIntervalID),
                    functionStart(_functionStart), functionEnd(_functionEnd) {
            entityID = std::to_string(pthread_self()) + "_" + std::to_string(::getpid());
        }

        void setFunctionStart(timespec val) {
            functionStart = val;
        }

        void setFunctionEnd(timespec val) {
            functionEnd = val;
        }

        void appendToString(string &other) const {
            //std::stringstream ss;
            //ss << threadID;
            // ss.str() insert this in beginning of string call
            other.append(string("1," + entityID + ',' + std::to_string(semIntervalID) 
                                + ',' + std::to_string((functionStart.tv_sec * 1000000000) 
                                + functionStart.tv_nsec) + ',' 
                                + std::to_string((functionEnd.tv_sec * 1000000000) + functionEnd.tv_nsec) + '\n'));
        }

        friend std::ostream& operator<<(std::ostream &os, const FunctionLog &funcLog) {
            os << string(funcLog.entityID + ',' + std::to_string(funcLog.semIntervalID) 
                         + ',' + std::to_string((funcLog.functionStart.tv_sec * 1000000000) 
                         + funcLog.functionStart.tv_nsec) + ',' 
                         + std::to_string((funcLog.functionEnd.tv_sec * 1000000000) 
                         + funcLog.functionEnd.tv_nsec));

            return os;
        }

        friend std::string& operator+=(std::string &str, const FunctionLog &funcLog) {
            funcLog.appendToString(str);

            return str;
        }

    private:
        string entityID;
        unsigned int semIntervalID;

        timespec functionStart;
        timespec functionEnd;
};

struct SharedMem {
    std::atomic_uint_fast64_t transaction_id;
    bool sharedMemInitialized;
};

class VProfSharedMemory {
    private:
        static std::shared_ptr<VProfSharedMemory> instance;
        static bool singletonInitialized;

        const char* GetExecutableName() const;

        VProfSharedMemory();
    public:
        static std::shared_ptr<VProfSharedMemory> GetInstance();
        std::atomic_uint_fast64_t *transaction_id;
};

class TraceTool {
private:
    static TraceTool *instance;
    /*!< Start time of the current transaction. */
    vector<vector<FunctionLog> > function_times;
    /*!< Stores the running time of the child functions
                                                 and also transaction latency (the last one). */
    vector<ulint> transaction_start_times;
    /*!< Stores the start time of transactions. */

    TraceTool();

    TraceTool(TraceTool const &) { };
public:
    static timespec global_last_query;
    static __thread timespec trans_start;
    static __thread ulint current_transaction_id;
    /*!< Each thread can execute only one transaction at
                                                          a time. This is the ID of the current transactions. */

    static __thread int path_count;
    /*!< Number of node in the function call path. Used for
                                            tracing running time of functions. */

    static __thread bool is_commit;
    /*!< True if the current transactions commits. */
    static __thread bool commit_successful; /*!< True if the current transaction successfully commits. */
    static bool should_shutdown;
    static pthread_t back_thread;
    static ofstream log_file;

    /*!< Maps the process local transaction ID to the global transaction ID. */
    static std::unordered_map<uint, uint> processToGlobalSID;

    int id;

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
    Get the current time in microsecond. */
    static ulint now_micro();

    /********************************************************************//**
    Start a new query. This may also start a new transaction. */
    void start_trx();
    /********************************************************************//**
    End a new query. This may also end the current transaction. */
    void end_trx();
    /********************************************************************//**
    End the current transaction. */
    void end_transaction();
    /********************************************************************//**
    Dump data about function running time and latency to log file. */
    void write_latency(string dir);
    /********************************************************************//**
    Write necessary data to log files. */
    void write_log();

    /********************************************************************//**
    Record running time of a function. */
    void add_record(int function_index, timespec &start, timespec &end);
};

class OperationLog {
    public:
        OperationLog(): 
        semIntervalID(-1), obj(nullptr), op(MUTEX_LOCK), 
        entityID(std::to_string(pthread_self()) + "_" + std::to_string(::getpid())) {}


        OperationLog(const void* _obj, Operation _op):
        semIntervalID(TraceTool::processToGlobalSID[TraceTool::current_transaction_id]), obj(_obj), 
        op(_op), entityID(std::to_string(pthread_self()) + "_" + 
                          std::to_string(::getpid())) {}

        unsigned int getSemIntervalID() {
            return semIntervalID;
        }

        const void* getObj() {
            return obj;
        }

        void appendToString(string &other) const {
            std::stringstream ss2;
            ss2 << obj;
            other.append(string("0," + entityID + ',' + std::to_string(semIntervalID) + ',' + ss2.str() + ',' + 
                                std::to_string(op) + '\n'));
        }

        friend std::string& operator+=(std::string &str, const OperationLog &funcLog) {
            funcLog.appendToString(str);

            return str;
        }

    private:
        string entityID;
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
        static thread_local OperationLog currOpLog;

        std::ofstream logFile;

        static pid_t lastPID;

        std::vector<OperationLog> *opLogs;
        std::vector<FunctionLog> *funcLogs;
        static std::mutex dataMutex;

        std::thread writerThread;
        bool doneWriting;

        static void maybeCreateInstance();

        SynchronizationTraceTool();

        static void checkFileClean();
        static void writeLogWorker();
        static void writeLogs(std::vector<OperationLog> *opLogs,
                             std::vector<FunctionLog> *funcLogs);
};

#endif
