#ifndef RETURN_INSTRUMENTOR_VISITOR_H
#define RETURN_INSTRUMENTOR_VISITOR_H

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

// STL libs
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include <utility>

class ReturnInstrumentorVisitor : public clang::RecursiveASTVisitor<ReturnInstrumentorVisitor> {
    private:
        // Context storing additional state
        clang::ASTContext *astContext;

        // Client to rewrite source
        std::shared_ptr<clang::Rewriter> rewriter;

        std::string targetFunctionNameString;
        // Name of the target function to instrument and names of its parameters.
        std::vector<std::string> targetFunctionNameAndArgs;

        std::shared_ptr<bool> shouldFlush;

        clang::SourceRange targetFunctionRange;

        std::vector<std::string> getFunctionNameAndArgs(const clang::FunctionDecl *decl);

        void initForInstru(const clang::FunctionDecl *decl);

        bool isTargetFunction(const clang::FunctionDecl *decl);

        bool inRange(clang::SourceRange largeRange, clang::SourceRange smallRange);

        bool inTargetFunction(const clang::Stmt *stmt);

        std::string exprToString(const clang::Expr *expr);

    public:
        explicit ReturnInstrumentorVisitor(clang::CompilerInstance &ci, 
                              std::shared_ptr<clang::Rewriter> _rewriter,
                              std::string _targetFunctionNameAndArgs,
                              std::shared_ptr<bool> _shouldFlush);

        ~ReturnInstrumentorVisitor();

        // Override trigger for when a ReturnStmt is found in the AST
        virtual bool VisitReturnStmt(const clang::ReturnStmt *stmt);

        virtual bool VisitFunctionDecl(const clang::FunctionDecl *decl);
};

#endif