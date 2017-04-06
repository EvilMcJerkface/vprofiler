#include "IPCWrapperGen.h"

#define noop (void)0

TracingInnerWrapperGenerator::TracingInnerWrapperGenerator(std::shared_ptr<std::unordered_map<std::string, std::string>> operationMap):
    prologuePrefix("SYNCHRONIZATION_CALL_START("), 
    epiloguePrefix("SYNCHRONIZATION_CALL_END("), 
    operationMap(_operationMap) {}

void TracingInnerWrapperGenerator::GenerateWrapperPrologue(std::string &fname,
                                                           FunctionPrototype &prototype) {
    string object = prototype.isMemberCall ? "obj" : prototype.paramVars[0];

    implementationFile << "SYNCHRONIZATION_CALL_START(" + 
                          (*operationMap)[fname] + 
                          ", static_cast<void*>(" + object + "));\n\t";
}

void TracingInnerWrapperGenerator::GenerateWrapperPrologue(std::string &fname,
                                                           FunctionPrototype &prototype) {
    implementationFile << "SYNCHRONIZATION_CALL_END();\n";
}

std::string IPCInnerWrapperGenerator::BuildFunctionCallFromParams(WrapperGenState &funcToInstrument,
                                                                  FunctionPrototype &prototype) {
    std::string callFromParameters = "";

    int argumentIndex;
    int argumentsNeeded = funcToInstrument.usedIndices.size();
    for (int i = 0; i < argumentsNeeded; i++) {
        argumentIndex = funcToInstrument.usedIndices[i];
        callFromParameters += prototype.arguments[argumentIndex];

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

CachingIPCWrapperGenerator::CachingIPCWrapperGenerator():
    IPCWrapperGenerator({{"mknod", IPCWrapperGenState("on_mknod(", {0, 1}, false)},
                         {"open", IPCWrapperGenState("on_open(", {0}, true)},
                         {"msgget", IPCWrapperGenState("on_msgget(", {}, true)},
                         {"close", IPCWrapperGenState("on_close(", {0}, false)},
                         {"pipe", IPCWrapperGenState("on_pipe(", {0}, false)},
                         {"pipe2", IPCWrapperGenState("on_pipe(", {0}, false)}}) {}

NonCachingIPCWrapperGenerator::NonCachingIPCWrapperGenerator():
    IPCWrapperGenerator({{"msgrcv", IPCWrapperGenState("on_msgrcv(", {0}, false)},
                         {"msgsnd", IPCWrapperGenState("on_msgsnd(", {0}, false)},
                         {"read", IPCWrapperGenState("on_read(", {0}, false)},
                         {"write", IPCWrapperGenState("on_write(", {0}, false)}}) {}

// Do nothing
void CachingIPCWrapperGenerator::GenerateWrapperPrologue(std::string &fname, 
                                                         FunctionPrototype &prototype) {
    noop;
}

void CachingIPCWrapperGenerator::GenerateWrapperEpilogue(std::string &fname, 
                                                         FunctionPrototype &prototype) {
    WrapperGenState 
    std::string cachingCall = assignedFunctionState[fname].internalCallPrefix;

    cachingCall += BuildFunctionCallFromParams(fname, prototype);

    implementationFile << cachingCall;
}

void NonCachingIPCWrapperGenerator::GenerateWrapperPrologue(std::string &fname, FunctionPrototype &prototype) {
    std::string lockCall = "FIFOLockingAgent::LockFIFO(";

    lockCall += BuildFunctionCallFromParams(fname, prototype);

    implementationFile << lockCall;
}

void NonCachingIPCWrapperGenerator::GenerateWrapperEpilogue(std::string &fname, FunctionPrototype &prototype) {
    std::string loggingCall = assignedFunctionState[fname].internalCallPrefix;
    std::string unlockCall = "FIFOLockingAgent::UnlockFIFO(";
    std::string callSuffix = BuildFunctionCallFromParams(fname, prototype);

    loggingCall += callSuffix;
    unlockCall += callSuffix;

    implementationFile << loggingCall;
    implementationFile << unlockCall;
}
