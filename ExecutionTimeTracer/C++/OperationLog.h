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
