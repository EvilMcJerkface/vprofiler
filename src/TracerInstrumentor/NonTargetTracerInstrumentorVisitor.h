#ifndef NON_TARGET_TRACER_INSTRUMENTOR_VISITOR_H
#define NON_TARGET_TRACER_INSTRUMENTOR_VISITOR_H

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
#include <tuple>

#include "FunctionPrototype.h"

class NonTargetTracerInstrumentorVisitor : public clang::RecursiveASTVisitor<NonTargetTracerInstrumentorVisitor> {
    private:
        // Context storing additional state
        clang::ASTContext *astContext;

        // Client to rewrite source
        std::shared_ptr<clang::Rewriter> rewriter;

        std::vector<std::vector<std::string>> rootFunctionNamesAndArgs;
        std::shared_ptr<std::unordered_map<int, bool>> funcDone;

        std::shared_ptr<bool> shouldFlush;

        std::shared_ptr<std::pair<clang::SourceLocation, std::string>> wrapperImplLoc;

        std::shared_ptr<std::vector<clang::SourceLocation>> funcStartLocs;
        std::shared_ptr<int> numFuncs;

        std::string functionNamesFile;

        std::ofstream functionNamesStream;

        clang::SourceRange currentRootRange;
        std::string currentRootFunc;

        std::string functionNameToWrapperName(std::string functionName);

        std::string getRootFuncName(int rootIndex);

        void fixFunction(const clang::CallExpr *call, const std::string &functionName,
                         bool  isMemberCall);

        void appendNonObjArgs(std::string &newCall, std::vector<const clang::Expr*> &args);

        // Creates the wrapper prototype based on function decl.
        void createNewPrototype(const clang::FunctionDecl *decl,
                                const std::string &functionName,
                                bool isMemberFunc);

        std::string getFileAndLine(const clang::FunctionDecl *decl);

        std::string getEntireParamDeclAsString(const clang::ParmVarDecl *decl);

        std::string getContainingFilename(const clang::FunctionDecl *decl);

        std::vector<std::string> getFunctionNameAndArgs(const clang::FunctionDecl *decl);

        void initForInstru(const clang::FunctionDecl *decl);

        int isRootFunction(const clang::FunctionDecl *decl);

        bool inRange(clang::SourceRange largeRange, clang::SourceRange smallRange);

        bool inRootFunction(const clang::Expr *call);
        bool inRootFunction(const clang::Stmt *stmt);
        

        std::string generateWrapperImpl(FunctionPrototype prototype);

    public:
        explicit NonTargetTracerInstrumentorVisitor(clang::CompilerInstance &ci, 
                              std::shared_ptr<clang::Rewriter> _rewriter,
                              std::vector<std::vector<std::string>> &_rootFunctionNamesAndArgs,
                              std::shared_ptr<std::unordered_map<int, bool>> _funcDone,
                              std::shared_ptr<bool> _shouldFlush,
                              std::shared_ptr<std::pair<clang::SourceLocation, std::string>> _wrapperImplLoc,
                              std::shared_ptr<std::vector<clang::SourceLocation>> _funcStartLocs,
                              std::shared_ptr<int> _numFuncs,
                              std::string _functionNamesFile);

        ~NonTargetTracerInstrumentorVisitor();

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