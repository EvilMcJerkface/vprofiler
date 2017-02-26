#include "FunctionFileReader.h"

void FunctionFileReader::Parse() {
    std::ifstream infile(filename.c_str());
    std::string line; 

    // Should eventually trim the line to help user not input bad data
    while (std::getline(infile, line)) {
        qualifiedNames->push_back(line);
        std::replace(line.begin(), line.end(), ':', '_');
        std::replace(line.begin(), line.end(), '<', '_');
        std::replace(line.begin(), line.end(), '>', '_');
        (*funcMap)[(*qualifiedNames)[qualifiedNames->size() - 1]] = line + "_vprofiler";
    }

    std::vector<std::string> nameComponents;
    std::string unqualifiedName;
    for (int i = 0; i < qualifiedNames->size(); i++) {
        std::vector<std::string> separateWords = SplitString((*qualifiedNames)[i], ' ');

        if (separateWords.size() != 2) {
            throw runtime_error("Did not find pattern <fully qualified function name>" 
                                " <function type> for function " + separateWords[0]);
        }

        transform(separateWords[1].begin(), separateWords[1].end(),
                  separateWords[1].begin(), ::toupper);

        if (operationStrings.find(separateWords[1]) == operationStrings.end()) {
            throw runtime_error("Function type " + separateWords[1] + " not "
                                "for function " + separateWords[0]);
        }

        (*opMap)[(*qualifiedNames)[i]] = separateWords[1];

        // Fill unqualified names
        nameComponents = SplitString(separateWords[0], ':');
        unqualifiedName = nameComponents[nameComponents.size() - 1];

        unqualifiedNames->push_back(unqualifiedName);
    }

    beenParsed = true;
}

std::shared_ptr<std::unordered_map<std::string, std::string>> FunctionFileReader::GetFunctionMap() { 
    if (!beenParsed) {
        throw std::logic_error("FunctionFileReader::GetFunctionMap called before function file was parsed.");
    }

    return funcMap;
}

std::shared_ptr<std::unordered_map<std::string, std::string>> FunctionFileReader::GetOperationMap() { 
    if (!beenParsed) {
        throw std::logic_error("FunctionFileReader::GetFunctionMap called before function file was parsed.");
    }

    return opMap;
}

std::shared_ptr<std::vector<std::string>> FunctionFileReader::GetQualifiedFunctionNames() {
    if (!beenParsed) {
        throw std::logic_error("FunctionFileReader::GetQualifiedFunctionNames called before function file was parsed.");
    }

    return qualifiedNames;
}

std::shared_ptr<std::vector<std::string>> FunctionFileReader::GetUnqualifiedFunctionNames() {
    if (!beenParsed) {
        throw std::logic_error("FunctionFileReader::GetUnqualifiedFunctionNames called before function file was parsed.");
    }

    return unqualifiedNames;
}
