#include "FunctionPrototype.h"
#include "IPCWrapperGenState.h"

#include <string>
#include <unordered_map>
#include <memory>

typedef std::unordered_map<std::string, WrapperGenState> WrapperGenMap;

class InnerWrapperGenerator {
    public:
        virtual void GenerateWrapperPrologue(std::string &fname, FunctionPrototype &prototype) = 0;
        virtual void GenerateWrapperEpilogue(std::string &fname, FunctionPrototype &prototype) = 0;

    protected:
        std::fstream implementationFile;
}

class TracingInnerWrapperGenerator : public InnerWrapperGenerator {
    public:
        TracingInnerWrapperGenerator(std::shared_ptr<std::unordered_map<std::string, std::string>> _operationMap);

        void GenerateWrapperPrologue(std::string &fname, FunctionPrototype &prototype);
        void GenerateWrapperEpilogue(std::string &fname, FunctionPrototype &prototype);        

    private:
        const std::string prologuePrefix;
        const std::string epiloguePrefix;

        std::shared_ptr<std::unordered_map<std::string, std::string>> operationMap;
}

class IPCInnerWrapperGenerator : public InnerWrapperGenerator {
    protected:
        const IPCWrapperGenMap assignedFunctionState;

        IPCWrapperGenerator(IPCWrapperGenMap _assignedFunctionState): 
            assignedFunctionState(_assignedFunctionState) {}

        std::string BuildFunctionCallFromParams(WrapperGenState &funcToInstrument, 
                                                FunctionPrototype &prototype);
};

class CachingIPCInnerWrapperGenerator : public IPCInnerWrapperGenerator {
    public:
        CachingIPCWrapperGenerator();

        void GenerateWrapperPrologue(std::string &fname, FunctionPrototype &prototype);
        void GenerateWrapperEpilogue(std::string &fname, FunctionPrototype &prototype);        
};

class NonCachingIPCWrapperGenerator : public IPCInnerWrapperGenerator {
    public:
        NonCachingIPCWrapperGenerator();

        void GenerateWrapperPrologue(std::string &fname, FunctionPrototype &prototype);
        void GenerateWrapperEpilogue(std::string &fname, FunctionPrototype &prototype);        
};
