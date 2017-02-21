// VProf libs
#include "FunctionPrototype.h"

// STL libs
#include <unordered_map>
#include <string>
#include <memory>
#include <fstream>

class WrapperGenerator {
    private:
        // Map from fully qualified names to instrument to a FunctionPrototype object
        // containing all information needed to form the wrapper.
        std::shared_ptr<std::unordered_map<std::string, FunctionPrototype>> prototypeMap;

        // File handle for header file.
        std::ofstream headerFile;

        // File handle for implementation file.
        std::ofstream implementationFile;

    public:
        WrapperGenerator(std::shared_ptr<std::unordered_map<std::string, 
                         FunctionPrototype>> _prototypeMap,
                         std::string pathPrefix="");

        void GenerateHeader();

        void GenerateImplementations();

        void GenerateWrappers();
};
