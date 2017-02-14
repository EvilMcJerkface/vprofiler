#include <vector>
#include <string>

namespace FileFinder {
    // Returns a vector of the potential files in which calls to 
    // functionName may reside.
    std::vector<std::string> FindFunctionPotentialFiles(const std::string &functionName);

    // Convenience function for finding all potential files of functions the 
    // user wishes to profile.
    std::vector<std::string> FindFunctionsPotentialFiles(const std::vector<std::string> &functions);
};
