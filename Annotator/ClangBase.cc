#include "ClangBase.h"

// REMOVE THIS INCLUDE AFTER TESTING
#include <iostream>

using namespace clang;

void VProfVisitor::fixFunction(const CallExpr *call, const std::string &functionName,
                               bool isMemberCall) {
    // Get args
    std::string newCall = functions[functionName] + "(";
    std::vector<const Expr*> args;

    if (isMemberCall) {
        const CXXMemberCallExpr *memCall = static_cast<const CXXMemberCallExpr*>(call);
        Expr *obj = memCall->getImplicitObjectArgument();
        args.push_back(obj);
        
        // Add ampersand to get address of obj if it is not a pointer.  This ensures
        // the first arg of a member function that the user wants profiled is a 
        // pointer to the instance of the object on which to operate.
        if (!obj->getType()->isPointerType()) {
            newCall += "&";
        }

    }

    for (int i = 0, j = call->getNumArgs(); i < j; i++) {
        args.push_back(call->getArg(i));
    }

    appendNonObjArgs(newCall, args);
    newCall += ")";

    rewriter->ReplaceText(SourceRange(call->getLocStart(), 
                          call->getRParenLoc()), newCall);
}

void VProfVisitor::appendNonObjArgs(std::string &newCall, std::vector<const Expr*> &args) {
    for (int i = 0, j = args.size(); i < j; i++) {
        std::string TypeS;
        llvm::raw_string_ostream s(TypeS);
        args[i]->printPretty(s, 0, rewriter->getLangOpts());
        newCall += s.str();

        if (i != (j - 1)) {
            newCall += ", ";
        }
    }
}

bool VProfVisitor::VisitCallExpr(const CallExpr *call) {
    const FunctionDecl *decl = call->getDirectCallee();
    // Exit if call is to a function pointer
    if (!decl || isa<clang::CXXMemberCallExpr>(call)) {
        return true;
    }
    const std::string functionName = decl->getQualifiedNameAsString();

    // If fname is in list of functions to replace
    if (functions.find(functionName) != functions.end()) {
        fixFunction(call, functionName, false);
    }

    return true;
}

bool VProfVisitor::VisitCXXMemberCallExpr(const CXXMemberCallExpr *call) {
    const std::string functionName = call->getMethodDecl()->getQualifiedNameAsString();

    if (functions.find(functionName) != functions.end()) {
        fixFunction(call, functionName, true);
    }

    return true;
}

VProfVisitor::VProfVisitor(std::shared_ptr<clang::CompilerInstance> ci, 
                           std::shared_ptr<clang::Rewriter> _rewriter,
                           std::unordered_map<std::string, std::string> &_functions):
                           astContext(std::unique_ptr<clang::ASTContext>(&ci->getASTContext())), 
                           rewriter(_rewriter), functions(_functions) {

    rewriter->setSourceMgr(astContext->getSourceManager(),
                          astContext->getLangOpts());

}

VProfVisitor::~VProfVisitor() {}
