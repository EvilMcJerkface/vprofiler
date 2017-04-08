#ifndef WRAPPER_GEN_MODULES_H
#define WRAPPER_GEN_MODULES_H

#include "FunctionPrototype.h"
#include "WrapperGenState.h"

#include <string>
#include <unordered_map>
#include <memory>
#include <fstream>

typedef std::unordered_map<std::string, WrapperGenState> WrapperGenStateMap;

class InnerWrapperGenerator {
    public:
        virtual void GenerateWrapperPrologue(const std::string &fname, 
                                             const FunctionPrototype &prototype) = 0;
        virtual void GenerateWrapperEpilogue(const std::string &fname, 
                                             const FunctionPrototype &prototype) = 0;

    protected:
        std::ofstream implementationFile;
};

class TracingInnerWrapperGenerator : public InnerWrapperGenerator {
    public:
        TracingInnerWrapperGenerator(std::shared_ptr<std::unordered_map<std::string, std::string>> _operationMap);

        void GenerateWrapperPrologue(const std::string &fname, 
                                     const FunctionPrototype &prototype);
        void GenerateWrapperEpilogue(const std::string &fname, 
                                     const FunctionPrototype &prototype);        

    private:
        const std::string prologuePrefix;
        const std::string epiloguePrefix;

        std::shared_ptr<std::unordered_map<std::string, std::string>> operationMap;
};

class IPCInnerWrapperGenerator : public InnerWrapperGenerator {
    protected:
        WrapperGenStateMap assignedFunctionState;

        IPCInnerWrapperGenerator(WrapperGenStateMap _assignedFunctionState): 
            assignedFunctionState(_assignedFunctionState) {}

        std::string BuildFunctionCallFromParams(const WrapperGenState &funcToInstrument, 
                                                const FunctionPrototype &prototype);
};

class CachingIPCInnerWrapperGenerator : public IPCInnerWrapperGenerator {
    public:
        CachingIPCInnerWrapperGenerator();

        void GenerateWrapperPrologue(const std::string &fname, 
                                     const FunctionPrototype &prototype);
        void GenerateWrapperEpilogue(const std::string &fname, 
                                     const FunctionPrototype &prototype);        
};

class NonCachingIPCInnerWrapperGenerator : public IPCInnerWrapperGenerator {
    public:
        NonCachingIPCInnerWrapperGenerator();

        void GenerateWrapperPrologue(const std::string &fname,
                                     const FunctionPrototype &prototype);
        void GenerateWrapperEpilogue(const std::string &fname, 
                                     const FunctionPrototype &prototype);        
};

#endif
