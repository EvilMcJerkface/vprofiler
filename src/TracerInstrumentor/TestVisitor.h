#ifndef TEST_VISITOR_H
#define TEST_VISITOR_H

// STL libs
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <sstream>
#include <system_error>

// Clang libs
#include "clang/Tooling/Tooling.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Type.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"

class TestVisitor : public clang::RecursiveASTVisitor<TestVisitor> {
    private:
        // Context storing additional state
        clang::ASTContext *astContext;

        // Client to rewrite source
        std::shared_ptr<clang::Rewriter> rewriter;

        const clang::Expr *condExpr;
        clang::SourceRange condRange;

    public:
        explicit TestVisitor(clang::CompilerInstance &ci,
                              std::shared_ptr<clang::Rewriter> _rewriter);

        ~TestVisitor(); 

        // Add braces to line of code
        virtual bool VisitStmt(const clang::Stmt *stmt);

        // Add braces to line of code
        void AddBraces(const clang::Stmt *stmt);

        // Override trigger for when a FunctionDecl is found in the AST
        virtual bool VisitFunctionDecl(const clang::FunctionDecl *decl);

        // Override trigger for when a CallExpr is found in the AST
        virtual bool VisitCallExpr(const clang::CallExpr *call);

        // Override trigger for when a CXXMemberCallExpr is found in the AST
        virtual bool VisitCXXMemberCallExpr(const clang::CXXMemberCallExpr *call);
};

class TestASTConsumer : public clang::ASTConsumer {
    private:
        std::unique_ptr<TestVisitor> visitor;

    public:
        // Override ctor to pass hash map of fully qualified function names to the function
        // name to which the key functions should be converted to in the source.
        explicit TestASTConsumer(clang::CompilerInstance &ci, 
                                  std::shared_ptr<clang::Rewriter> _rewriter) {
            
            visitor = std::unique_ptr<TestVisitor>(new TestVisitor(ci, _rewriter));
        }

        ~TestASTConsumer() {}

        virtual void HandleTranslationUnit(clang::ASTContext &context) {
            visitor->TraverseDecl(context.getTranslationUnitDecl());
        }
};


class TestFrontendAction : public clang::ASTFrontendAction {
    private:
        // Name of the transformed file.
        std::string filename;
        
        // Rewriter used by the consumer.
        std::shared_ptr<clang::Rewriter> rewriter;

    public:
        TestFrontendAction() {}

        ~TestFrontendAction() {}

        void EndSourceFileAction() override {
            clang::SourceManager &SM = rewriter->getSourceMgr();

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

            return std::unique_ptr<TestASTConsumer>(new TestASTConsumer(ci, rewriter));
        }
};


class TestFrontendActionFactory : public clang::tooling::FrontendActionFactory {
    public:
        TestFrontendActionFactory() {}

        // Creates a TestFrontendAction to be used by clang tool.
        virtual TestFrontendAction *create() {
            return new TestFrontendAction();
        }
};

// This is absurdly long, but not sure how to break lines up to make more
// readable
std::unique_ptr<TestFrontendActionFactory> CreateTestFrontendActionFactory();

#endif