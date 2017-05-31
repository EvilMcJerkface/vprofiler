#include "FileFinder.h"

std::string FileFinder::cdCommand() {
    return std::string("cd " + sourceBaseDir);
}

// Build a cscope query which will return files which contain functionName
std::string FileFinder::buildQuery(const std::string &functionName) {
    return std::string(cdCommand() + " && cscope -R -L0" + functionName);
}

// Parses cscope output and returns a vector of the unique files 
// represented in the CScope output.
std::vector<std::string> FileFinder::parseCScopeOutput(const std::string &output) {
    std::stringstream ss;
    ss.str(output);

    std::vector<std::string> filenames;
    std::string line;

    while (getline(ss, line)) {
        // The filename is in index 0
        filenames.push_back(sourceBaseDir + SplitString(line, ' ')[0]);
    }

    // Ensure files are unique
    std::sort(filenames.begin(), filenames.end());
    filenames.erase(std::unique(filenames.begin(), filenames.end()), filenames.end());

    return filenames;
}

std::vector<std::string> FileFinder::FindFunctionPotentialFiles(const std::string &functionName) {
    std::string query = buildQuery(functionName);

    std::string output = execute(query); 

    return parseCScopeOutput(output);
}

FileFinder::FileFinder(const std::string _sourceBaseDir): sourceBaseDir(_sourceBaseDir) {}

void FileFinder::BuildCScopeDB() {
    execute(cdCommand() + " && cscope -R -L");
}
