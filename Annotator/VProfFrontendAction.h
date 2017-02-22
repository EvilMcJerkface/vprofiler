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

    public:
        VProfFrontendAction(std::shared_ptr<std::unordered_map<std::string, 
                                   std::string>> _functionNameMap,
                                   std::shared_ptr<std::unordered_map<std::string,
                                   FunctionPrototype>> _prototypeMap):
                                   functionNameMap(_functionNameMap),
                                   prototypeMap(_prototypeMap) {}

        ~VProfFrontendAction() {
        }

        virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &ci,
                                                                      llvm::StringRef file) {
            std::shared_ptr<clang::Rewriter> rewriter = std::make_shared<clang::Rewriter>();
            rewriter->setSourceMgr(ci.getSourceManager(), ci.getLangOpts());

            return std::unique_ptr<VProfASTConsumer>(new VProfASTConsumer(ci,
                                                                          rewriter,
                                                                          functionNameMap,
                                                                          prototypeMap,
                                                                          file.str()));
        }
};

#endif
