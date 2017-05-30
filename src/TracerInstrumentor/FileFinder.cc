#include "FileFinder.h"

#include <iostream>

std::string FileFinder::cdCommand() {
    return std::string("cd " + sourceBaseDir);
}

// Build a cscope query which will return the file with the function definition
std::string FileFinder::buildFunctionDefinitionQuery(const std::string &functionName) {
    return std::string(cdCommand() + " && cscope -R -L1" + functionName);
}

std::string FileFinder::FindFunctionDefinition(const std::string &functionName) {
    std::string query = buildFunctionDefinitionQuery(functionName);

    std::string output = execute(query);

    if (output.size() == 0) {
        return "";
    }
    
    return SplitString(output, ' ')[0];
}

FileFinder::FileFinder(const std::string _sourceBaseDir): sourceBaseDir(_sourceBaseDir) {}

void FileFinder::BuildCScopeDB() {
    execute(cdCommand() + " && cscope -R -L");
}
