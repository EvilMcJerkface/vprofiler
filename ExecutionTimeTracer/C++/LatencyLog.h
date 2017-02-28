#ifndef LATENCY_LOG_H
#define LATENCY_LOG_H

#include <thread>

class LatencyLog {
    public:
        long latency;
        std::thread::id threadID;
    
        LatencyLog(): latency(0), threadID() {}

        LatencyLog(long _latency, std::thread::id _threadID): 
        latency(_latency), threadID(_threadID) {}
};

#endif
