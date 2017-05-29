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
cl::opt<std::string> FunctionName("f",
                             cl::desc("Specify name of the function to instrument"),
                             cl::value_desc("Fuction_Name"),
                             cl::Required,
                             cl::ValueRequired);

cl::opt<std::string> SourceBaseDir("s",
                              cl::desc("Specifies the root directory of the "
                                       "source tree."),
                              cl::value_desc("Source_Base_Dir"),
                              cl::Required,
                              cl::ValueRequired);

int main(int argc, const char **argv) {
    CommonOptionsParser OptionsParser(argc, argv, TracerInstrumentorOptions);

    FileFinder fileFinder(SourceBaseDir);
    fileFinder.BuildCScopeDB();
    std::string targetFile = fileFinder.FindFunctionDefinition(FunctionName);

    std::vector<std::string> files;
    files.push_back(targetFile);

    ClangTool EventAnnotatorTool(OptionsParser.getCompilations(), files);

    EventAnnotatorTool.run(CreateTracerInstrumentorFrontendActionFactory(FunctionName).get());

    return 0;
}
