#include <memory>
#include <unordered_map>

class VProfFrontendAction : public ASTFrontendAction {
    public:
        virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &ci, StringRef file) {
            return std::unique_ptr<ASTConsumer>(new VProfASTConsumer(&ci));
        }
}

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
}

class VProfVisitor : public RecursiveASTVisitor {
    private:
        // Context storing additional state
        std::unique_ptr<ASTContext> astContext;

        // Client to rewrite source
        Rewriter rewriter;

        // Hash map of fully qualified function names to the function
        // name to which the key functions should be converted to in the source.
        std::unordered_map<std::string, std::string> functions;

    public:
        explicit VProfVisitor(CompilerInstance &ci, Rewriter &_rewriter,
                              std::unordered_map<std::string, std::string> &_functions):
                              astContext(std::make_unique(&ci->GetASTContext())), 
                              rewriter(_rewriter), functions(_functions) {

            rewriter.setSourceMgr(astContext->getSourceManager(),
                                  astContext->getLangOpts());

        }

        virtual void VisitCallExpr(const CallExpr *call) {
            // Exit if call is to a function pointer
            if (!(const FunctionDecl *decl = call->getDirectCallee())) {
                return;
            }
            const std::string functionName = decl->getQualifiedNameAsString();

            // If fname is in list of functions to replace
            if (functions.find(functionName) != functions.end()) {
                fixFunctionNonMember(call, functionName);
            }

            return;
        }

        virtual void VisitCXXMemberCallExpr(const CXXMemberCallExpr *call) {
            const std::string functionName = call->getMethodDecl()->getQualifiedNameAsString();

            if (functions.find(functionName) != functions.end()) {
                fixFunctionMember(call, functionName);
            }
        }

        // FOR DEBUGGING PURPOSES. PRINT SUPPOSED FUNCTION CALL INSTEAD OF WRITING TO FILE
}
