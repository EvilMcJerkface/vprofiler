#ifndef RETURN_INSTRUMENTOR_FRONT_END_FACTORY_H
#define RETURN_INSTRUMENTOR_FRONT_END_FACTORY_H

#include "ReturnInstrumentorVisitor.h"

#include "clang/Tooling/Tooling.h"

class ReturnInstrumentorASTConsumer : public clang::ASTConsumer {
    private:
        std::unique_ptr<ReturnInstrumentorVisitor> visitor;

    public:
        // Override ctor to pass hash map of fully qualified function names to the function
        // name to which the key functions should be converted to in the source.
        explicit ReturnInstrumentorASTConsumer(clang::CompilerInstance &ci, 
                                std::shared_ptr<clang::Rewriter> _rewriter,
                                std::string _targetFunctionName,
                                std::shared_ptr<bool> _shouldFlush) {
            
            visitor = std::unique_ptr<ReturnInstrumentorVisitor>(new ReturnInstrumentorVisitor(ci, 
                                                                    _rewriter,
                                                                    _targetFunctionName,
                                                                    _shouldFlush));
        }

        ~ReturnInstrumentorASTConsumer() {}

        virtual void HandleTranslationUnit(clang::ASTContext &context) {
            visitor->TraverseDecl(context.getTranslationUnitDecl());
        }
};

class ReturnInstrumentorFrontendAction : public clang::ASTFrontendAction {
    private:
        // Name of the transformed file.
        std::string filename;
        
        // Rewriter used by the consumer.
        std::shared_ptr<clang::Rewriter> rewriter;

        // Name of the target function.
        std::string targetFunctionName;

        clang::FileID fileID;

        std::shared_ptr<bool> shouldFlush;

    public:
        ReturnInstrumentorFrontendAction(std::string _targetFunctionName) :
                                            targetFunctionName(_targetFunctionName),
                                            shouldFlush(std::make_shared<bool>(false)) {}

        ~ReturnInstrumentorFrontendAction() {}

        void EndSourceFileAction() override {
            if (*shouldFlush) {
                std::error_code OutErrInfo;
                std::error_code ok;

                llvm::raw_fd_ostream outputFile(llvm::StringRef(filename), 
                                                OutErrInfo, llvm::sys::fs::F_None); 

                if (OutErrInfo == ok) {
                    const clang::RewriteBuffer *RewriteBuf = rewriter->getRewriteBufferFor(fileID);
                    outputFile << std::string(RewriteBuf->begin(), RewriteBuf->end());
                    outputFile.close();
                }
            }
        }

        virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &ci,
                                                                      llvm::StringRef file) {
            rewriter = std::make_shared<clang::Rewriter>();
            rewriter->setSourceMgr(ci.getSourceManager(), ci.getLangOpts());

            filename = file.str();
            fileID = ci.getSourceManager().getMainFileID();

            return std::unique_ptr<ReturnInstrumentorASTConsumer>(new ReturnInstrumentorASTConsumer(ci,
                                                                                                    rewriter,
                                                                                                    targetFunctionName,
                                                                                                    shouldFlush));
        }
};

class ReturnInstrumentorFrontendActionFactory : public clang::tooling::FrontendActionFactory {
    private:
        std::string targetFunctionName;

    public:
        ReturnInstrumentorFrontendActionFactory(std::string _targetFunctionName) :
                                                targetFunctionName(_targetFunctionName) {}

        // Creates a ReturnInstrumentorFrontendAction to be used by clang tool.
        virtual ReturnInstrumentorFrontendAction *create() {
            return new ReturnInstrumentorFrontendAction(targetFunctionName);
        }
};

// This is absurdly long, but not sure how to break lines up to make more
// readable
std::unique_ptr<ReturnInstrumentorFrontendActionFactory> CreateReturnInstrumentorFrontendActionFactory(
        std::string targetFunctionName) {
    return std::unique_ptr<ReturnInstrumentorFrontendActionFactory>(
        new ReturnInstrumentorFrontendActionFactory(targetFunctionName));
}

#endif
