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
        InnerWrapperGenerator(std::ofstream &_implementationFile):
            implementationFile(_implementationFile) {}

        void GenerateFunctionImplementation(const std::string &fname,
                                            const FunctionPrototype &prototype);

    protected:
        virtual void GenerateWrapperPrologue(const std::string &fname, 
                                             const FunctionPrototype &prototype) = 0;

        // I don't know what to call what's between prologue and epilogue. 
        // Writer blogs said interlude so that's what we're going with. \_(-_-)_/
        virtual void GenerateWrapperInterlude(const std::string &fname,
                                              const FunctionPrototype &prototype);

        virtual void GenerateWrapperEpilogue(const std::string &fname, 
                                             const FunctionPrototype &prototype) = 0;

        std::ofstream &implementationFile;
};

class TracingInnerWrapperGenerator : public InnerWrapperGenerator {
    public:
        TracingInnerWrapperGenerator(std::ofstream &_implementationFile, 
                                     std::shared_ptr<std::unordered_map<std::string, std::string>> _operationMap);

    protected:
        virtual void GenerateWrapperPrologue(const std::string &fname, 
                                             const FunctionPrototype &prototype);
        virtual void GenerateWrapperEpilogue(const std::string &fname, 
                                             const FunctionPrototype &prototype);        

    private:
        const std::string prologuePrefix;
        const std::string epiloguePrefix;

        std::shared_ptr<std::unordered_map<std::string, std::string>> operationMap;
};

class IPCInnerWrapperGenerator : public InnerWrapperGenerator {
    protected:
        WrapperGenStateMap assignedFunctionState;

        IPCInnerWrapperGenerator(std::ofstream &_implementationFile, 
                                 WrapperGenStateMap _assignedFunctionState): 
            InnerWrapperGenerator(_implementationFile),
            assignedFunctionState(_assignedFunctionState) {}

        std::string BuildFunctionCallFromParams(const WrapperGenState &funcToInstrument, 
                                                const FunctionPrototype &prototype);
};

class CachingIPCInnerWrapperGenerator : public IPCInnerWrapperGenerator {
    public:
        CachingIPCInnerWrapperGenerator(std::ofstream &_implementationFile);

    protected:
        virtual void GenerateWrapperPrologue(const std::string &fname, 
                                             const FunctionPrototype &prototype);
        virtual void GenerateWrapperEpilogue(const std::string &fname, 
                                             const FunctionPrototype &prototype);        
};

class NonCachingIPCInnerWrapperGenerator : public IPCInnerWrapperGenerator {
    public:
        NonCachingIPCInnerWrapperGenerator(std::ofstream &_implementationFile);

    protected:
        virtual void GenerateWrapperPrologue(const std::string &fname,
                                             const FunctionPrototype &prototype);
        virtual void GenerateWrapperInterlude(const std::string &fname,
                                              const FunctionPrototype &prototype);
        virtual void GenerateWrapperEpilogue(const std::string &fname, 
                                             const FunctionPrototype &prototype);        
};

#endif
