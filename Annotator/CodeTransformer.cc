#include <stdexcept>

#include "CodeTransformer.h"

unique_ptr<CodeTransformer> CodeTransformer::singleton = nullptr;

bool CodeTransformer::Run(std::string &filename) { 
    const FileEntry *file = fileManager.getFile(filename);
    sourceManager.createMainFileID(file);

    compiler.getDiagnosticClient().BeginSourceFile(
             compiler.getLangOpts(),
             &compiler.getPreprocessor());

    ParseAST(compiler.getPreprocessor(), &vprofConsumer, 
             compiler.getASTContext());
}

// Returns nullptr if CodeTransformer::CreateCodeTransformer was not called previously
static unique_ptr<CodeTransformer> CodeTransformer::GetInstance() {
    if (singleton == nullptr) {
        throw std::logic_error("CodeTransformer::GetInstance called before CodeTransformer::CreateCodeTransformer was invoked.");
    }
    return singleton;
}

static void CodeTransformer::CreateCodeTransformer(std::unordered_map<std::string, std::string> &functionNames) {
    singleton = make_unique<CodeTransformer>(functionNames);
}

CodeTransformer::CodeTransformer(std::unordered_map<std::string, std::string> &_functionNames) {
    functionNames = _functionNames;

    compiler.createDiagnostics(0, 0);

    TargetOptions options;
    options.Triple = llvm::sys::getDefaultTargetTriple();
    TargetInfo *targInfo = TargetInfo::CreateTargetInfo(
        compiler.getDiagnostics(), options);
    compiler.setTarget(targInfo);

    compiler.createFileManager();
    fileManager = compiler.getFileManager();

    compiler.createSourceManager(fileManager);
    sourceManager = compiler.getSourceManager();

    compiler.createPreprocessor();
    compiler.createASTContext();

    rewrite.setSourceMgr(sourceManager, compiler.getLangOpts());
}
