// VProf headers
#include "FileFinder.h"
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

// Build a cscope query which will return files which contain functionName
std::string buildQuery(const std::string &functionName) {
    return std::string("cscope -L3" + functionName);
}

// Parses cscope output and returns a vector of the unique files 
// represented in the CScope output.
std::vector<std::string> parseCScopeOutput(const std::string &output) {
    std::stringstream ss;
    ss.str(output);

    std::vector<std::string> filenames;
    std::string line;

    while (getline(ss, line)) {
        // The filename is in index 0
        filenames.push_back(SplitString(line, ' ')[0]);
    }

    // Ensure files are unique
    std::sort(filenames.begin(), filenames.end());
    filenames.erase(std::unique(filenames.begin(), filenames.end()), filenames.end());

    return filenames;
}

// Credit to stackoverflow user waqas. Code can be found at 
// https://www.stackoverflow.com/questions/478898/how-to-execute-a-command-and-get-output-of-command-within-c-using-posix
std::string exec(const std::string &command) {
    std::array<char, 128> buffer;
    std::string result;

    std::shared_ptr<FILE> pipe(popen(command, 'r'), pclose);

    if (!pipe) {
        throw std::runtime_error("popen(" + command + ", 'r') failed.");
    }

    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr) {
                result += buffer.data();
        }
    }

    return result;
}

std::vector<std::string> FileFinder::FindFunctionPotentialFiles(const std::string &functionName) {
    std::string query = buildQuery(functionName);

    std::string output = exec(query); 

    return parseCScopeOutput(output);
}

std::vector<std::string> FileFinder::FindFunctionsPotentialFiles(const std::vector<std::string> &functions) {
    std::vector<std::string> result;

    for (std::string &function : functions) {
        std::vector<std::string> functionResult = FindFunctionPotentialFiles(function);

        result.insert(result.end(), functionResult.begin(), functionResult.end());
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
}
