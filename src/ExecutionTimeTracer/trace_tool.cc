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
#include <iostream>
#include <thread>
#include <mutex>
#include <memory>
#include <atomic>
#include <exception>
#include <unordered_map>

#include <boost/thread/shared_mutex.hpp>
#include <boost/filesystem.hpp>

using std::ifstream;
using std::ofstream;
using std::getline;
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

typedef unsigned long ulint;

class Filesystem {
    public:
        static void CreateDirIfNotExists(const string &dirName);

    private:
        static unordered_map<string, bool> dirInitialized;
};
unordered_map<string, bool> Filesystem::dirInitialized;

class FunctionLog {
    public:
        FunctionLog():
        semIntervalID("-1"), entityID(std::to_string(pthread_self()) + "_" + std::to_string(::getpid())) {}

        FunctionLog(std::string _semIntervalID):
        semIntervalID(_semIntervalID) {
            entityID = std::to_string(pthread_self()) + "_" + std::to_string(::getpid());
        }

        FunctionLog(std::string _semIntervalID, 
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

        void appendToString(std::string &other) const {
            other.append(std::string("1," + entityID + ',' + semIntervalID 
                                + ',' + std::to_string((functionStart.tv_sec * 1000000000) 
                                + functionStart.tv_nsec) + ',' 
                                + std::to_string((functionEnd.tv_sec * 1000000000) + functionEnd.tv_nsec) + '\n'));
        }

        friend std::ostream& operator<<(std::ostream &os, const FunctionLog &funcLog) {
            os << std::string(funcLog.entityID + ',' + funcLog.semIntervalID 
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

        std::string entityID;
        std::string semIntervalID;

        timespec functionStart;
        timespec functionEnd;
};

static int TARGET_PATH_COUNT = 0;
static thread_local int pathCount = 0;
static thread_local timespec function_start;
static thread_local timespec function_end;
static thread_local timespec call_start;
static thread_local timespec call_end;

class FunctionTracer {
public:
    static FunctionTracer *getInstance() {
        if (singleton == nullptr) {
            singletonMutex.lock();
            if (singleton == nullptr) {
                singleton = std::unique_ptr<FunctionTracer>(new FunctionTracer());
            }
            singletonMutex.unlock();
        }
        return singleton.get();
    }

    FunctionTracer() {
        Filesystem::CreateDirIfNotExists("latency");
        logFile.open("latency/FunctionLog_" + std::to_string(::getpid()), std::ios_base::trunc);
        shouldStop = false;
        writerThread = std::thread(writeLogs);
        lastPID = ::getpid();
    }

    ~FunctionTracer() {
        shouldStop = true;
        writerThread.join();
    }

    std::string getCurrentSIID() {
        return currentSIID;
    }

    void startSI(std::string SIID) {
        currentSIID = SIID;
        timespec transStart = get_time();
        siStartMutex.lock();
        siStarts[SIID] = transStart;
        siStartMutex.unlock();
    }

    void switchSI(std::string SIID) {
        currentSIID = SIID;
    }

    void endSI(bool successful) {
        timespec transStart;
        siStartMutex.lock();
        transStart = siStarts[currentSIID];
        siStarts.erase(currentSIID);
        siStartMutex.unlock();

        timespec transEnd = get_time();
        FunctionLog log(currentSIID, transStart, transEnd);
        localFunctionLogs.back().push_back(log);
        commitStatusMutex.lock();
        commitStatus[currentSIID] = successful;
        commitStatusMutex.unlock();
        submitToWriterThread();
    }

    void addRecord(int functionIndex, timespec &start, timespec &end) {
        FunctionLog log(currentSIID, start, end);
        localFunctionLogs[functionIndex].push_back(log);
    }

    void expandNumFuncs(int numFuncs) {
        std::vector<FunctionLog> tempVector;
        while (localFunctionLogs.size() < numFuncs + 2) {
            localFunctionLogs.push_back(tempVector);
        }
    }

private:
    static void writeLogs();

    static bool haveForkedSinceLastOp() {
        pid_t currPID = ::getpid();
        bool retVal = currPID != lastPID;
        lastPID = currPID;

        return retVal;
    }
    static void refreshStateAfterFork() {
        singleton->logFile.close();
        Filesystem::CreateDirIfNotExists("latency");
        singleton->logFile.open("latency/FunctionLog_" + std::to_string(::getpid()), std::ios_base::trunc);
        singleton->shouldStop = false;
        singleton->committedLogs.clear();
        singleton->writerThread.detach();
        singleton->writerThread = std::thread(writeLogs);
    }

    timespec get_time() {
        timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        return now;
    }
    void submitToWriterThread() {
        std::vector<std::vector<FunctionLog>> newLocalLogs;
        std::vector<FunctionLog> tempVector;
        for (size_t i = 0; i < localFunctionLogs.size(); ++i) {
            newLocalLogs.push_back(tempVector);
        }
        dataMutex.lock();
        commitStatusMutex.lock();
        while (committedLogs.size() < localFunctionLogs.size()) {
            committedLogs.push_back(tempVector);
        }
        for (size_t i = 0; i < localFunctionLogs.size(); ++i) {
            for (size_t j = 0; j < localFunctionLogs[i].size(); ++j) {
                FunctionLog log = localFunctionLogs[i][j];
                auto it = singleton->commitStatus.find(log.semIntervalID);
                // Not committed yet.
                if (it == singleton->commitStatus.end()) {
                    newLocalLogs[i].push_back(log);
                } else if (it->second) {
                    // Successfully committed
                    committedLogs[i].push_back(log);
                }
            }
        }
        commitStatusMutex.unlock();
        dataMutex.unlock();
        localFunctionLogs = newLocalLogs;
    }

    static std::unique_ptr<FunctionTracer> singleton;
    static std::mutex singletonMutex;

    static pid_t lastPID;

    static thread_local std::vector<std::vector<FunctionLog>> localFunctionLogs;

    std::unordered_map<std::string, timespec> siStarts;
    std::mutex siStartMutex;
    
    std::vector<std::vector<FunctionLog>> committedLogs;
    std::mutex dataMutex;
    std::ofstream logFile;

    std::unordered_map<std::string, bool> commitStatus;
    std::mutex commitStatusMutex;

    static thread_local std::string currentSIID;

    std::thread writerThread;
    bool shouldStop;
};

std::unique_ptr<FunctionTracer> FunctionTracer::singleton;
std::mutex FunctionTracer::singletonMutex;
pid_t FunctionTracer::lastPID;
thread_local std::vector<std::vector<FunctionLog>> FunctionTracer::localFunctionLogs;
thread_local std::string FunctionTracer::currentSIID;

void FunctionTracer::writeLogs() {
    while (!singleton->shouldStop) {
        std::cout << "[Writer] Sleeping for 5s" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (haveForkedSinceLastOp()) {
            refreshStateAfterFork();
        }
        std::vector<std::vector<FunctionLog>> logsToWrite;
        singleton->dataMutex.lock();
        logsToWrite.swap(singleton->committedLogs);
        singleton->dataMutex.unlock();
        
        if (logsToWrite.size() == 0) {
            std::cout << "[Writer] No log entry found." << std::endl;
            continue;
        }
        
        int count = 0;
        for (size_t i = 0; i < logsToWrite.size(); ++i) {
            for (size_t j = 0; j < logsToWrite[i].size(); ++j) {
                count++;
                singleton->logFile << i << "," << logsToWrite[i][j] << std::endl;
            }
        }
        std::cout << "[Writer] " << count << " entries written" << std::endl;
    }
    std::cout << "[Writer] Exiting" << std::endl;
    singleton->logFile.close();
}

void TARGET_PATH_SET(int pathCount) {
    TARGET_PATH_COUNT = pathCount;
}

void PATH_INC(int expectedCount) {
    if (pathCount == expectedCount) {
        pathCount++;
    }
}

void PATH_DEC(int expectedCount) {
    if (pathCount == expectedCount + 1) {
        pathCount--;
    }
}

int PATH_GET() {
    return pathCount;
}

void SESSION_START(const char *SIID) {
    FunctionTracer::getInstance()->startSI(std::string(SIID));
}

void SWITCH_SI(const char *SIID) {
    FunctionTracer::getInstance()->switchSI(std::string(SIID));
}

void SESSION_END(int successful) {
    FunctionTracer::getInstance()->endSI(successful);
}

void TRACE_FUNCTION_START(int numFuncs) {
    FunctionTracer::getInstance()->expandNumFuncs(numFuncs);
    if (pathCount == TARGET_PATH_COUNT) {
        clock_gettime(CLOCK_REALTIME, &function_start);
    }
}

void TRACE_FUNCTION_END() {
    if (pathCount == TARGET_PATH_COUNT) {
        clock_gettime(CLOCK_REALTIME, &function_end);
        FunctionTracer::getInstance()->addRecord(0, function_start, function_end);
    }
}

int TRACE_START() {
    if (pathCount == TARGET_PATH_COUNT) {
        clock_gettime(CLOCK_REALTIME, &call_start);
    }
    return 0;
}

int TRACE_END(int index) {
    if (pathCount == TARGET_PATH_COUNT) {
        clock_gettime(CLOCK_REALTIME, &call_end);
        FunctionTracer::getInstance()->addRecord(index, call_start, call_end);
    }
    return 0;
}

class OperationLog {
    public:
        OperationLog(): 
        semIntervalID("-1"), op(MUTEX_LOCK), 
        entityID(std::to_string(pthread_self()) + "_" + std::to_string(::getpid())) {}


        OperationLog(const void* _obj, Operation _op): OperationLog(objToString(_obj), _op) {}

        OperationLog(string id, Operation _op):
        semIntervalID(FunctionTracer::getInstance()->getCurrentSIID()), objID(id), 
        op(_op), entityID(std::to_string(pthread_self()) + "_" + 
                          std::to_string(::getpid())) {}

        std::string getSemIntervalID() {
            return semIntervalID;
        }

        void appendToString(string &other) const {
            other.append(string("0," + entityID + ',' + semIntervalID + ',' + objID + ',' + 
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
        std::string semIntervalID;
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
        size_t OnWrite(int fd, const void *buf, size_t nbytes);
        void OnClose(int fd);
        void OnPipe(int pipefd[2]);
        void OnMsgGet(int msqid);
        int OnMsgSnd(int fd, const void *msgp, size_t msgsz, int msgflg);
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
        ulint fifoIDCounter;
        unordered_map<string, string> fifoNamesToIDs;
        boost::shared_mutex fifoFdsMutex;
        unordered_map<int, string> fifoFdsToIDs;
        unordered_map<int, mutex> fifoAccessLockTable;

        boost::shared_mutex pipeMutex;
        ulint pipeIDCounter;
        unordered_map<int, string> pipeFdsToIDs;
        unordered_map<string, mutex> pipeAccessLockTable;

        boost::shared_mutex msgMutex;
        ulint msgIDCounter;
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

        template <typename T>
        static void pushToVec(std::vector<T> &vec, const T &val);

        static bool haveForkedSinceLastOp();
        static void refreshStateAfterFork();

        static void writeLogWorker();
        static void writeLogs(std::vector<OperationLog> *opLogs,
                              std::vector<FunctionLog> *funcLogs);
};
int SynchronizationTraceTool::numThingsLogged = 0;
thread_local OperationLog SynchronizationTraceTool::currOpLog;
thread_local FunctionLog SynchronizationTraceTool::currFuncLog;
std::mutex SynchronizationTraceTool::dataMutex;
pid_t SynchronizationTraceTool::lastPID;

std::unique_ptr<SynchronizationTraceTool> SynchronizationTraceTool::instance = nullptr;
mutex SynchronizationTraceTool::singletonMutex;

void SYNCHRONIZATION_CALL_START(Operation op, void* obj) {
    SynchronizationTraceTool::SynchronizationCallStart(static_cast<Operation>(op), obj);
}

void SYNCHRONIZATION_CALL_END() {
    SynchronizationTraceTool::SynchronizationCallEnd();
}

void Filesystem::CreateDirIfNotExists(const string &dirName) {
    boost::filesystem::path dir(dirName);

    if (!boost::filesystem::exists(dir)) {
        // TODO add if here where we log error if dir is not created
        boost::filesystem::create_directory(dir);
    }
}

SynchronizationTraceTool::SynchronizationTraceTool() {
    opLogs = new vector<OperationLog>;
    funcLogs = new vector<FunctionLog>;
    doneWriting = false;

    opLogs->reserve(1000000);
    funcLogs->reserve(1000000);

    fifoIDCounter = 0;
    pipeIDCounter = 0;
    msgIDCounter = 0;

    lastPID = ::getpid();

    Filesystem::CreateDirIfNotExists("latency");
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
    currFuncLog = FunctionLog(FunctionTracer::getInstance()->getCurrentSIID());

    pushToVec(*instance->opLogs, OperationLog(obj, op));
    dataMutex.unlock();

    currFuncLog.start();
}

void SynchronizationTraceTool::SynchronizationCallEnd() {
    currFuncLog.end();

    dataMutex.lock();
    pushToVec(*instance->funcLogs, currFuncLog);
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

template <typename T>
void SynchronizationTraceTool::pushToVec(vector<T> &vec, const T &val) {
    if (haveForkedSinceLastOp()) {
        refreshStateAfterFork();
    }

    vec.push_back(val);
}

bool SynchronizationTraceTool::haveForkedSinceLastOp() {
    pid_t currPID = ::getpid();
    bool retVal = currPID != lastPID;
    lastPID = currPID;

    return retVal;
}

void SynchronizationTraceTool::refreshStateAfterFork() {
    instance->logFile.close();
    Filesystem::CreateDirIfNotExists("latency");
    instance->logFile.open("latency/SynchronizationLog_" + std::to_string(instance->lastPID), std::ios_base::trunc);

    instance->writerThread.detach();
    instance->writerThread = thread(writeLogWorker); 
}

void SynchronizationTraceTool::writeLogWorker() {
    bool stopLogging = false;

    // Loop forever writing logs
    while (!stopLogging) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (instance != nullptr) {
            if (haveForkedSinceLastOp()) {
                refreshStateAfterFork();
            }

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

void ON_MKNOD(const char *path, mode_t mode) {
    bool is_fifo = mode & ~S_IFIFO;
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

size_t ON_WRITE(int fd, const void *buf, size_t nbytes) {
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

int ON_MSGSND(int fd, const void *msgp, size_t msgsz, int msgflg) {
    return SynchronizationTraceTool::GetInstance()->OnMsgSnd(fd, msgp, msgsz, msgflg);
}

ssize_t ON_MSGRCV(int fd, void *msgp, size_t msgsz, long msgtyp, int msgflg) {
    return SynchronizationTraceTool::GetInstance()->OnMsgRcv(fd, msgp, msgsz, msgtyp, msgflg);
}

void SynchronizationTraceTool::AddFIFOName(const char *path_cstr) {
    string path(path_cstr);
    boost::unique_lock<boost::shared_mutex> lock(fifoNamesMutex);
    string ID = "F" + std::to_string(fifoIDCounter++);
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
        unordered_map<int, string>::iterator it = fifoFdsToIDs.find(fd);
        if (it != fifoFdsToIDs.end()) {
            ID = it->second;
            mutexToLock = &fifoAccessLockTable[fd];
        }
    }
    if (mutexToLock == nullptr) {
        boost::shared_lock<boost::shared_mutex> readIDLock(pipeMutex);
        unordered_map<int, string>::iterator it = pipeFdsToIDs.find(fd);
        if (it != pipeFdsToIDs.end()) {
            ID = it->second;
            mutexToLock = &pipeAccessLockTable[ID];
        }
    }
    if (mutexToLock != nullptr) {
        mutexToLock->lock();
        dataMutex.lock();
        pushToVec(*instance->opLogs, OperationLog(ID, MESSAGE_RECEIVE));
        currFuncLog = FunctionLog(FunctionTracer::getInstance()->getCurrentSIID());
        dataMutex.unlock();
        currFuncLog.start();
        result = read(fd, buf, nbytes);
        mutexToLock->unlock();
        currFuncLog.end();
        dataMutex.lock();
        pushToVec(*instance->funcLogs, currFuncLog);
        dataMutex.unlock();
    } else {
        result = read(fd, buf, nbytes);
    }
    return result;
}

size_t SynchronizationTraceTool::OnWrite(int fd, const void *buf, size_t nbytes) {
    size_t result = 0;
    string ID;
    mutex *mutexToLock = nullptr;
    {
        boost::shared_lock<boost::shared_mutex> readIDLock(fifoFdsMutex);
        unordered_map<int, string>::iterator it = fifoFdsToIDs.find(fd);
        if (it != fifoFdsToIDs.end()) {
            ID = it->second;
            mutexToLock = &fifoAccessLockTable[fd];
        }
    }
    if (mutexToLock == nullptr) {
        boost::shared_lock<boost::shared_mutex> readIDLock(pipeMutex);
        unordered_map<int, string>::iterator it = pipeFdsToIDs.find(fd);
        if (it != pipeFdsToIDs.end()) {
            ID = it->second;
            mutexToLock = &pipeAccessLockTable[ID];
        }
    }
    if (mutexToLock != nullptr) {
        mutexToLock->lock();
        dataMutex.lock();
        pushToVec(*instance->opLogs, OperationLog(ID, MESSAGE_SEND));
        dataMutex.unlock();
        currFuncLog = FunctionLog(FunctionTracer::getInstance()->getCurrentSIID());
        currFuncLog.start();
        result = write(fd, buf, nbytes);
        mutexToLock->unlock();
        currFuncLog.end();
        dataMutex.lock();
        pushToVec(*instance->funcLogs, currFuncLog);
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
    string ID = "P" + std::to_string(pipeIDCounter);
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

int SynchronizationTraceTool::OnMsgSnd(int msqid, const void *msgp, size_t msgsz, int msgflg) {
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
    pushToVec(*instance->opLogs, OperationLog(ID, MESSAGE_SEND));
    dataMutex.unlock();

    currFuncLog = FunctionLog(FunctionTracer::getInstance()->getCurrentSIID());
    currFuncLog.start();
    int result = msgsnd(msqid, msgp, msgsz, msgflg);
    mutexToLock->unlock();
    currFuncLog.end();
    
    dataMutex.lock();
    pushToVec(*instance->funcLogs, currFuncLog);
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
    pushToVec(*instance->opLogs, OperationLog(ID, MESSAGE_RECEIVE));
    dataMutex.unlock();

    currFuncLog = FunctionLog(FunctionTracer::getInstance()->getCurrentSIID());
    currFuncLog.start();
    int result = msgrcv(msqid, msgp, msgsz, msgtyp, msgflg);
    mutexToLock->unlock();
    currFuncLog.end();

    dataMutex.lock();
    pushToVec(*instance->funcLogs, currFuncLog);
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


