#ifndef OPERATION_LOG_H
#define OPERATION_LOG_H

#include "trace_tool.h"

#include <thread>
#include <iostream>

enum Operation  { MUTEX_LOCK,
                  MUTEX_UNLOCK,
                  CV_WAIT,
                  CV_BROADCAST,
                  CV_SIGNAL,
                  QUEUE_ENQUEUE,
                  QUEUE_DEQUEUE,
                  MESSAGE_SEND,
                  MESSAGE_RECEIVE };

class OperationLog {
    public:
        OperationLog(const void* _obj, Operation _op):
        semIntervalID(TraceTool::current_transaction_id), obj(_obj), op(_op) {
            threadID = std::this_thread::get_id();
        }

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

#endif
