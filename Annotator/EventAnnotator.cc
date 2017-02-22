// VProf libs
#include "FunctionFileReader.h"
#include "FileFinder.h"
#include "VProfFrontendActionFactory.h"

// Clang libs
#include "llvm/Support/CommandLine.h"
#include "clang/Tooling/Tooling.h"

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
using namespace cl;
using namespace llvm;
using namespace clang::tooling;

OptionCategory VProfOptions("VProfiler options");
static opt<string> FunctionFileOpt("f", 
                                   desc("Specify filename containing fully "
                                        "qualified function names to profile"),
                                   value_desc("Filename"),
                                   Required);


int main(int argc, char **argv) {
    bool cscopeDBCompiled = false;

    CommonOptionsParser OptionsParser(argc, argv, VProfOptions);

    FunctionFileReader funcFileReader(FunctionFileOpt);
    funcFileReader.Parse();

    ClangTool EventAnnotatorTool(OptionsParser.getCompilations(),
                                 OptionsParser.getSourcePathList());

    shared_ptr<unordered_map<string, FunctionPrototype>> prototypeMap = make_shared<unordered_map<string, FunctionPrototype>>();

    EventAnnotatorTool.run(newVProfFrontendActionFactory(funcFileReader.GetFunctionMap(),
                                                         prototypeMap));
    
    WrapperGenerator wrapperGenerator(prototypeMap);
    wrapperGenerator.GenerateWrappers();

    return 0;
}
