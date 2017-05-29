#ifndef FUNCTION_LOG_H
#define FUNCTION_LOG_H

#include "trace_tool.h"

#include <time.h>
#include <thread>

// NOTE we're keeping one of these for each synchronization and traced
// function instance.  In particular, for non-synchronization calls
// storing semIntervalID is redundant.  If we have problems with
// too much overhead, change this.
class FunctionLog {
    public:
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

#endif
