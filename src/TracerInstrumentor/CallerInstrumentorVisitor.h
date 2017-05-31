#ifndef CALLER_INSTRUMENTOR_VISITOR_H
#define CALLER_INSTRUMENTOR_VISITOR_H

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

#include "FunctionPrototype.h"

class CallerInstrumentorVisitor : public clang::RecursiveASTVisitor<CallerInstrumentorVisitor> {
    private:
        // Context storing additional state
        clang::ASTContext *astContext;

        // Client to rewrite source
        std::shared_ptr<clang::Rewriter> rewriter;

        std::string targetFunctionNameString;
        // Name of the target function to instrument and names of its parameters.
        std::vector<std::string> targetFunctionNameAndArgs;

        std::vector<std::string> callerNameAndArgs;

        int targetInCallerIndex;
        
        int targetPathCount;

        std::shared_ptr<bool> shouldFlush;

        clang::SourceRange callerRange;

        int functionIndex;

        std::vector<std::string> getFunctionNameAndArgs(const clang::FunctionDecl *decl);

        void initForInstru(const clang::FunctionDecl *decl);

        bool inRange(clang::SourceRange largeRange, clang::SourceRange smallRange);

        bool isTargetFunction(const clang::FunctionDecl *decl);

        bool isCaller(const clang::FunctionDecl *decl);

        bool inCaller(const clang::Expr *call);
        bool inCaller(const clang::Stmt *stmt);

        void insertPathCountUpdates(const clang::CallExpr *call);

        void addBraces(const clang::Stmt *s);

    public:
        explicit CallerInstrumentorVisitor(clang::CompilerInstance &ci, 
                              std::shared_ptr<clang::Rewriter> _rewriter,
                              std::string _targetFunctionNameAndArgs,
                              std::string _callerNameAndArgs,
                              int _targetPathCount,
                              std::shared_ptr<bool> _shouldFlush);

        ~CallerInstrumentorVisitor();

        // Override trigger for when a FunctionDecl is found in the AST
        virtual bool VisitFunctionDecl(const clang::FunctionDecl *decl);

        // Override trigger for when a Stmt is found in the AST
        virtual bool VisitStmt(const clang::Stmt *s);

        // Override trigger for when a CallExpr is found in the AST
        virtual bool VisitCallExpr(const clang::CallExpr *call);

        // Override trigger for when a CXXMemberCallExpr is found in the AST
        virtual bool VisitCXXMemberCallExpr(const clang::CXXMemberCallExpr *call);
};

#endif