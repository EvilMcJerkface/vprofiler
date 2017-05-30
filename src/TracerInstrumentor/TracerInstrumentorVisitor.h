#ifndef TRACER_INSTRUMENTOR
#define TRACER_INSTRUMENTOR

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

class TracerInstrumentorVisitor : public clang::RecursiveASTVisitor<TracerInstrumentorVisitor> {
    private:
        // Context storing additional state
        clang::ASTContext *astContext;

        // Client to rewrite source
        std::shared_ptr<clang::Rewriter> rewriter;

        std::string targetFunctionNameString;
        // Name of the target function to instrument and names of its parameters.
        std::vector<std::string> targetFunctionNameAndArgs;

        std::shared_ptr<bool> shouldFlush;

        std::shared_ptr<std::pair<clang::SourceLocation, std::string>> wrapperImplLoc;

        std::string functionNamesFile;

        std::ofstream functionNamesStream;

        clang::SourceRange targetFunctionRange;

        const clang::Expr *condExpr;
        clang::SourceRange condRange;
        bool instruDoneForCond;

        int functionIndex;

        std::string functionNameToWrapperName(std::string functionName);

        void fixFunction(const clang::CallExpr *call, const std::string &functionName,
                         bool  isMemberCall);

        void appendNonObjArgs(std::string &newCall, std::vector<const clang::Expr*> &args);

        // Creates the wrapper prototype based on function decl.
        void createNewPrototype(const clang::FunctionDecl *decl,
                                const std::string &functionName,
                                bool isMemberFunc);

        std::string getEntireParamDeclAsString(const clang::ParmVarDecl *decl);

        std::string getContainingFilename(const clang::FunctionDecl *decl);

        std::vector<std::string> getFunctionNameAndArgs(const clang::FunctionDecl *decl);

        void initForInstru(const clang::FunctionDecl *decl);

        bool isTargetFunction(const clang::FunctionDecl *decl);

        bool inRange(clang::SourceRange largeRange, clang::SourceRange smallRange);

        bool inTargetFunction(const clang::Expr *call);
        bool inTargetFunction(const clang::Stmt *stmt);

        void saveCondExpr(const clang::Expr *cond);

        bool inCondRange(const clang::Expr *call);

        std::string generateWrapperImpl(FunctionPrototype prototype);

    public:
        explicit TracerInstrumentorVisitor(clang::CompilerInstance &ci, 
                              std::shared_ptr<clang::Rewriter> _rewriter,
                              std::string _targetFunctionNameAndArgs,
                              std::shared_ptr<bool> _shouldFlush,
                              std::shared_ptr<std::pair<clang::SourceLocation, std::string>> _wrapperImplLoc,
                              std::string _functionNamesFile);

        ~TracerInstrumentorVisitor();

        // Override trigger for when a FunctionDecl is found in the AST
        virtual bool VisitFunctionDecl(const clang::FunctionDecl *decl);

        // Override trigger for when a CXXMethod is found in the AST
        // Doesn't seem to be necessary. Covered by the previous function.
        // virtual bool VisitCXXMethodDecl(const clang::CXXMethodDecl *decl);

        // Override trigger for when a CallExpr is found in the AST
        virtual bool VisitCallExpr(const clang::CallExpr *call);

        // Override trigger for when a CXXMemberCallExpr is found in the AST
        virtual bool VisitCXXMemberCallExpr(const clang::CXXMemberCallExpr *call);
};

#endif