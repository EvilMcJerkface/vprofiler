#include "TracerInstrumentorFrontendActionFactory.h"
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
                             cl::Required,
                             cl::ValueRequired);

cl::opt<std::string> SourceBaseDir("s",
                              cl::desc("Specifies the root directory of the "
                                       "source tree."),
                              cl::value_desc("Source_Base_Dir"),
                              cl::Required,
                              cl::ValueRequired);

cl::opt<std::string> BackupDir("b",
                              cl::desc("Specifies the dir for back up files."),
                              cl::value_desc("Backup_Dir"),
                              cl::Required,
                              cl::ValueRequired);

cl::opt<std::string> FunctionNamesFile("n",
                              cl::desc("Specifies the path of the function names file."),
                              cl::value_desc("Function_Names_File"),
                              cl::Required,
                              cl::ValueRequired);
                              

std::string getUnqualifiedFunctionName(std::string functionNameAndArgs) {
    std::string qualifiedName = SplitString(FunctionNameAndArgs, '|')[0];
    std::string nameAndIndex = SplitString(qualifiedName, ':').back();
    return SplitString(nameAndIndex, '-')[0];
}

void fixReturns(std::string file) {
    execute("sed -r -i 's/return (.*_vprofiler.*)/resVprof = \\1\\n\\tTRACE_FUNCTION_END();\\n\\treturn resVprof;/g' " + file);
}

int main(int argc, const char **argv) {
    CommonOptionsParser OptionsParser(argc, argv, TracerInstrumentorOptions);

    FileFinder fileFinder(SourceBaseDir);
    fileFinder.BuildCScopeDB();
    std::string functionName = getUnqualifiedFunctionName(FunctionNameAndArgs);
    std::string targetFile = fileFinder.FindFunctionDefinition(functionName);

    if (targetFile.size() == 0) {
        std::cout << "Function " << functionName << " not found" << std::endl;
        return 0;
    }

    std::vector<std::string> files;
    files.push_back(targetFile);

    ClangTool TracerInstrumentator(OptionsParser.getCompilations(), files);

    TracerInstrumentator.run(CreateTracerInstrumentorFrontendActionFactory(FunctionNameAndArgs, BackupDir, FunctionNamesFile).get());

    for (size_t i = 0; i < files.size(); ++i) {
        fixReturns(files[i]);
    }

    return 0;
}
