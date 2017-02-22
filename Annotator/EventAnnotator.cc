// VProf libs
#include "FunctionFileReader.h"
#include "FileFinder.h"
#include "VProfFrontendActionFactory.h"
#include "WrapperGenerator.h"

// Clang libs
#include "llvm/Support/CommandLine.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"

// STL libs
#include <string>
#include <iostream>
#include <getopt.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * TODO reimplement only creating AST for files with the functions in them.  *
 * FileFinder fileFinder(sourceBaseDir);                                     *
 * if (!cscopeDBCompiled) {                                                  *
 *     fileFinder.BuildCScopeDB();                                           *
 * }                                                                         *
 * vector<string> potentialFiles = fileFinder.FindFunctionsPotentialFiles(   *
 *                         funcFileReader.GetUnqualifiedFunctionNames());    *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

using namespace std;
using namespace llvm;
using namespace clang::tooling;

cl::OptionCategory VProfOptions("VProfiler options");
cl::opt<string> FunctionFileOpt("f", 
                                cl::desc("Specify filename containing fully "
                                         "qualified function names to profile"),
                                cl::value_desc("Filename"),
                                cl::Required,
                                cl::ValueRequired);

cl::opt<string> SourceBaseDir("s",
                              cl::desc("Specifies the root directory of the "
                                       "source tree."),
                              cl::value_desc("Source_Base_Dir"),
                              cl::Required,
                              cl::ValueRequired);


int main(int argc, const char **argv) {
    CommonOptionsParser OptionsParser(argc, argv, VProfOptions);

    FunctionFileReader funcFileReader(FunctionFileOpt);
    funcFileReader.Parse();

    FileFinder fileFinder(SourceBaseDir);
    fileFinder.BuildCScopeDB();

    ClangTool EventAnnotatorTool(OptionsParser.getCompilations(),
                                 fileFinder.FindFunctionsPotentialFiles(funcFileReader.GetUnqualifiedFunctionNames()));

    shared_ptr<unordered_map<string, FunctionPrototype>> prototypeMap = make_shared<unordered_map<string, FunctionPrototype>>();

    EventAnnotatorTool.run(newVProfFrontendActionFactory(funcFileReader.GetFunctionMap(),
                                                         prototypeMap).get());
    
    WrapperGenerator wrapperGenerator(prototypeMap, funcFileReader.GetLogInfoMap());
    wrapperGenerator.GenerateWrappers();

    return 0;
}
