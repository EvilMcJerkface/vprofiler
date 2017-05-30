#include "TracerInstrumentorVisitor.h"
#include "Utils.h"

#include "clang/Lex/Lexer.h"

#include <iostream>

using namespace clang;

std::string TracerInstrumentorVisitor::functionNameToWrapperName(std::string functionName) {
    std::vector<std::string> nameParts = SplitString(functionName, ':');
    std::string wrapperName;
    for (size_t i = 0; i < nameParts.size() - 1; i++) {
        wrapperName += nameParts[i] + '_';
    }
    wrapperName += nameParts.back() + std::to_string(functionIndex) + "_vprofiler";
    return wrapperName;
}

void TracerInstrumentorVisitor::fixFunction(const CallExpr *call, const std::string &functionName,
                               bool isMemberCall) {
    // Get args
    std::string newCall = functionNameToWrapperName(functionName) + "(";
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

void TracerInstrumentorVisitor::appendNonObjArgs(std::string &newCall, std::vector<const Expr*> &args) {
    for (unsigned int i = 0, j = args.size(); i < j; i++) {
        std::string TypeS;
        llvm::raw_string_ostream s(TypeS);
        args[i]->printPretty(s, 0, rewriter->getLangOpts());
        newCall += s.str();

        if (i != (j - 1)) {
            newCall += ", ";
        }
    }
}

std::string TracerInstrumentorVisitor::getContainingFilename(const FunctionDecl *decl) {
    SourceManager *sourceMgr = &rewriter->getSourceMgr();

    return sourceMgr->getFilename(decl->getLocation()).str();
}

bool TracerInstrumentorVisitor::shouldCreateNewPrototype(const std::string &functionName) {
    return prototypeMap.find(functionName) == prototypeMap.end();
}

// TODO change name to getParamDeclAsString
std::string TracerInstrumentorVisitor::getEntireParamDeclAsString(const ParmVarDecl *decl) {
    return decl->getType().getAsString() + " " + decl->getNameAsString();
}

void TracerInstrumentorVisitor::createNewPrototype(const FunctionDecl *decl, 
                                      const std::string &functionName,
                                      bool isMemberFunc) {
    FunctionPrototype newPrototype;

    newPrototype.filename = getContainingFilename(decl);

    newPrototype.returnType = decl->getReturnType().getAsString();
    newPrototype.functionPrototype += newPrototype.returnType + " " + functionNameToWrapperName(functionName) + "(";

    bool isCXXMethodAndNotStatic = false;
    if (isMemberFunc) {
        const CXXMethodDecl *methodDecl = static_cast<const CXXMethodDecl*>(decl);
        if (methodDecl->isStatic()) {
            newPrototype.innerCallPrefix = methodDecl->getQualifiedNameAsString();
        }
        else {
            isCXXMethodAndNotStatic = true;

            newPrototype.innerCallPrefix = "obj->" + methodDecl->getNameAsString();
            newPrototype.functionPrototype += methodDecl->getThisType(*astContext).getAsString() + " obj";
        }
    }
    // Is there a more succinct way to write this?
    else {
        newPrototype.innerCallPrefix = decl->getNameAsString();
    }

    for (unsigned int i = 0, j = decl->getNumParams(); i < j; i++) {
        if (isCXXMethodAndNotStatic && i == 0) {
            newPrototype.functionPrototype +=", ";
        }

        const ParmVarDecl* paramDecl = decl->getParamDecl(i);
        newPrototype.functionPrototype += getEntireParamDeclAsString(paramDecl);

        newPrototype.paramVars.push_back(paramDecl->getNameAsString() + (paramDecl->isParameterPack() ? "..." : ""));

        if (i != (j - 1)) {
            newPrototype.functionPrototype += ", ";
        }
    }

    newPrototype.functionPrototype += ")";
    newPrototype.isMemberCall = isMemberFunc;

    prototypeMap[functionName] = newPrototype;
}

bool TracerInstrumentorVisitor::inRange(clang::SourceRange largeRange, clang::SourceRange smallRange) {
    unsigned start = smallRange.getBegin().getRawEncoding();
    unsigned end = smallRange.getEnd().getRawEncoding();
    return largeRange.getBegin().getRawEncoding() <= start &&
           end <= largeRange.getEnd().getRawEncoding();
}

bool TracerInstrumentorVisitor::inTargetFunction(const Expr *call) {
    clang::SourceRange range(call->getLocStart(), call->getLocEnd());
    return inRange(targetFunctionRange, range);
}

bool TracerInstrumentorVisitor::inTargetFunction(const Stmt *stmt) {
    return inRange(targetFunctionRange, stmt->getSourceRange());
}

void TracerInstrumentorVisitor::saveCondExpr(const clang::Expr *cond) {
    condExpr = cond;
    condRange = condExpr->getSourceRange();
    instruDoneForCond = false;
}

bool TracerInstrumentorVisitor::inCondRange(const clang::Expr *call) {
    return inRange(condRange, call->getSourceRange());
}

std::vector<std::string> TracerInstrumentorVisitor::getFunctionNameAndArgs(
    const clang::FunctionDecl *decl, bool isMemberFunc) {
    std::vector<std::string> nameAndArgs;

    if (isMemberFunc) {
        nameAndArgs.push_back(decl->getQualifiedNameAsString());
    }
    else {
        nameAndArgs.push_back(decl->getNameAsString());
    }

    for (unsigned int i = 0, j = decl->getNumParams(); i < j; i++) {
        const ParmVarDecl* paramDecl = decl->getParamDecl(i);
        nameAndArgs.push_back(paramDecl->getNameAsString());
    }
    return nameAndArgs;
}

bool TracerInstrumentorVisitor::isTargetFunction(const clang::FunctionDecl *decl,
                                                 bool isMemberFunc) {
    std::vector<std::string> nameAndArgs = getFunctionNameAndArgs(decl, isMemberFunc);
    if (nameAndArgs.size() != targetFunctionNameAndArgs.size()) {
        return false;
    }
    for (size_t i = 0; i < nameAndArgs.size(); i++) {
        if (nameAndArgs[i] != targetFunctionNameAndArgs[i]) {
            return false;
        }
    }
    return true;
}

bool TracerInstrumentorVisitor::VisitCallExpr(const CallExpr *call) {
    if (!inTargetFunction(call)) {
        return true;
    }
    const FunctionDecl *decl = call->getDirectCallee();
    // Exit if call is to a function pointer
    if (!decl || isa<CXXMemberCallExpr>(call)) {
        return true;
    }
    const std::string functionName = decl->getQualifiedNameAsString();
    fixFunction(call, functionName, false);

    createNewPrototype(decl, functionName, false);

    functionIndex++;

    return true;
}

bool TracerInstrumentorVisitor::VisitCXXMemberCallExpr(const clang::CXXMemberCallExpr *call) {
    if (!inTargetFunction(call)) {
        return true;
    }
    if (call->getMethodDecl() == nullptr) {
        return true;
    }
    const std::string functionName = call->getMethodDecl()->getQualifiedNameAsString();
    fixFunction(call, functionName, true);

    createNewPrototype(call->getMethodDecl(), functionName, true);

    functionIndex++;

    return true;
}

bool TracerInstrumentorVisitor::VisitFunctionDecl(const clang::FunctionDecl *decl) {
    if (!decl->hasBody()) {
        return true;
    }

    if (!isTargetFunction(decl, false)) {
        return true;
    }

    *shouldFlush = true;
    functionIndex = 1;
    targetFunctionRange = decl->getSourceRange();

    return true;
}

bool TracerInstrumentorVisitor::VisitCXXMethodDecl(const clang::CXXMethodDecl *decl) {
    if (!decl->hasBody()) {
        return true;
    }

    if (!isTargetFunction(decl, true)) {
        return true;
    }

    *shouldFlush = true;
    functionIndex = 1;
    targetFunctionRange = decl->getSourceRange();

    return true;
}

TracerInstrumentorVisitor::TracerInstrumentorVisitor(CompilerInstance &ci,
                            std::shared_ptr<Rewriter> _rewriter,
                            std::string _targetFunctionName,
                            std::shared_ptr<bool> _shouldFlush):
                            astContext(&ci.getASTContext()), 
                            rewriter(_rewriter),
                            targetFunctionNameAndArgs(SplitString(_targetFunctionName, '|')),
                            shouldFlush(_shouldFlush),
                            instruDoneForCond(false) {
    rewriter->setSourceMgr(astContext->getSourceManager(),
                          astContext->getLangOpts());
}

TracerInstrumentorVisitor::~TracerInstrumentorVisitor() {}
