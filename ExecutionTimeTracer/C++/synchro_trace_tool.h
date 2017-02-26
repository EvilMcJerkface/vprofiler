#ifndef SYNCHRO_TRACE_TOOL_H
#define SYNCHRO_TRACE_TOOL_H

#include "trace_tool.h"
#include "OperationLog.h"
#include "FunctionLog.h"

#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <memory>

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
        std::shared_timed_mutex logMutex;

        static void maybeCreateInstance();

        SynchronizationTraceTool();

        static void writeLogWorker();
        static void writeLogs(std::vector<OperationLog> *opLogs,
                             std::vector<FunctionLog> *funcLogs);
};

#endif
