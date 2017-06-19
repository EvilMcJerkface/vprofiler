#include "NonTargetTracerInstrumentorFrontendActionFactory.h"
#include "CallerInstrumentorFrontendActionFactory.h"
#include "TracerInstrumentorFrontendActionFactory.h"
#include "ReturnInstrumentorFrontendActionFactory.h"
#include "FileFinder.h"

// Clang libs
#include "llvm/Support/CommandLine.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"

// STL libs
#include <string>
#include <iostream>

using namespace llvm;
using namespace clang::tooling;

cl::OptionCategory TracerInstrumentorOptions("TracerInstrumentor options");
cl::opt<std::string> FunctionNameAndArgs("f",
                             cl::desc("Specify name of the function to instrument and its parameters."),
                             cl::value_desc("Fuction_Name_And_Args"),
                             cl::Optional,
                             cl::ValueRequired);

cl::opt<std::string> CallerNameAndArgs("c",
                             cl::desc("Specify name of the caller and its parameters."),
                             cl::value_desc("Caller_Name_And_Args"),
                             cl::Optional,
                             cl::ValueRequired);

cl::opt<std::string> RootNamesAndArgs("r",
                             cl::desc("Specify name of all threads' root functions to instrument and its parameters."),
                             cl::value_desc("Root_Names_And_Args"),
                             cl::Optional,
                             cl::ValueRequired);

cl::opt<int> TargetPathCount("t",
                             cl::desc("Specify name of the caller and its parameters."),
                             cl::value_desc("Target_Path_Count"),
                             cl::Optional,
                             cl::ValueRequired);

cl::opt<std::string> SourceBaseDir("s",
                              cl::desc("Specifies the root directory of the "
                                       "source tree."),
                              cl::value_desc("Source_Base_Dir"),
                              cl::Required,
                              cl::ValueRequired);

cl::opt<std::string> CallerBackupDir("e",
                              cl::desc("Specifies the dir for back up files "
                                       "before caller function instrumentation."),
                              cl::value_desc("Backup_Dir"),
                              cl::Optional,
                              cl::ValueRequired);

cl::opt<std::string> TargetBackupDir("b",
                              cl::desc("Specifies the dir for back up files "
                                       "before target function instrumentation."),
                              cl::value_desc("Backup_Dir"),
                              cl::Required,
                              cl::ValueRequired);

cl::opt<std::string> FunctionNamesFile("n",
                              cl::desc("Specifies the path of the function names file."),
                              cl::value_desc("Function_Names_File"),
                              cl::Required,
                              cl::ValueRequired);
                              

std::string getUnqualifiedFunctionName(std::string &functionNameAndArgs) {
    std::string qualifiedName = SplitString(functionNameAndArgs, '|')[0];
    std::string nameAndIndex = SplitString(qualifiedName, ':').back();
    return SplitString(nameAndIndex, '-')[0];
}

std::vector<std::string> findAllPotentialFiles(std::string &rootFunctionNamesAndArgs,
                                               FileFinder &fileFinder) {
    std::vector<std::string> fileNames;
    std::vector<std::string> rootFunctions = SplitString(rootFunctionNamesAndArgs, ',');
    for (size_t i = 0; i < rootFunctions.size(); ++i) {
        std::vector<std::string> potentialFiles = fileFinder.FindFunctionPotentialFiles(
            getUnqualifiedFunctionName(rootFunctions[i]));
        fileNames.insert(fileNames.end(), potentialFiles.begin(), potentialFiles.end());
    }
    return fileNames;
}

int main(int argc, const char **argv) {
    CommonOptionsParser OptionsParser(argc, argv, TracerInstrumentorOptions);

    FileFinder fileFinder(SourceBaseDir);
    fileFinder.BuildCScopeDB();

    if (CallerNameAndArgs.size() > 0) {
        std::string callerFunctionName = getUnqualifiedFunctionName(CallerNameAndArgs);
        std::vector<std::string> potentialCallerFiles = fileFinder.FindFunctionPotentialFiles(callerFunctionName);

        if (potentialCallerFiles.size() == 0) {
            std::cout << "Function " << callerFunctionName << " not found" << std::endl;
            return 0;
        }
        ClangTool CallerInstrumentator(OptionsParser.getCompilations(), potentialCallerFiles);
        CallerInstrumentator.run(CreateCallerInstrumentorFrontendActionFactory(
            FunctionNameAndArgs, CallerNameAndArgs, TargetPathCount, CallerBackupDir).get());
    }

    if (FunctionNameAndArgs.length() == 0) {
        std::vector<std::string> allPotentialFiles = findAllPotentialFiles(RootNamesAndArgs, fileFinder);
        ClangTool NonTargetTracerInstrumentator(OptionsParser.getCompilations(), allPotentialFiles);
        NonTargetTracerInstrumentator.run(CreateNonTargetTracerInstrumentorFrontendActionFactory(
            RootNamesAndArgs, TargetBackupDir, FunctionNamesFile).get());
    } else {
        std::string targetFunctionName = getUnqualifiedFunctionName(FunctionNameAndArgs);
        std::vector<std::string> potentialTargetFiles = fileFinder.FindFunctionPotentialFiles(targetFunctionName);

        if (potentialTargetFiles.size() == 0) {
            std::cout << "Function " << targetFunctionName << " not found" << std::endl;
            return 0;
        }
        ClangTool TracerInstrumentator(OptionsParser.getCompilations(), potentialTargetFiles);
        TracerInstrumentator.run(CreateTracerInstrumentorFrontendActionFactory(
            FunctionNameAndArgs, TargetPathCount, TargetBackupDir, FunctionNamesFile).get());

        ClangTool ReturnInstrumentator(OptionsParser.getCompilations(), potentialTargetFiles);
        ReturnInstrumentator.run(CreateReturnInstrumentorFrontendActionFactory(FunctionNameAndArgs).get());
    }

    return 0;
}
