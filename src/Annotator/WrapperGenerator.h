// VProf libs
#include "FunctionPrototype.h"
#include "FunctionFileReader.h"
#include "WrapperGenModules.h"

// STL libs
#include <unordered_map>
#include <string>
#include <memory>
#include <fstream>
#include <algorithm>
#include <vector>
#include <functional>

typedef std::unordered_map<std::string, std::shared_ptr<InnerWrapperGenerator>> WrapperGenMap;

class WrapperGenerator {
    private:
        // Map from fully qualified names to instrument to a FunctionPrototype object
        // containing all information needed to form the wrapper.
        std::shared_ptr<std::unordered_map<std::string, FunctionPrototype>> prototypeMap;

        std::shared_ptr<std::unordered_map<std::string, std::string>> operationMap;

        // Map takes an operation to the wrapper generator that should be used to generate
        // the wrapped function's wrapper.
        WrapperGenMap operationToGenerator;

        // File handle for header file.
        std::ofstream headerFile;

        // File handle for implementation file.
        std::ofstream implementationFile;

        bool isIPCOperation(const std::string &op) const;

        void instrumentIPCFunction(std::string &functionName, FunctionPrototype &prototype);

        std::vector<std::string> getFilenames();

        // For initialization only
        void initOpToGenMap();

    public:
        WrapperGenerator(std::shared_ptr<std::unordered_map<std::string, 
                         FunctionPrototype>> _prototypeMap,
                         std::shared_ptr<std::unordered_map<std::string, 
                         std::string>> _operationMap,
                         std::string pathPrefix="");

        void GenerateHeader();

        void GenerateImplementations();

        void GenerateWrappers();
};
