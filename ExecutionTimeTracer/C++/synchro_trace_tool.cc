#include "synchro_trace_tool.h"

using namespace std;

unique_ptr<SynchronizationTraceTool> SynchronizationTraceTool::instance = nullptr;
mutex SynchronizationTraceTool::singletonMutex;
//__thread FunctionLog SynchronizationTraceTool::currFuncLog;

SynchronizationTraceTool::SynchronizationTraceTool() {
    opLogs = new vector<OperationLog>;
    funcLogs = new vector<FunctionLog>;

    opLogs->reserve(1000000);
    funcLogs->reserve(1000000);

    opLogFile.open("latency/OperationLog.log");
    funcLogFile.open("latency/SynchronizationTimeLog.log");

    thread logWriter(writeLogWorker);
    logWriter.detach();
}

SynchronizationTraceTool::~SynchronizationTraceTool() {
    logMutex.lock();
    
    // opLogs and funcLogs are deleted in here.  Maybe move that
    // functionality to a cleanup function.
    writeLogs(instance->opLogs, instance->funcLogs);

    logMutex.unlock();
}

void SynchronizationTraceTool::SynchronizationCallStart(Operation op, void *obj) {
    if (instance == nullptr) {
        maybeCreateInstance();
    }

    thread::id thisThreadID = this_thread::get_id();
    
    /*if (threadSemanticIntervals.find(thisThreadID) == threadSemanticIntervals.end()) {
        threadSemanticIntrervals[thisThreadID] = nextSemanticIntervalID++;
    }*/

    instance->logMutex.lock_shared();
    instance->opLogs->push_back(OperationLog(obj, op));
    instance->logMutex.unlock_shared();

    currFuncLog = FunctionLog();

    timespec startTime;
    clock_gettime(CLOCK_REALTIME, &startTime);

    currFuncLog.setFunctionStart(startTime);
}

void SynchronizationTraceTool::SynchronizationCallEnd() {
    timespec endTime;
    clock_gettime(CLOCK_REALTIME, &endTime);
    currFuncLog.setFunctionEnd(endTime);

    instance->logMutex.lock_shared();
    instance->funcLogs->push_back(currFuncLog);
    instance->logMutex.unlock_shared();
}

void SynchronizationTraceTool::maybeCreateInstance() {
    singletonMutex.lock();

    if (instance == nullptr) {
        instance = unique_ptr<SynchronizationTraceTool>(new SynchronizationTraceTool());
    }

    singletonMutex.unlock();
}

void SynchronizationTraceTool::writeLogWorker() {
    // Loop forever writing logs
    while (true) {
        this_thread::sleep_for(5s);
        if (instance != nullptr) {
            vector<OperationLog> *newOpLogs = new vector<OperationLog>;
            vector<FunctionLog> *newFuncLogs = new vector<FunctionLog>;

            // TODO TODO TODO experiment with this size!!!!
            newOpLogs->reserve(instance->opLogs->size() * 4);
            newFuncLogs->reserve(instance->funcLogs->size() * 4);

            // Lock exclusively
            instance->logMutex.lock();

            vector<OperationLog> *oldOpLogs = instance->opLogs;
            vector<FunctionLog> *oldFuncLogs = instance->funcLogs;

            instance->opLogs = newOpLogs;
            instance->funcLogs = newFuncLogs;

            instance->logMutex.unlock();

            writeLogs(oldOpLogs, oldFuncLogs);
        }
    }
}

void SynchronizationTraceTool::writeLogs(vector<OperationLog> *opLogs, 
                                         vector<FunctionLog> *funcLogs) {
    for (OperationLog &opLog : *opLogs) {
        instance->opLogFile << opLog;
    }

    for (FunctionLog &funcLog : *funcLogs) {
        instance->funcLogFile << funcLog;
    }

    delete opLogs;
    delete funcLogs;
}
