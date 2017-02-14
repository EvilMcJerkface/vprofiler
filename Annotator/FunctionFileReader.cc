#include "FunctionFileReader.h"
#include "Utils.h"

#include <fstream>
#include <string>
#include <stdexcept>

FunctionFileReader::Parse() {
    std::ifstream infile(filename);
    std::string line; 

    // Should eventually trim the line to help user not input bad data
    while (std::getline(infile, line)) {
        qualifiedNames.push_back(line);
    }

    std::vector<std::string> nameComponents;
    std::string unqualifiedName;
    for (std::string &func : qualifiedNames) {
        nameComponents = SplitString(func, ':');
        unqualifiedName = nameComponents[nameComponents.size() - 1];

        unqualifiedNames.push_back(unqualifiedName);
        funcMap[unqualifiedName] = unqualifiedName + "_vprofiler";
    }

    beenParsed = true;
}

std::unordered_map<std::string, std::string> FunctionFileReader::GetFunctionMap() { 
    if (!beenParsed) {
        throw std::logic_error("FunctionFileReader::GetFunctionMap called before function file was parsed.");
    }

    return funcMap;
}

std::vector<std::string> FunctionFileReader::GetQualifiedFunctionNames() {
    if (!beenParsed) {
        throw std::logic_error("FunctionFileReader::GetQualifiedFunctionNames called before function file was parsed.");
    }

    return qualifiedNames;
}

std::vector<std::string> FunctionFileReader::GetUnqualifiedFunctionNames() {
    if (!beenParsed) {
        throw std::logic_error("FunctionFileReader::GetUnqualifiedFunctionNames called before function file was parsed.");
    }

    return unqualifiedNames;
}
