#ifndef SYNCHRO_TRACE_TOOL_H
#define SYNCHRO_TRACE_TOOL_H

#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

class SynchronizationTraceTool {
    public:
        template <typename T>
        static void SynchronizationCallStart(unsigned int funcID, Operations op, T *obj);

        static void SynchronizationCallEnd();

        static void Teardown();

    private:
        static SynchronizationTraceTool *instance;
        static std::mutex singletonMutex;

        static __thread FunctionLog currFuncLog;

        std::ofstream funcLogFile;
        std::ofstream opLogFile;

        std::vector<std::vector<OperationLog>> *opLogs;
        std::vector<FunctionLog> *funcLogs;
        std::shared_mutex logMutex;

        std::unordered_map<std::thread::id, unsigned long int> threadSemanticIntervals;

        static void maybeCreateInstance();

        SynchronizationTool();
        ~SynchronizationTool();

        static void writeLogWorker();
        static void writeLog(std::vector<OperationLog> opLogs,
                             std::vector<FunctionLog> funcLogs);
}

#endif
