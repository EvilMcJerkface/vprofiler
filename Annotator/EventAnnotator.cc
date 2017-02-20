// VProf libs
#include "FunctionFileReader.h"
#include "FileFinder.h"
#include "CodeTransformer.h"

// STL libs
#include <string>
#include <iostream>
#include <getopt.h>

using namespace std;

int main(int argc, char **argv) {
    int c;
    string functionFilename;
    string sourceBaseDir;
    bool cscopeDBCompiled = false;

    string usageStr = "Event Annotator usage:\n -f is the path to a file \
                       containing one fully qualified function name per \
                       line.\n  Use -c to indicate that cscope -R need not be run.\
                       -s indicates the base directory of the source to be profiled.";

    opterr = 0;

    // Add breaking if functionFilename and sourceBaseDir are never set.
    while ((c = getopt(argc, argv, "hf:s:c")) != -1) {
        switch (c) {
            case 'h':
                cout << usageStr;
                return 1;

            case 'f':
                functionFilename = optarg;
                break;

            case 's':
                sourceBaseDir = optarg;
                break;

            case 'c':
                cscopeDBCompiled = true;
                break;

            default:
                cout << "Unrecognized option " + optopt << endl << usageStr;
                return 1;
        }
    }

    if (sourceBaseDir == "" || functionFilename == "") {    
        cout << usageStr;
        return 1;
    }

    FunctionFileReader funcFileReader(functionFilename);
    funcFileReader.Parse();

    FileFinder fileFinder(sourceBaseDir);
    if (!cscopeDBCompiled) { 
        fileFinder.BuildCScopeDB();
    }
    vector<string> potentialFiles = fileFinder.FindFunctionsPotentialFiles(
                            funcFileReader.GetUnqualifiedFunctionNames());

    CodeTransformer::CreateCodeTransformer(funcFileReader.GetFunctionMap());
    
    //CodeTransformer ct(funcFileReader.GetFunctionMap());

    for (std::string &potentialFile : potentialFiles) {
        CodeTransformer::GetInstance()->TransformFile(potentialFile);
        //ct.TransformFile(potentialFile);
    }

    return 0;
}
