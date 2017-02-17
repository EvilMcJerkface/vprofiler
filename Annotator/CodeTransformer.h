// Clang libs
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Rewrite/Core/Rewriter.h"

// STL libs
#include <vector>
#include <unordered_map>
#include <string>

// VProf libs
#include "ClangBase.h"

class CodeTransformer {
    public:
        static CodeTransformer* GetInstance();
        // Engine class which runs clang related source translation

        // Runs the source translation on a single source file.
        bool TransformFile(std::string &filename);
        
        // Creates the singleton instance of CodeTranslator.  
        // functionNames maps fully qualified function names which 
        // the user would like profiled to their VProfiler equivalent.
        static void CreateCodeTransformer(const std::unordered_map<std::string, std::string> &functionNames);

    private:
        // Copy ctor
        CodeTransformer(const CodeTransformer *other);

        // Ctor only called in GetInstance
        CodeTransformer(const std::unordered_map<std::string, std::string> &functionNames);

        // Singleton pattern
        static std::unique_ptr<CodeTransformer> singleton;

        // Maps fully qualified function names which the user would like profiled
        // to their VProfiler equivalent.
        std::unordered_map<std::string, std::string> functionNames;

        // Clang internals
        clang::CompilerInstance compiler;
        clang::Rewriter rewriter;
        clang::FileManager *fileManager;
        clang::SourceManager *sourceManager;

        // VProf clang overrides
        std::unique_ptr<VProfASTConsumer> astConsumer;
};
