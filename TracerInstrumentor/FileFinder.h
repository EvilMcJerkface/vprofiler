#include "Utils.h"

// STL headers
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <cstdio>
#include <iostream>
#include <array>

class FileFinder {
    public:
        // Constructs a FileFinder object. Nothing notable here.
        FileFinder(const std::string _sourceBaseDir);

        // Build CScope's database in the sourceBaseDir.
        void BuildCScopeDB();

        // Returns the name of the file in which the given funciton
        // is defined.
        std::string FindFunctionDefinition(const std::string &functionName);

    private:
        //////////////////////////////
        // Private member functions //
        //////////////////////////////

        // Builds a cscope query for the given function name. Of form
        // cscope -L1<functionName>
        std::string buildFunctionDefinitionQuery(const std::string &functionName);

        // Returns the cd command for the instance.
        std::string cdCommand();

        //////////////////////////////
        // Private member variables //
        //////////////////////////////

        const std::string sourceBaseDir;
};
