#include "synchro_trace_tool.h"

using namespace std;

static SynchronizationTraceTool::instance = nullptr;
__thread FunctionLog TraceTool::currFuncLog;

SynchronizationTraceTool::SynchronizationTool() {
    opLogs = new vector<vector<OperationLog>>;
    funcLogs = new vector<FunctionLog>;

    opLogs->reserve(1000000);
    funcLogs->reserve(1000000);

    opLogFile.open(dir + "OperationLog.log");
    funcLogFile.open(dir + "SynchronizationTimeLog.log");

    thread logWriter(writeLogWorker);
    logWriter.detach();
}

SynchronizationTraceTool::~SynchronizationTraceTool() {
    logMutex.lock();
    
    // opLogs and funcLogs are deleted in here.  Maybe move that
    // functionality to a cleanup function.
    writeLog(instance->opLogs, instance->funcLogs);

    logMutex.unlock();
}

void SynchronizationTraceTool::ChangeThreadSemanticInterval(unsigned int long sid = -1) {
    thread::id thisThreadID = thread::this_thread::get_id();

    threadSemanticIntervals[thisThreadID] = (sid == -1) ? nextSemanticIntervalID++ : sid;
}

void SynchronizationTraceTool::SynchronizationCallStart(unsigned int funcID,
                                                        Operations op,
                                                        T *obj) {
    if (instance == nullptr) {
        maybeCreateInstance();
    }

    thread::id thisThreadID = thread::this_thread::get_id();
    
    if (threadSemanticIntervals.find(thisThreadID) == threadSemanticIntervals.end()) {
        threadSemanticIntrervals[thisThreadID] = nextSemanticIntervalID++;
    }

    logMutex.lock_shared();
    instance->opLogs.push_back(OperationLog(obj, threadSemanticIntervals[thisThreadID]));
    logMutex.unlock_shared();

    currFuncLog = FunctionLog(threadSemanticIntervals[thisThreadID]);

    timespec startTime;
    clock_gettime(CLOCK_REALTIME, startTime);

    currFuncLog.setFunctionStart(startTime);
}

void SynchronizationTraceTool::SynchronizationCallEnd() {
    timespec endTime;
    clock_gettime(CLOCK_REALTIME, endTime);
    currFuncLog.setFunctionEnd(endTime);

    logMutex.lock_shared();
    instance->funcLogs.push_back(currFuncLog);
    logMutex.unlock_shared();
}

void SynchronizationTraceTool::maybeCreateInstance() {
    singletonMutex.lock();

    if (instance == nullptr) {
        instance = new SynchronizationTool();
    }

    singletonMutex.unlock();
}

void SynchronizationTraceTool::Teardown() {
    singletonMutex.lock();
    if (instance != nullptr) {
        delete instance;
    }
    singletonMutex.unlock();
}

void SynchronizationTraceTool::writeLogWorker() {
    // Loop forever writing logs
    while (true) {
        this_thread::sleep_for(5s);
        if (instance != nullptr) {
            vector<OperationLog> newOpLogs = new vector<OperationLog>;
            vector<FunctionLog> newFuncLogs = new vector<FunctionLog>;

            // TODO TODO TODO experiment with this size!!!!
            newOpLogs->reserve(instance->opLogs->size() * 4);
            newFuncLogs->reserve(instance->funcLogs->size() * 4);

            // Lock exclusively
            logMutex.lock();

            vector<vector<OperationLog>> *oldOpLogs = instance->opLogs;
            vector<FunctionLog> *oldFuncLogs = instance->funcLogs;

            instance->opLogs = newOpLogs;
            instance->funcLogs = newFuncLogs;

            logMutex.unlock();

            writeLog(oldOpLogs, oldFuncLogs);
        }
    }
}

void SynchronizationTraceTool::writeLogs(vector<OperationLog> *opLogs, 
                                         vector<FunctionLog> *funcLogs) {
    for (OperationLog &opLog : *opLogs) {
        opLogFile << opLog;
    }

    for (FunctionLog &funcLog : *funcLogs) {
        funcLogFile << funcLog;
    }

    delete opLogs;
    delete funcLogs;
}
