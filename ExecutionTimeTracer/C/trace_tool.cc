#include "trace_tool.h"
#include <pthread.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <thread>
#include <mutex>
#include <memory>

using std::ifstream;
using std::ofstream;
using std::getline;
using std::ofstream;
using std::vector;
using std::endl;
using std::string;
using std::to_string;
using std::set;
using std::thread;
using std::mutex;

#define TARGET_PATH_COUNT 0
#define NUMBER_OF_FUNCTIONS 0
#define LATENCY
#define MONITOR

ulint transaction_id = 0;

class TraceTool {
private:
    static TraceTool *instance;
    /*!< Start time of the current transaction. */
    vector<vector<int> > function_times;
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
    void add_record(int function_index, long duration);
};

class FunctionLog {
    public:
        FunctionLog():
        semIntervalID(-1), threadID(std::thread::id()) {}

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

class OperationLog {
    public:
        OperationLog(): 
        semIntervalID(-1), obj(nullptr), op(MUTEX_LOCK), threadID(std::thread::id()) {}

        OperationLog(const void* _obj, Operation _op):
        semIntervalID(TraceTool::current_transaction_id), obj(_obj), 
        op(_op), threadID(std::this_thread::get_id()) {}

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

TraceTool *TraceTool::instance = NULL;
__thread ulint TraceTool::current_transaction_id = 0;

timespec TraceTool::global_last_query;

ofstream TraceTool::log_file;

__thread int TraceTool::path_count = 0;
__thread bool TraceTool::is_commit = false;
__thread bool TraceTool::commit_successful = true;
__thread timespec TraceTool::trans_start;

bool TraceTool::should_shutdown = false;
pthread_t TraceTool::back_thread;

thread_local OperationLog SynchronizationTraceTool::currOpLog;
thread_local FunctionLog SynchronizationTraceTool::currFuncLog;
std::mutex SynchronizationTraceTool::dataMutex;
pid_t SynchronizationTraceTool::lastPID;

std::unique_ptr<SynchronizationTraceTool> SynchronizationTraceTool::instance = nullptr;
mutex SynchronizationTraceTool::singletonMutex;

/* Define MONITOR if needs to trace running time of functions. */
#ifdef MONITOR
static __thread timespec function_start;
static __thread timespec function_end;
static __thread timespec call_start;
static __thread timespec call_end;
#endif

void set_id(int id) {
    TraceTool::get_instance()->id = id;
    if (!TraceTool::log_file.is_open()) {
        TraceTool::log_file.open("logs/log_file_" + to_string(id));
    }
}

int get_thread_id() {
    return TraceTool::get_instance()->id;
}

pthread_t get_thread() {
    return TraceTool::back_thread;
}

void set_should_shutdown(int shutdown) {
    TraceTool::should_shutdown = shutdown;
}

void log_command(const char *command) {
    TraceTool::get_instance()->log_file << "[Thread " << pthread_self() << "]: " << command << endl;
}

void QUERY_START() {
    TraceTool::get_instance()->global_last_query = TraceTool::get_time();
}

void SESSION_START() {
#ifdef LATENCY
    TraceTool::get_instance()->start_trx();
#endif
}

void SESSION_END(int successful) {
#ifdef LATENCY
    TraceTool::get_instance()->is_commit = true;
    TraceTool::get_instance()->commit_successful = successful;
    TraceTool::get_instance()->end_trx();
#endif
}

void PATH_INC() {
#ifdef LATENCY
    TraceTool::get_instance()->path_count++;
#endif
}

void PATH_DEC() {
#ifdef LATENCY
    TraceTool::get_instance()->path_count--;
#endif
}

void PATH_SET(int path_count) {
#ifdef LATENCY
    TraceTool::get_instance()->path_count = path_count;
#endif
}

int PATH_GET() {
#ifdef LATENCY
    return TraceTool::get_instance()->path_count;
#endif
}

void TRACE_FUNCTION_START() {
#ifdef MONITOR
    if (TraceTool::should_monitor()) {
        clock_gettime(CLOCK_REALTIME, &function_start);
    }
#endif
}

void TRACE_FUNCTION_END() {
#ifdef MONITOR
    if (TraceTool::should_monitor()) {
        clock_gettime(CLOCK_REALTIME, &function_end);
        long duration = TraceTool::difftime(function_start, function_end);
        TraceTool::get_instance()->add_record(0, duration);
    }
#endif
}

int TRACE_START() {
#ifdef MONITOR
    if (TraceTool::should_monitor()) {
        clock_gettime(CLOCK_REALTIME, &call_start);
    }
#endif
    return 0;
}

int TRACE_END(int index) {
#ifdef MONITOR
    if (TraceTool::should_monitor()) {
        clock_gettime(CLOCK_REALTIME, &call_end);
        long duration = TraceTool::difftime(call_start, call_end);
        TraceTool::get_instance()->add_record(index, duration);
    }
#endif
    return 0;
}

void SYNCHRONIZATION_CALL_START(Operation op, void* obj) {
    SynchronizationTraceTool::SynchronizationCallStart(static_cast<Operation>(op), obj);
}

void SYNCHRONIZATION_CALL_END() {
    SynchronizationTraceTool::SynchronizationCallEnd();
}


timespec get_trx_start() {
    return TraceTool::get_instance()->trans_start;
}

/********************************************************************//**
Get the current TraceTool instance. */
TraceTool *TraceTool::get_instance() {
    if (instance == NULL) {
        instance = new TraceTool;
#ifdef LATENCY
        /* Create a background thread for dumping function running time
           and latency data. */
        pthread_create(&back_thread, NULL, check_write_log, NULL);
#endif
    }
    return instance;
}

TraceTool::TraceTool() : function_times() {
    /* Open the log file in append mode so that it won't be overwritten */
    const int number_of_functions = NUMBER_OF_FUNCTIONS + 1;
    vector<int> function_time;
    function_time.push_back(0);
    for (int index = 0; index < number_of_functions; index++) {
        function_times.push_back(function_time);
        function_times[index].reserve(500000);
    }
    transaction_start_times.reserve(500000);
    transaction_start_times.push_back(0);

    srand(time(0));
}

bool TraceTool::should_monitor() {
    return path_count == TARGET_PATH_COUNT;
}

void *TraceTool::check_write_log(void *arg) {
    /* Runs in an infinite loop and for every 5 seconds,
       check if there's any query comes in. If not, then
       dump data to log files. */
    while (true) {
        sleep(5);
        timespec now = get_time();
        if (now.tv_sec - global_last_query.tv_sec >= 10 && transaction_id > 0) {
            /* Create a new TraceTool instance. */
            TraceTool *old_instance = instance;
            instance = new TraceTool;
            instance->id = old_instance->id;

            /* Reset the global transaction ID. */
            transaction_id = 0;

            /* Dump data in the old instance to log files and
               reclaim memory. */
            old_instance->write_log();
            delete old_instance;
        }

        if (now.tv_sec - global_last_query.tv_sec >= 5 && should_shutdown) {
            break;
        }
    }
    return NULL;
}

timespec TraceTool::get_time() {
    timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return now;
}

long TraceTool::difftime(timespec start, timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
}

ulint TraceTool::now_micro() {
    timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return now.tv_sec * 1000000 + now.tv_nsec / 1000;
}

/********************************************************************//**
Start a new query. This may also start a new transaction. */
void TraceTool::start_trx() {
    is_commit = false;
    /* This happens when a log write happens, which marks the end of a phase. */
    if (current_transaction_id > transaction_id) {
        current_transaction_id = 0;
    }
#ifdef LATENCY
    commit_successful = true;
    /* Use a write lock here because we are appending content to the vector. */
    current_transaction_id = transaction_id++;
    transaction_start_times[current_transaction_id] = now_micro();
    for (vector<vector<int> >::iterator iterator = function_times.begin();
         iterator != function_times.end();
         ++iterator) {
        iterator->push_back(0);
    }
    transaction_start_times.push_back(0);
    clock_gettime(CLOCK_REALTIME, &global_last_query);
    trans_start = get_time();
#endif
}

void TraceTool::end_trx() {
#ifdef LATENCY
    if (is_commit) {
        end_transaction();
    }
#endif
}

void TraceTool::end_transaction() {
#ifdef LATENCY
    timespec now = get_time();
    long latency = difftime(trans_start, now);
    function_times.back()[current_transaction_id] = (int) latency;
    if (!commit_successful) {
        transaction_start_times[current_transaction_id] = 0;
    }
    is_commit = false;
#endif
}

void TraceTool::add_record(int function_index, long duration) {
    if (current_transaction_id > transaction_id) {
        current_transaction_id = 0;
    }
    function_times[function_index][current_transaction_id] += duration;
}

void TraceTool::write_latency(string dir) {
    log_file << "Thread is " << pthread_self() << endl;
    ofstream tpcc_log;
    tpcc_log.open(dir + "tpcc_" + to_string(id));

    for (ulint index = 0; index < transaction_start_times.size(); ++index) {
        ulint start_time = transaction_start_times[index];
        if (start_time > 0) {
            tpcc_log << start_time << endl;
        }
    }

    int function_index = 0;
    for (vector<vector<int> >::iterator iterator = function_times.begin();
         iterator != function_times.end(); ++iterator) {
        ulint number_of_transactions = iterator->size();
        for (ulint index = 0; index < number_of_transactions; ++index) {
            if (transaction_start_times[index] > 0) {
                long latency = (*iterator)[index];
                tpcc_log << function_index << ',' << latency << endl;
            }
        }
        function_index++;
        vector<int>().swap(*iterator);
    }
    vector<vector<int> >().swap(function_times);
    tpcc_log.close();
}

void TraceTool::write_log() {
//    log_file << "Write log on instance " << instance << ", id is " << id << endl;
    if (id > 0) {
        write_latency("latency/");
    }
}

SynchronizationTraceTool::SynchronizationTraceTool() {
    opLogs = new vector<OperationLog>;
    funcLogs = new vector<FunctionLog>;
    doneWriting = false;

    opLogs->reserve(1000000);
    funcLogs->reserve(1000000);

    lastPID = ::getpid();

    logFile.open("latency/SynchronizationLog_" + std::to_string(lastPID), std::ios_base::trunc);

    writerThread = thread(writeLogWorker);
}

SynchronizationTraceTool::~SynchronizationTraceTool() {
    dataMutex.lock();
    doneWriting = true;
    dataMutex.unlock();

    writerThread.join();
}

void SynchronizationTraceTool::SynchronizationCallStart(Operation op, void *obj) {
    if (instance == nullptr) {
        maybeCreateInstance();
    }

    dataMutex.lock();
    instance->opLogs->push_back(currOpLog);
    dataMutex.unlock();

    currFuncLog = FunctionLog(TraceTool::current_transaction_id);
    timespec startTime;
    clock_gettime(CLOCK_REALTIME, &startTime);
    currFuncLog.setFunctionStart(startTime);
}

void SynchronizationTraceTool::SynchronizationCallEnd() {
    timespec endTime;
    clock_gettime(CLOCK_REALTIME, &endTime);
    currFuncLog.setFunctionEnd(endTime);

    dataMutex.lock();
    instance->funcLogs->push_back(currFuncLog);
    dataMutex.unlock();
}

void SynchronizationTraceTool::maybeCreateInstance() {
    singletonMutex.lock();

    if (instance == nullptr) {
        instance = std::unique_ptr<SynchronizationTraceTool>(new SynchronizationTraceTool());
    }

    singletonMutex.unlock();
}

void SynchronizationTraceTool::checkFileClean() {
    pid_t currPID = ::getpid();

    if (currPID != lastPID) {
        instance->logFile.close();
        instance->logFile.open("latency/SynchronizationLog_" + std::to_string(currPID), std::ios_base::trunc);
    }

    lastPID = currPID;
}

void SynchronizationTraceTool::writeLogWorker() {
    bool stopLogging = false;

    // Loop forever writing logs
    while (!stopLogging) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (instance != nullptr) {
            checkFileClean();

            vector<OperationLog> *newOpLogs = new vector<OperationLog>;
            vector<FunctionLog> *newFuncLogs = new vector<FunctionLog>;

            // TODO TODO TODO experiment with this size!!!!
            newOpLogs->reserve(instance->opLogs->size() * 4);
            newFuncLogs->reserve(instance->funcLogs->size() * 4);

            dataMutex.lock();

            vector<OperationLog> *oldOpLogs = instance->opLogs;
            vector<FunctionLog> *oldFuncLogs = instance->funcLogs;

            instance->opLogs = newOpLogs;
            instance->funcLogs = newFuncLogs;

            if (instance->doneWriting) {
                stopLogging = true;
            }

            dataMutex.unlock();

            writeLogs(oldOpLogs, oldFuncLogs);
        }
    }
}

void SynchronizationTraceTool::writeLogs(vector<OperationLog> *opLogs, 
                                         vector<FunctionLog> *funcLogs) {
    for (OperationLog &opLog : *opLogs) {
        instance->logFile << "0," << opLog;
    }

    for (FunctionLog &funcLog : *funcLogs) {
        instance->logFile << "1," << funcLog;
    }

    delete opLogs;
    delete funcLogs;
}

std::ostream& operator<<(std::ostream &os, const Operation &op) {
    switch (op) {
        case MUTEX_LOCK:
            os << "ML";
            break;
        case MUTEX_UNLOCK:
            os << "MU";
            break;
        case CV_WAIT:
            os << "CVW";
            break;
        case CV_BROADCAST:
            os << "CVB";
            break;
        case CV_SIGNAL:
            os << "CVS";
            break;
        case QUEUE_ENQUEUE:
            os << "QE";
            break;
        case QUEUE_DEQUEUE:
            os << "QD";
            break;
        case MESSAGE_SEND:
            os << "MS";
            break;
        case MESSAGE_RECEIVE:
            os << "MR";
            break;
    }

    return os;
}

