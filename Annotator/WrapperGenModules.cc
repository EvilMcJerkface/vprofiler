#include "WrapperGenModules.h"

#define noop (void)0

using namespace std;

TracingInnerWrapperGenerator::TracingInnerWrapperGenerator(shared_ptr<unordered_map<string, string>> _operationMap):
    prologuePrefix("SYNCHRONIZATION_CALL_START("), 
    epiloguePrefix("SYNCHRONIZATION_CALL_END("), 
    operationMap(_operationMap) {}

void TracingInnerWrapperGenerator::GenerateWrapperPrologue(const string &fname,
                                                           const FunctionPrototype &prototype) {
    string object = prototype.isMemberCall ? "obj" : prototype.paramVars[0];

    implementationFile << "SYNCHRONIZATION_CALL_START(" + 
                          (*operationMap)[fname] + 
                          ", static_cast<void*>(" + object + "));\n\t";
}

void TracingInnerWrapperGenerator::GenerateWrapperEpilogue(const string &fname,
                                                           const FunctionPrototype &prototype) {
    implementationFile << "SYNCHRONIZATION_CALL_END();\n";
}

string IPCInnerWrapperGenerator::BuildFunctionCallFromParams(const WrapperGenState &funcToInstrument,
                                                             const FunctionPrototype &prototype) {
    string callFromParameters = "";

    int argumentIndex;
    int argumentsNeeded = funcToInstrument.argumentIndices.size();
    for (int i = 0; i < argumentsNeeded; i++) {
        argumentIndex = funcToInstrument.argumentIndices[i];
        callFromParameters += prototype.paramVars[argumentIndex];

        if (funcToInstrument.usesResult || i != argumentsNeeded - 1) {
            callFromParameters += ", ";
        }
    }

    if (funcToInstrument.usesResult) {
        callFromParameters += "result";
    }

    callFromParameters += ");\n\n";

    return callFromParameters;
}

CachingIPCInnerWrapperGenerator::CachingIPCInnerWrapperGenerator():
    IPCInnerWrapperGenerator({{"mknod", WrapperGenState("on_mknod(", {0, 1}, false)},
                              {"open", WrapperGenState("on_open(", {0}, true)},
                              {"msgget", WrapperGenState("on_msgget(", {}, true)},
                              {"close", WrapperGenState("on_close(", {0}, false)},
                              {"pipe", WrapperGenState("on_pipe(", {0}, false)},
                              {"pipe2", WrapperGenState("on_pipe(", {0}, false)}}) {}

NonCachingIPCInnerWrapperGenerator::NonCachingIPCInnerWrapperGenerator():
    IPCInnerWrapperGenerator({{"msgrcv", WrapperGenState("on_msgrcv(", {0}, false)},
                              {"msgsnd", WrapperGenState("on_msgsnd(", {0}, false)},
                              {"read", WrapperGenState("on_read(", {0}, false)},
                              {"write", WrapperGenState("on_write(", {0}, false)}}) {}

// Do nothing
void CachingIPCInnerWrapperGenerator::GenerateWrapperPrologue(const string &fname, 
                                                              const FunctionPrototype &prototype) {
    noop;
}

void CachingIPCInnerWrapperGenerator::GenerateWrapperEpilogue(const string &fname, 
                                                              const FunctionPrototype &prototype) {
    string cachingCall = assignedFunctionState[fname].internalCallPrefix;

    cachingCall += BuildFunctionCallFromParams(assignedFunctionState[fname], prototype);

    implementationFile << cachingCall;
}

void NonCachingIPCInnerWrapperGenerator::GenerateWrapperPrologue(const string &fname,
                                                                 const FunctionPrototype &prototype) {
    string lockCall = "FIFOLockingAgent::LockFIFO(";

    lockCall += BuildFunctionCallFromParams(assignedFunctionState[fname], prototype);

    implementationFile << lockCall;
}

void NonCachingIPCInnerWrapperGenerator::GenerateWrapperEpilogue(const string &fname,
                                                                 const FunctionPrototype &prototype) {
    string loggingCall = assignedFunctionState[fname].internalCallPrefix;
    string unlockCall = "FIFOLockingAgent::UnlockFIFO(";
    string callSuffix = BuildFunctionCallFromParams(assignedFunctionState[fname], prototype);

    loggingCall += callSuffix;
    unlockCall += callSuffix;

    implementationFile << loggingCall;
    implementationFile << unlockCall;
}
