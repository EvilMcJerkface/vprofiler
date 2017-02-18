#ifndef CLANGBASE_H
#define CLANGBASE_H

// Clang libs
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Type.h"
#include "clang/AST/Decl.h"
#include "clang/Rewrite/Core/Rewriter.h"

// LLVM libs
//#include "llvm/Support/raw_ostream.h"

// STL libs
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

class VProfVisitor : public clang::RecursiveASTVisitor<VProfVisitor> {
    private:
        // Context storing additional state
        std::unique_ptr<clang::ASTContext> astContext;

        // Client to rewrite source
        std::shared_ptr<clang::Rewriter> rewriter;

        // Hash map of fully qualified function names to the function
        // name to which the key functions should be converted to in the source.
        std::unordered_map<std::string, std::string> functions;

        // FOR DEBUGGING PURPOSES. PRINT SUPPOSED FUNCTION CALL INSTEAD OF WRITING TO FILE
        void fixFunction(const clang::CallExpr *call, const std::string &functionName,
                         bool  isMemberCall);

        void appendNonObjArgs(std::string &newCall, std::vector<const clang::Expr*> &args);

    public:
        explicit VProfVisitor(std::shared_ptr<clang::CompilerInstance> ci, std::shared_ptr<clang::Rewriter> _rewriter,
                              std::unordered_map<std::string, std::string> &_functions);

        ~VProfVisitor(); 

        // Override trigger for when a CallExpr is found in the AST
        virtual bool VisitCallExpr(const clang::CallExpr *call);

        // Override trigger for when a CXXMemberCallExpr is found in the AST
        virtual bool VisitCXXMemberCallExpr(const clang::CXXMemberCallExpr *call);
};

class VProfASTConsumer : public clang::ASTConsumer {
    private:
        std::unique_ptr<VProfVisitor> visitor;
    public:
        // Override ctor to pass hash map of fully qualified function names to the function
        // name to which the key functions should be converted to in the source.
        explicit VProfASTConsumer(std::shared_ptr<clang::CompilerInstance> ci, std::shared_ptr<clang::Rewriter> rewriter,
                               std::unordered_map<std::string, std::string> &functions): 
                               visitor(std::unique_ptr<VProfVisitor>(new VProfVisitor(ci, rewriter, functions))) {}

        //virtual ~VProfASTConsumer() {}

        virtual void HandleTranslationUnit(clang::ASTContext &context) {
            visitor->TraverseDecl(context.getTranslationUnitDecl());
        }
};

/* Don't think this is needed
 * class VProfFrontendAction : public clang::ASTFrontendAction {
    public:
        virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef file) {
            return std::unique_ptr<VProfASTConsumer>(new VProfASTConsumer(&ci));
        }
};*/

#endif
