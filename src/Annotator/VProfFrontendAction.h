#ifndef VPROFFRONTENDACTION_H
#define VPROFFRONTENDACTION_H

// Clang libs
#include "clang/Frontend/FrontendAction.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include <unordered_map>
#include <string>
#include <memory>

// VProf libs
#include "ClangBase.h"

class VProfFrontendAction : public clang::ASTFrontendAction {
    private:
        // Maps qualified function name to vprof wrapper name.
        std::shared_ptr<std::unordered_map<std::string, std::string>> functionNameMap;

        // Maps qualified function name to FunctionPrototype object.
        std::shared_ptr<std::unordered_map<std::string, FunctionPrototype>> prototypeMap;

        // Name of the transformed file.
        std::string filename;
        
        // Rewriter used by the consumer.
        std::shared_ptr<clang::Rewriter> rewriter;

    public:
        VProfFrontendAction(std::shared_ptr<std::unordered_map<std::string, 
                                   std::string>> _functionNameMap,
                                   std::shared_ptr<std::unordered_map<std::string,
                                   FunctionPrototype>> _prototypeMap):
                                   functionNameMap(_functionNameMap),
                                   prototypeMap(_prototypeMap) {}

        ~VProfFrontendAction() {}

        void EndSourceFileAction() override {
            clang::SourceManager &SM = rewriter->getSourceMgr();
            filename.insert(filename.find("."), "_vprof");

            std::error_code OutErrInfo;
            std::error_code ok;

            llvm::raw_fd_ostream outputFile(llvm::StringRef(filename), 
                                            OutErrInfo, llvm::sys::fs::F_None); 

            if (OutErrInfo == ok) {
                rewriter->getEditBuffer(SM.getMainFileID()).write(outputFile);
            }
        }

        virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &ci,
                                                                      llvm::StringRef file) {
            rewriter = std::make_shared<clang::Rewriter>();
            rewriter->setSourceMgr(ci.getSourceManager(), ci.getLangOpts());

            filename = file.str();

            return std::unique_ptr<VProfASTConsumer>(new VProfASTConsumer(ci,
                                                                          rewriter,
                                                                          functionNameMap,
                                                                          prototypeMap,
                                                                          filename));
        }
};

#endif
