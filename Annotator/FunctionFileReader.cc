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
        LogInformation newLogInfo;
        std::vector<std::string> separateWords = SplitString((*qualifiedNames)[i], ' ');

        // Fill logInfoMap
        newLogInfo.isMessageBased = separateWords.size() > 1 
                                    && separateWords[1] == "message_based";
        newLogInfo.functionID = i;
        (*logInfoMap)[(*qualifiedNames)[i]] = newLogInfo;

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

std::shared_ptr<std::unordered_map<std::string, LogInformation>> FunctionFileReader::GetLogInfoMap() { 
    if (!beenParsed) {
        throw std::logic_error("FunctionFileReader::GetFunctionMap called before function file was parsed.");
    }

    return logInfoMap;
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
