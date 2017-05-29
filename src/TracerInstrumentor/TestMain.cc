#include "TestVisitor.h"

// Clang libs
#include "llvm/Support/CommandLine.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"

// STL libs
#include <string>
#include <iostream>
#include <getopt.h>

using namespace llvm;
using namespace clang::tooling;

cl::OptionCategory TestOptions("Test options");

int main(int argc, const char **argv) {
    CommonOptionsParser OptionsParser(argc, argv, TestOptions);

    std::vector<std::string> files;
    files.push_back("visitor_test.cc");

    ClangTool EventAnnotatorTool(OptionsParser.getCompilations(), files);

    EventAnnotatorTool.run(CreateTestFrontendActionFactory().get());

    return 0;
}