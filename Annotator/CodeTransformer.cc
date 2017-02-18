#include <stdexcept>

#include "CodeTransformer.h"

using namespace clang;

std::unique_ptr<CodeTransformer> CodeTransformer::singleton = nullptr;

bool CodeTransformer::TransformFile(std::string &filename) { 
    CreateCompiler();

    const FileEntry *file = fileManager->getFile(filename);
    sourceManager->setMainFileID(sourceManager->createFileID(file, SourceLocation(), SrcMgr::C_User));

    compiler->getDiagnosticClient().BeginSourceFile(
             compiler->getLangOpts(),
             &compiler->getPreprocessor());

    ParseAST(compiler->getPreprocessor(), astConsumer.get(), 
             compiler->getASTContext());

    compiler->getDiagnosticClient().EndSourceFile();

    std::string outFilename = filename;
    outFilename.insert(outFilename.find("."), "_vprof");

    std::error_code OutErrInfo;
    std::error_code ok;

    llvm::raw_fd_ostream outputFile(llvm::StringRef(outFilename), OutErrInfo, llvm::sys::fs::F_None); 

    if (OutErrInfo == ok) {
        const RewriteBuffer *RewriteBuf = rewriter->getRewriteBufferFor(sourceManager->getMainFileID());

        outputFile << std::string(RewriteBuf->begin(), RewriteBuf->end());
    }

    return true;
}

// Returns nullptr if CodeTransformer::CreateCodeTransformer was not called previously
CodeTransformer* CodeTransformer::GetInstance() {
    return singleton.get();
}

void CodeTransformer::CreateCodeTransformer(const std::unordered_map<std::string, std::string> &functionNames) {
    if (!singleton) { 
        singleton = std::unique_ptr<CodeTransformer>(new CodeTransformer(functionNames));
    }
}

void CodeTransformer::CreateCompiler() {
    compiler = std::make_shared<CompilerInstance>();
    rewriter = std::make_shared<Rewriter>();

    compiler->createDiagnostics(nullptr, false);

    compiler->getLangOpts().CPlusPlus = 1;

    std::shared_ptr<TargetOptions> options = std::make_shared<TargetOptions>();
    options->Triple = llvm::sys::getDefaultTargetTriple();
    TargetInfo *targInfo = TargetInfo::CreateTargetInfo(
        compiler->getDiagnostics(), options);
    compiler->setTarget(targInfo);

    compiler->createFileManager();
    fileManager = &compiler->getFileManager();

    compiler->createSourceManager(*fileManager);
    sourceManager = &compiler->getSourceManager();

    compiler->createPreprocessor(clang::TU_Complete);
    compiler->createASTContext();

    rewriter->setSourceMgr(*sourceManager, compiler->getLangOpts());

    astConsumer = std::unique_ptr<VProfASTConsumer>(new VProfASTConsumer(compiler, rewriter, functionNames));
}

CodeTransformer::CodeTransformer(const std::unordered_map<std::string, std::string> &functions):
compiler(std::make_shared<CompilerInstance>()), rewriter(std::make_shared<Rewriter>()){
    functionNames = functions;
}
