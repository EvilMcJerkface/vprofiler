// Clang libs
#include "FrontendAction.h"
#include "ASTConsumer.h"
#include "RecursiveASTVisitor.h"
#include "CompilerInstance.h"
#include "Rewriter.h"
#include "ASTContext.h"
#include "Expr.h"
#include "ExprCXX.h"
#include "Type.h"
#include "Decl.h"

// LLVM libs
#include "raw_ostream.h"

// STL libs
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

class VProfFrontendAction : public ASTFrontendAction {
    public:
        virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &ci, StringRef file) {
            return std::unique_ptr<ASTConsumer>(new VProfASTConsumer(&ci));
        }
};

class VProfASTConsumer : public ASTConsumer {
    private:
        std::unique_ptr<VProfVisitor> visitor;
    public:
        // Override ctor to pass hash map of fully qualified function names to the function
        // name to which the key functions should be converted to in the source.
        explicit VProfConsumer(CompilerInstance &ci, Rewriter &rewriter,
                               std::unordered_map<std::string, std::string> &functions): 
                               visitor(ci, rewriter, functions) {}


        virtual void HandleTranslationUnit(ASTContext &context) {
            visitor->TraverseDecl(context.getTranslationUnitDecl());
        }
};

class VProfVisitor : public RecursiveASTVisitor {
    private:
        // Context storing additional state
        std::unique_ptr<ASTContext> astContext;

        // Client to rewrite source
        Rewriter rewriter;

        // Hash map of fully qualified function names to the function
        // name to which the key functions should be converted to in the source.
        std::unordered_map<std::string, std::string> functions;

        // FOR DEBUGGING PURPOSES. PRINT SUPPOSED FUNCTION CALL INSTEAD OF WRITING TO FILE
        void fixFunction(const CallExpr *call, const std::string &functionName);

        void appendNonObjArgs(std::string &newCall, std::vector<Expr*> &args);

    public:
        explicit VProfVisitor(CompilerInstance &ci, Rewriter &_rewriter,
                              std::unordered_map<std::string, std::string> &_functions):
                              astContext(std::make_unique(&ci->GetASTContext())), 
                              rewriter(_rewriter), functions(_functions) {

            rewriter.setSourceMgr(astContext->getSourceManager(),
                                  astContext->getLangOpts());

        }

        // Override trigger for when a CallExpr is found in the AST
        virtual void VisitCallExpr(const CallExpr *call);

        // Override trigger for when a CXXMemberCallExpr is found in the AST
        virtual void VisitCXXMemberCallExpr(const CXXMemberCallExpr *call);
};
