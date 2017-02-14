// Clang libs
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Rewrite/Rewriter.h"

// STL libs
#include <vector>
#include <unordered_map>
#include <string>

class CodeTransformer {
    public:
        static unique_ptr<CodeTransformer> GetInstance();
        // Engine class which runs clang related source translation

        // Runs the source translation on a single source file.
        bool TransformFile(std::string &filename);
        
        // Creates the singleton instance of CodeTranslator.  
        // functionNames maps fully qualified function names which 
        // the user would like profiled to their VProfiler equivalent.
        static void CodeTransformer::CreateCodeTransformer(std::unordered_map<std::string, 
                                                         std::string> &functionNames);

    private:
        // Ctor only called in GetInstance
        CodeTransformer(std::unordered_map<std::string, std::string> &functionNames);

        // Singleton pattern
        static std::unique_ptr<CodeTransformer> singleton;

        // Maps fully qualified function names which the user would like profiled
        // to their VProfiler equivalent.
        std::unordered_map<std::string, std::string> functionNames;

        // Clang internals
        CompilerInstance compiler;
        Rewriter rewriter;
        FileManager fileManager;
        SourceManager sourceManager;

        // VProf clang overrides
        VProfASTConsumer astConsumer;
};
