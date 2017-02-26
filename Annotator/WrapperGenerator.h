// VProf libs
#include "FunctionPrototype.h"
#include "FunctionFileReader.h"

// STL libs
#include <unordered_map>
#include <string>
#include <memory>
#include <fstream>
#include <algorithm>
#include <vector>

class WrapperGenerator {
    private:
        // Map from fully qualified names to instrument to a FunctionPrototype object
        // containing all information needed to form the wrapper.
        std::shared_ptr<std::unordered_map<std::string, FunctionPrototype>> prototypeMap;

        std::shared_ptr<std::unordered_map<std::string, std::string>> operationMap;

        // File handle for header file.
        std::ofstream headerFile;

        // File handle for implementation file.
        std::ofstream implementationFile;

        std::vector<std::string> getFilenames();

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
