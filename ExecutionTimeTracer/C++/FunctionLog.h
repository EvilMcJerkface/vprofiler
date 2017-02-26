#include "trace_tool.h"

#include <time.h>
#include <thread>

class FunctionLog {
    public:
        FunctionLog():
        semIntervalID(TraceTool::current_transaction_id) {
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
               << ',' << funcLog.functionStart.tv_nsec << ',' 
               << funcLog.functionEnd.tv_nsec << '\n';

            return os;
        }

    private:
        std::thread::id threadID;
        unsigned int semIntervalID;

        timespec functionStart;
        timespec functionEnd;
};

