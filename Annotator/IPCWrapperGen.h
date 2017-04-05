#include "FunctionPrototype.h"
#include "IPCWrapperGenState.h"

#include <string>
#include <unordered_map>

typedef std::unordered_map<std::string, IPCWrapperGenState> IPCWrapperGenMap;

class IPCWrapperGenerator {
    public:
        virtual void GenerateWrapperPrologue(std::string &fname, FunctionPrototype &prototype) = 0;
        virtual void GenerateWrapperEpilogue(std::string &fname, FunctionPrototype &prototype) = 0;

    protected:
        const IPCWrapperGenMap assignedFunctionState;
        std::fstream implementationFile;

        IPCWrapperGenerator(IPCWrapperGenMap _assignedFunctionState): 
            assignedFunctionState(_assignedFunctionState) {}

        std::string BuildFunctionCallFromParams(std::string &fname, 
                                                FunctionPrototype &prototype);
};

class CachingIPCWrapperGenerator : public IPCWrapperGenerator {
    public:
        CachingIPCWrapperGenerator();

        void GenerateWrapperPrologue(std::string &fname, FunctionPrototype &prototype);
        void GenerateWrapperEpilogue(std::string &fname, FunctionPrototype &prototype);        
};

class NonCachingIPCWrapperGenerator : public IPCWrapperGenerator {
    public:
        NonCachingIPCWrapperGenerator();

        void GenerateWrapperPrologue(std::string &fname, FunctionPrototype &prototype);
        void GenerateWrapperEpilogue(std::string &fname, FunctionPrototype &prototype);        
};
