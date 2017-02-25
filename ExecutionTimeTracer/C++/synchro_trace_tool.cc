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

void SynchronizationTraceTool::SynchronizationCallStart(unsigned int funcID,
                                                        Operations op,
                                                        T *obj) {
    if (instance == nullptr) {
        maybeCreateInstance();
    }

    thread::id thisThreadID = thread::this_thread::get_id();
    
    if (threadSemanticIntervals.find(thisThreadID) == threadSemanticIntervals.end()) {
        threadSemanticIntrervals[thisThreadID] = nextSemanticIntervalID;
        nextSemanticIntervalID++;
    }

    instance->opLogs.push_back(OperationLog(obj, threadSemanticIntervals[thisThreadID]));

    currFuncLog = FunctionLog(threadSemanticIntervals[thisThreadID]);

    timespec startTime;
    clock_gettime(CLOCK_REALTIME, startTime);

    currFuncLog.setFunctionStart(startTime);
}

void SynchronizationTraceTool::SynchronizationCallEnd() {
    timespec endTime;
    clock_gettime(CLOCK_REALTIME, endTime);
    currFuncLog.setFunctionEnd(endTime);

    instance->funcLogs.push_back(currFuncLog);
}

void SynchronizationTraceTool::maybeCreateInstance() {
    singletonMutex.lock();

    if (instance == nullptr) {
        instance = new SynchronizationTool();
    }

    singletonMutex.unlock();
}

void SynchronizationTraceTool::writeLogWorker() {
    // Loop forever writing logs
    while (true) {
        this_thread::sleep_for(5s);
        if (instance != nullptr) {
            vector<vector<OperationLog>> newOpLogs = new vector<OperationLog>;
            vector<FunctionLog> newFuncLogs = new vector<FunctionLog>

            vector<vector<OperationLog>> *oldOpLogs = instance->opLogs;
            vector<FunctionLog> *oldFuncLogs = instance->funcLogs;
        }
    }
}
