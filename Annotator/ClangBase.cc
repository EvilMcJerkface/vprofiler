#include "ClangBase.h"

void VProfVisitor::fixFunction(const CallExpr *call, const std::string &functionName) {
    // Get args
    std::string newCall = functions[functionName] + "(";
    std::vector<Expr*> args;

    if (CXXMemberCallExpr *memCall = dynamic_cast<CXXMemberCallExpr*>(call)) {
        Expr *obj = memCall->getImplicitObjectArgument();
        args.append(obj);
        
        // Add ampersand to get address of obj if it is not a pointer.  This ensures
        // the first arg of a member function that the user wants profiled is a 
        // pointer to the instance of the object on which to operate.
        if (!obj->getType()->isPointerType()) {
            newCall += "&";
        }

    }

    for (int i = 0, j = call->getNumArgs(); i < j; i++) {
        args.append(call->getArg(i));
    }

    appendNonObjArgs(call, newCall);
    newCall += ");";

    std::cout << newCall;
}

void VProfVisitor::appendNonObjArgs(std::string &newCall, std::vector<Expr*> &args) {
    for (Expr *arg : args) {
        std::string TypeS;
        llvm::raw_string_ostream s(TypeS);
        newCall += arg->printPretty(s, 0, rewriter->getLangOpts());

        if (i != (j - 1)) {
            newCall += ", ";
        }
    }
}

void VProfVisitor::VisitCallExpr(const CallExpr *call) {
    // Exit if call is to a function pointer
    if (!(const FunctionDecl *decl = call->getDirectCallee())) {
        return;
    }
    const std::string functionName = decl->getQualifiedNameAsString();

    // If fname is in list of functions to replace
    if (functions.find(functionName) != functions.end()) {
        fixFunction(call, functionName);
    }

    return;
}

void VProfVisitor::VisitCXXMemberCallExpr(const CXXMemberCallExpr *call) {
    const std::string functionName = call->getMethodDecl()->getQualifiedNameAsString();

    if (functions.find(functionName) != functions.end()) {
        fixFunction(call, functionName);
    }
}
