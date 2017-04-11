// VProf headers
#include "trace_tool.h"

// C headers
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

// C++ headers
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <thread>
#include <mutex>
#include <memory>
#include <atomic>
#include <exception>
#include <unordered_map>

#include <boost/thread/shared_mutex.hpp>

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
using std::unordered_map;

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
#define WINDOWS
#include <Windows.h>
#endif

#define TARGET_PATH_COUNT 0
#define NUMBER_OF_FUNCTIONS 0
#define LATENCY
#define MONITOR

/*!< Process local transaction ID. */
std::atomic_uint_fast64_t processTransactionID{0};

struct SharedMem {
    std::atomic_uint_fast64_t transaction_id;
    std::atomic_uint_fast64_t refCount;
    bool sharedMemInitialized;
};

class VProfSharedMemory {
    private:
        static std::shared_ptr<VProfSharedMemory> instance;
        static bool singletonInitialized;
        SharedMem *shm;

        void GetExecutableName(char *filename) const;

        VProfSharedMemory();
    public:
        ~VProfSharedMemory();

        static std::shared_ptr<VProfSharedMemory> GetInstance();

        std::atomic_uint_fast64_t* GetTransactionID();
};

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

        void start() {
            clock_gettime(CLOCK_REALTIME, &functionStart);
        }

        void end() {
            clock_gettime(CLOCK_REALTIME, &functionEnd);
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
        semIntervalID(-1), op(MUTEX_LOCK), 
        entityID(std::to_string(pthread_self()) + "_" + std::to_string(::getpid())) {}


        OperationLog(const void* _obj, Operation _op): OperationLog(objToString(_obj), _op) {}

        OperationLog(string id, Operation _op):
        semIntervalID(TraceTool::processToGlobalSID[TraceTool::current_transaction_id]), objID(id), 
        op(_op), entityID(std::to_string(pthread_self()) + "_" + 
                          std::to_string(::getpid())) {}

        unsigned int getSemIntervalID() {
            return semIntervalID;
        }

        void appendToString(string &other) const {
            other.append(string("0," + entityID + ',' + std::to_string(semIntervalID) + ',' + objID + ',' + 
                                std::to_string(op) + '\n'));
        }

        friend std::string& operator+=(std::string &str, const OperationLog &funcLog) {
            funcLog.appendToString(str);

            return str;
        }

    private:
        static string objToString(const void *obj) {
            std::stringstream ss;
            ss << obj;
            return ss.str();
        }


        string entityID;
        unsigned int semIntervalID;
        string objID;
        Operation op;
};

class SynchronizationTraceTool {
    public:
        static void SynchronizationCallStart(Operation op, void* obj);
        static void SynchronizationCallEnd();
        static SynchronizationTraceTool *GetInstance();
        
        void AddFIFOName(const char *path);
        void OnOpen(const char *path, int fd);
        size_t OnRead(int fd, void *buf, size_t nbytes);
        size_t OnWrite(int fd, void *buf, size_t nbytes);
        void OnClose(int fd);
        void OnPipe(int pipefd[2]);
        void OnMsgGet(int msqid);
        int OnMsgSnd(int fd, void *msgp, size_t msgsz, int msgflg);
        ssize_t OnMsgRcv(int fd, void *msgp, size_t msgsz, long msgtyp, int msgflg);
        void LockFIFO(int fd);
        void UNLOCK_FIFO(int fd);

        ~SynchronizationTraceTool();

    private:
        static std::unique_ptr<SynchronizationTraceTool> instance;
        static std::mutex singletonMutex;

        static thread_local FunctionLog currFuncLog;
        static thread_local OperationLog currOpLog;

        boost::shared_mutex fifoNamesMutex;
        unordered_map<string, string> fifoNamesToIDs;
        boost::shared_mutex fifoFdsMutex;
        unordered_map<int, string> fifoFdsToIDs;
        unordered_map<int, mutex> fifoAccessLockTable;

        boost::shared_mutex pipeMutex;
        unordered_map<int, string> pipeFdsToIDs;
        unordered_map<string, mutex> pipeAccessLockTable;

        boost::shared_mutex msgMutex;
        unordered_map<int, string> msqids;
        boost::shared_mutex msqidMutex;
        unordered_map<int, mutex> msqAccessLockTable;

        std::ofstream logFile;

        static pid_t lastPID;

        std::vector<OperationLog> *opLogs;
        std::vector<FunctionLog> *funcLogs;
        static std::mutex dataMutex;

        static int numThingsLogged;

        std::thread writerThread;
        bool doneWriting;

        static void maybeCreateInstance();

        SynchronizationTraceTool();

        static void writeLogWorker();
        static void checkFileClean();
        static void writeLogs(std::vector<OperationLog> *opLogs,
                              std::vector<FunctionLog> *funcLogs);
};

std::shared_ptr<VProfSharedMemory> VProfSharedMemory::instance = nullptr;
bool VProfSharedMemory::singletonInitialized = false;

TraceTool *TraceTool::instance = NULL;
__thread ulint TraceTool::current_transaction_id = 0;
std::unordered_map<uint, uint> TraceTool::processToGlobalSID{{0, 0}};

timespec TraceTool::global_last_query;

ofstream TraceTool::log_file;

__thread int TraceTool::path_count = 0;
__thread bool TraceTool::is_commit = false;
__thread bool TraceTool::commit_successful = true;
__thread timespec TraceTool::trans_start;

bool TraceTool::should_shutdown = false;
pthread_t TraceTool::back_thread;

int SynchronizationTraceTool::numThingsLogged = 0;
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
        TraceTool::log_file.open("latency/log_file_" + to_string(id));
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
        TraceTool::get_instance()->add_record(0, function_start, function_end);
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
        TraceTool::get_instance()->add_record(index, call_start, call_end);
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

void VProfSharedMemory::GetExecutableName(char *filename) const {
    #ifdef WINDOWS
    GetModuleFilename(NULL, filename, MAX_PATH);

    // Eventually need some other directive for Mac OS
    #else
    readlink("/proc/self/exe", filename, PATH_MAX);

    #endif
}

std::atomic_uint_fast64_t* VProfSharedMemory::GetTransactionID() {
    return &shm->transaction_id;
}

VProfSharedMemory::VProfSharedMemory() {
    #ifdef WINDOWS
    wchar_t filename[MAX_PATH];
    #else
    char filename[PATH_MAX];
    #endif
    GetExecutableName(filename);

    int fd = shm_open("/vprofsharedmem", O_RDWR | O_CREAT | O_EXCL, 0666);

    // We're the first process to create the shared memory.
    if (fd != -1) {
        // Set shared memory size.
        // TODO handle error.
        if (ftruncate(fd, sizeof(VProfSharedMemory)) == -1) {
            // noop
            (void)0;
        }

        shm = static_cast<SharedMem*>(mmap(NULL, sizeof(VProfSharedMemory), 
                                                   PROT_READ | PROT_WRITE,
                                                   MAP_SHARED, fd, 0));
        shm->transaction_id = 0;
        shm->sharedMemInitialized = true;
        shm->refCount = 1;
    }
    else {
        fd = shm_open("/vprofsharedmem", O_RDWR, 0666);

        // Wait for file to be truncated
        struct stat fileStats;
        do {
            fstat(fd, &fileStats);
        } while(fileStats.st_size < 1);

        shm = static_cast<SharedMem*>(mmap(NULL, sizeof(VProfSharedMemory), 
                                                   PROT_READ | PROT_WRITE,
                                                   MAP_SHARED, fd, 0));

        // Wait for shm to be initialized. Maybe add cv in shm?
        while (!shm->sharedMemInitialized) {}

        shm->refCount++;
    }
}

VProfSharedMemory::~VProfSharedMemory() {
    if ((shm->refCount.fetch_sub(1) - 1) == 0) {
        shm_unlink("/vprofsharedmem");
    }
}

/********************************************************************//**
Get shared memory instance. */
std::shared_ptr<VProfSharedMemory> VProfSharedMemory::GetInstance() {
    if (!singletonInitialized) {
        instance = std::shared_ptr<VProfSharedMemory>(new VProfSharedMemory());

        singletonInitialized = true;
    }

    return instance;
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
    vector<FunctionLog> function_time;
    function_time.push_back(0);
    for (int index = 0; index < number_of_functions; index++) {
        function_times.push_back(function_time);
        function_times[index].reserve(500000);
    }
    transaction_start_times.reserve(500000);
    transaction_start_times.push_back(0);

    processToGlobalSID.reserve(500000);

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
        if (now.tv_sec - global_last_query.tv_sec >= 10 && VProfSharedMemory::GetInstance()->GetTransactionID()->load() > 0) {
            /* Create a new TraceTool instance. */
            TraceTool *old_instance = instance;
            instance = new TraceTool;
            instance->id = old_instance->id;

            /* Reset the global transaction ID. */
            //VProfSharedMemory::GetInstance()->GetTransactionID()->store(0);
            processTransactionID.store(0);


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
    if (processToGlobalSID[current_transaction_id] > VProfSharedMemory::GetInstance()->GetTransactionID()->load()) {
        current_transaction_id = 0;
    }
#ifdef LATENCY
    commit_successful = true;
    /* Use a write lock here because we are appending content to the vector. */
    current_transaction_id = processTransactionID.fetch_add(1);
    processToGlobalSID[current_transaction_id] = VProfSharedMemory::GetInstance()->GetTransactionID()->fetch_add(1);
    transaction_start_times[current_transaction_id] = now_micro();
    for (vector<vector<FunctionLog> >::iterator iterator = function_times.begin();
         iterator != function_times.end();
         ++iterator) {
        iterator->push_back(FunctionLog(processToGlobalSID[current_transaction_id]));
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
    function_times.back()[current_transaction_id].setFunctionStart(trans_start);
    function_times.back()[current_transaction_id].setFunctionEnd(now);
    if (!commit_successful) {
        transaction_start_times[current_transaction_id] = 0;
    }
    is_commit = false;
#endif
}

void TraceTool::add_record(int function_index, timespec &start, timespec &end) {
    if (processToGlobalSID[current_transaction_id] > VProfSharedMemory::GetInstance()->GetTransactionID()->load()) {
        current_transaction_id = 0;
    }
    function_times[function_index][current_transaction_id].setFunctionStart(start);
    function_times[function_index][current_transaction_id].setFunctionEnd(end);
}

void TraceTool::write_latency(string dir) {
    log_file << pthread_self() << endl;
    ofstream tpcc_log;
    tpcc_log.open(dir + "tpcc_" + to_string(id));

    for (ulint index = 0; index < transaction_start_times.size(); ++index) {
        ulint start_time = transaction_start_times[index];
        if (start_time > 0) {
            tpcc_log << start_time << endl;
        }
    }

    int function_index = 0;
    for (vector<vector<FunctionLog> >::iterator iterator = function_times.begin();
         iterator != function_times.end(); ++iterator) {
        ulint number_of_transactions = iterator->size();
        for (ulint index = 0; index < number_of_transactions; ++index) {
            if (transaction_start_times[index] > 0) {
                tpcc_log << function_index << ',' << (*iterator)[index] << endl;
            }
        }
        function_index++;
        vector<FunctionLog>().swap(*iterator);
    }
    vector<vector<FunctionLog> >().swap(function_times);
    tpcc_log.close();
}

void TraceTool::write_log() {
//    log_file << "Write log on instance " << instance << ", id is " << id << endl;
    if (id > 0) {
        write_latency("latency/httpd_logs/");
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
    currFuncLog = FunctionLog(TraceTool::processToGlobalSID[TraceTool::current_transaction_id]);

    instance->opLogs->push_back(OperationLog(obj, op));
    dataMutex.unlock();

    currFuncLog.start();
}

void SynchronizationTraceTool::SynchronizationCallEnd() {
    currFuncLog.end();

    dataMutex.lock();
    instance->funcLogs->push_back(currFuncLog);
    dataMutex.unlock();
}

SynchronizationTraceTool* SynchronizationTraceTool::GetInstance() {
    if (instance == nullptr) {
        maybeCreateInstance();
    }
    return instance.get();
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
        std::this_thread::sleep_for(std::chrono::seconds(2));
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
    string writeStr = "";

    for (OperationLog &opLog : *opLogs) {
        writeStr += opLog;
    }

    for (FunctionLog &funcLog : *funcLogs) {
        writeStr += funcLog;
    }

    instance->logFile.write(writeStr.c_str(), writeStr.size());

    delete opLogs;
    delete funcLogs;
}

void ON_MKNOD(const char *path, int flags) {
    bool is_fifo = flags & ~S_IFIFO;
    if (is_fifo) {
        SynchronizationTraceTool::GetInstance()->AddFIFOName(path);
    }
}

void ON_OPEN(const char *path, int fd) {
    SynchronizationTraceTool::GetInstance()->OnOpen(path, fd);
}

size_t ON_READ(int fd, void *buf, size_t nbytes) {
    return SynchronizationTraceTool::GetInstance()->OnRead(fd, buf, nbytes);
}

size_t ON_WRITE(int fd, void *buf, size_t nbytes) {
    return SynchronizationTraceTool::GetInstance()->OnWrite(fd, buf, nbytes);
}

void ON_CLOSE(int fd) {
    SynchronizationTraceTool::GetInstance()->OnClose(fd);
}

void ON_PIPE(int pipefd[2]) {
    SynchronizationTraceTool::GetInstance()->OnPipe(pipefd);
}

void ON_MSGGET(int msqid) {
    SynchronizationTraceTool::GetInstance()->OnMsgGet(msqid);
}

int ON_MSGSND(int fd, void *msgp, size_t msgsz, int msgflg) {
    return SynchronizationTraceTool::GetInstance()->OnMsgSnd(fd, msgp, msgsz, msgflg);
}

ssize_t ON_MSGRCV(int fd, void *msgp, size_t msgsz, long msgtyp, int msgflg) {
    return SynchronizationTraceTool::GetInstance()->OnMsgRcv(fd, msgp, msgsz, msgtyp, msgflg);
}

void SynchronizationTraceTool::AddFIFOName(const char *path_cstr) {
    string path(path_cstr);
    boost::unique_lock<boost::shared_mutex> lock(fifoNamesMutex);
    string ID = "F" + std::to_string(fifoNamesToIDs.size());
    fifoNamesToIDs[path] = ID;
}

void SynchronizationTraceTool::OnOpen(const char *path_cstr, int fd) {
    unordered_map<string, string>::iterator it = fifoNamesToIDs.end();
    {
        string path(path_cstr);
        boost::shared_lock<boost::shared_mutex> readFIFONamesLock(fifoNamesMutex);
        it = fifoNamesToIDs.find(path);
        if (it == fifoNamesToIDs.end()) {
            return;
        }
    }
    boost::unique_lock<boost::shared_mutex> writeMutexTableLock(fifoFdsMutex);
    fifoFdsToIDs[fd] = it->second;
}

size_t SynchronizationTraceTool::OnRead(int fd, void *buf, size_t nbytes) {
    size_t result = 0;
    string ID;
    mutex *mutexToLock = nullptr;
    {
        boost::shared_lock<boost::shared_mutex> readIDLock(fifoFdsMutex);
        unordered_map<int, string>::iterator it = fifoFdsToIDs.end();
        if (it != fifoFdsToIDs.end()) {
            ID = it->second;
            mutexToLock = &fifoAccessLockTable[fd];
        }
    }
    if (mutexToLock == nullptr) {
        boost::shared_lock<boost::shared_mutex> readIDLock(pipeMutex);
        unordered_map<int, string>::iterator it = pipeFdsToIDs.end();
        if (it != pipeFdsToIDs.end()) {
            ID = it->second;
            mutexToLock = &pipeAccessLockTable[ID];
        }
    }
    if (mutexToLock != nullptr) {
        mutexToLock->lock();
        dataMutex.lock();
        opLogs->push_back(OperationLog(ID, MESSAGE_RECEIVE));
        currFuncLog = FunctionLog(TraceTool::processToGlobalSID[TraceTool::current_transaction_id]);
        dataMutex.unlock();
        currFuncLog.start();
        result = read(fd, buf, nbytes);
        mutexToLock->unlock();
        currFuncLog.end();
        dataMutex.lock();
        instance->funcLogs->push_back(currFuncLog);
        dataMutex.unlock();
    } else {
        result = read(fd, buf, nbytes);
    }
    return result;
}

size_t SynchronizationTraceTool::OnWrite(int fd, void *buf, size_t nbytes) {
    size_t result = 0;
    string ID;
    mutex *mutexToLock = nullptr;
    {
        boost::shared_lock<boost::shared_mutex> readIDLock(fifoFdsMutex);
        unordered_map<int, string>::iterator it = fifoFdsToIDs.end();
        if (it != fifoFdsToIDs.end()) {
            ID = it->second;
            mutexToLock = &fifoAccessLockTable[fd];
        }
    }
    if (mutexToLock == nullptr) {
        boost::shared_lock<boost::shared_mutex> readIDLock(pipeMutex);
        unordered_map<int, string>::iterator it = pipeFdsToIDs.end();
        if (it != pipeFdsToIDs.end()) {
            ID = it->second;
            mutexToLock = &pipeAccessLockTable[ID];
        }
    }
    if (mutexToLock != nullptr) {
        mutexToLock->lock();
        dataMutex.lock();
        opLogs->push_back(OperationLog(ID, MESSAGE_RECEIVE));
        dataMutex.unlock();
        currFuncLog = FunctionLog(TraceTool::processToGlobalSID[TraceTool::current_transaction_id]);
        currFuncLog.start();
        result = write(fd, buf, nbytes);
        mutexToLock->unlock();
        currFuncLog.end();
        dataMutex.lock();
        instance->funcLogs->push_back(currFuncLog);
        dataMutex.unlock();
    } else {
        result = write(fd, buf, nbytes);
    }
    return result;
}

void SynchronizationTraceTool::OnClose(int fd) {
    boost::unique_lock<boost::shared_mutex> writeFIFOFdLock(fifoFdsMutex);
    fifoFdsToIDs.erase(fd);
    writeFIFOFdLock.unlock();

    boost::unique_lock<boost::shared_mutex> writePipeFdLock(pipeMutex);
    pipeFdsToIDs.erase(fd);
    writePipeFdLock.unlock();
}

void SynchronizationTraceTool::OnPipe(int pipefd[2]) {
    boost::unique_lock<boost::shared_mutex> writePipeFdLock(pipeMutex);
    string ID = "P" + std::to_string(pipeFdsToIDs.size());
    pipeFdsToIDs[pipefd[0]] = ID;
    pipeFdsToIDs[pipefd[1]] = ID;
}

void SynchronizationTraceTool::OnMsgGet(int msqid) {
    string ID;
    {
        boost::unique_lock<boost::shared_mutex> writeMsqidLock(msgMutex);
        ID = "M" + std::to_string(msqid);
        msqids[msqid] = ID;
    }
    boost::unique_lock<boost::shared_mutex> writeMutexTableLock(msqidMutex);
    msqAccessLockTable[msqid];
}

int SynchronizationTraceTool::OnMsgSnd(int msqid, void *msgp, size_t msgsz, int msgflg) {
    mutex *mutexToLock = nullptr;
    string ID;
    {
        boost::shared_lock<boost::shared_mutex> readIDLock(msqidMutex);
        ID = msqids[msqid];
    }
    {
        boost::shared_lock<boost::shared_mutex> readMsqLock(msqidMutex);
        mutexToLock = &msqAccessLockTable[msqid];
    }
    mutexToLock->lock();

    dataMutex.lock();
    opLogs->push_back(OperationLog(ID, MESSAGE_SEND));
    dataMutex.unlock();

    currFuncLog = FunctionLog(TraceTool::processToGlobalSID[TraceTool::current_transaction_id]);
    currFuncLog.start();
    int result = msgsnd(msqid, msgp, msgsz, msgflg);
    mutexToLock->unlock();
    currFuncLog.end();
    
    dataMutex.lock();
    instance->funcLogs->push_back(currFuncLog);
    dataMutex.unlock();
    return result;
}

ssize_t SynchronizationTraceTool::OnMsgRcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg) {
    mutex *mutexToLock = nullptr;
    string ID;
    {
        boost::shared_lock<boost::shared_mutex> readIDLock(msqidMutex);
        ID = msqids[msqid];
    }
    {
        boost::shared_lock<boost::shared_mutex> readMsqLock(msqidMutex);
        mutexToLock = &msqAccessLockTable[msqid];
    }
    mutexToLock->lock();

    dataMutex.lock();
    opLogs->push_back(OperationLog(ID, MESSAGE_RECEIVE));
    dataMutex.unlock();

    currFuncLog = FunctionLog(TraceTool::processToGlobalSID[TraceTool::current_transaction_id]);
    currFuncLog.start();
    int result = msgrcv(msqid, msgp, msgsz, msgtyp, msgflg);
    mutexToLock->unlock();
    currFuncLog.end();

    dataMutex.lock();
    instance->funcLogs->push_back(currFuncLog);
    dataMutex.unlock();
    return result;
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


