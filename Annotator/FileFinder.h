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

        // Returns a vector of the potential files in which calls to 
        // functionName may reside.
        std::vector<std::string> FindFunctionPotentialFiles(const std::string &functionName);

        // Convenience function for finding all potential files of functions the 
        // user wishes to profile.
        std::vector<std::string> FindFunctionsPotentialFiles(const std::shared_ptr<std::vector<std::string>> functions);

    private:
        //////////////////////////////
        // Private member functions //
        //////////////////////////////

        // Builds a cscope query for the given function name. Of form
        // cscope -L3<functionName>
        std::string buildQuery(const std::string &functionName);

        // Returns the cd command for the instance.
        std::string cdCommand();

        // Parses cscope output and returns a vector of the unique files 
        // represented in the CScope output.
        std::vector<std::string> parseCScopeOutput(const std::string &output);

        //////////////////////////////
        // Private member variables //
        //////////////////////////////

        const std::string sourceBaseDir;
};
