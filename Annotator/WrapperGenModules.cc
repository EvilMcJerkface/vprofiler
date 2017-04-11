#include "WrapperGenModules.h"

#define noop (void)0

using namespace std;

void InnerWrapperGenerator::GenerateFunctionImplementation(const string &fname,
                                                           const FunctionPrototype &prototype) {
    GenerateWrapperPrologue(fname, prototype);
    GenerateWrapperInterlude(fname, prototype);
    GenerateWrapperEpilogue(fname, prototype);
}

void InnerWrapperGenerator::GenerateWrapperInterlude(const string &fname,
                                                     const FunctionPrototype &prototype) {
    if (prototype.returnType != "void") {
        implementationFile << "result = ";
    }

    implementationFile << prototype.innerCallPrefix + "(";

    for (int i = 0, j = prototype.paramVars.size(); i < j; i++) {
        implementationFile << prototype.paramVars[i];

        if (i != (j - 1)) {
            implementationFile << ", ";
        }
    }

    implementationFile << ");\n\t";
}

TracingInnerWrapperGenerator::TracingInnerWrapperGenerator(std::ofstream &_implementationFile, 
                                                          shared_ptr<unordered_map<string, string>> _operationMap):
    InnerWrapperGenerator(_implementationFile),
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

CachingIPCInnerWrapperGenerator::CachingIPCInnerWrapperGenerator(std::ofstream &_implementationFile):
    IPCInnerWrapperGenerator(_implementationFile,
                             {{"mknod", WrapperGenState("ON_MKNOD(", {0, 1}, false)},
                              {"open", WrapperGenState("ON_OPEN(", {0}, true)},
                              {"msgget", WrapperGenState("ON_MSGGET(", {}, true)},
                              {"close", WrapperGenState("ON_CLOSE(", {0}, false)},
                              {"pipe", WrapperGenState("ON_PIPE(", {0}, false)},
                              {"pipe2", WrapperGenState("ON_PIPE(", {0}, false)}}) {}

NonCachingIPCInnerWrapperGenerator::NonCachingIPCInnerWrapperGenerator(std::ofstream &_implementationFile):
    IPCInnerWrapperGenerator(_implementationFile,
                             {{"msgrcv", WrapperGenState("ON_MSGRCV(", {0}, false)},
                              {"msgsnd", WrapperGenState("ON_MSGSND(", {0}, false)},
                              {"read", WrapperGenState("ON_READ(", {0}, false)},
                              {"write", WrapperGenState("ON_WRITE(", {0}, false)}}) {}

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
    noop;
}

void NonCachingIPCInnerWrapperGenerator::GenerateWrapperInterlude(const string &fname,
                                                                  const FunctionPrototype &prototype) {
    FunctionPrototype dummyPrototype(prototype);
    dummyPrototype.innerCallPrefix = assignedFunctionState[fname].internalCallPrefix;

    InnerWrapperGenerator::GenerateWrapperInterlude(fname, dummyPrototype);
}

void NonCachingIPCInnerWrapperGenerator::GenerateWrapperEpilogue(const string &fname,
                                                                 const FunctionPrototype &prototype) {
    noop;
}
