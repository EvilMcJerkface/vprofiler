#include "NonTargetTracerInstrumentorVisitor.h"
#include "Utils.h"

#include "clang/Lex/Lexer.h"

#include <iostream>

using namespace clang;

std::string NonTargetTracerInstrumentorVisitor::functionNameToWrapperName(std::string functionName) {
    std::vector<std::string> nameParts = SplitString(functionName, ':');
    std::string wrapperName;
    for (size_t i = 0; i < nameParts.size() - 1; i++) {
        wrapperName += nameParts[i] + '_';
    }
    wrapperName += nameParts.back() + std::to_string((*numFuncs)) + "_vprofiler";
    return wrapperName;
}

std::string NonTargetTracerInstrumentorVisitor::getRootFuncName(int rootIndex) {
    std::vector<std::string> funcNameAndArgs = rootFunctionNamesAndArgs[rootIndex];
    std::string funcName = funcNameAndArgs[0];
    for (size_t i = 1; i < funcNameAndArgs.size(); ++i) {
        funcName += "|" + funcNameAndArgs[i];
    }
    return funcName;
}

void NonTargetTracerInstrumentorVisitor::fixFunction(const CallExpr *call, const std::string &functionName,
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

void NonTargetTracerInstrumentorVisitor::appendNonObjArgs(std::string &newCall, std::vector<const Expr*> &args) {
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

std::string NonTargetTracerInstrumentorVisitor::getContainingFilename(const FunctionDecl *decl) {
    SourceManager *sourceMgr = &rewriter->getSourceMgr();

    return sourceMgr->getFilename(decl->getLocation()).str();
}

// TODO change name to getParamDeclAsString
std::string NonTargetTracerInstrumentorVisitor::getEntireParamDeclAsString(const ParmVarDecl *decl) {
    return decl->getType().getAsString() + " " + decl->getNameAsString();
}

std::string NonTargetTracerInstrumentorVisitor::getFileAndLine(const clang::FunctionDecl *decl) {
    clang::SourceManager &sourceManager = rewriter->getSourceMgr();
    std::string fileName;
    if (decl->getDefinition()) {
        clang::SourceLocation start = decl->getDefinition()->getSourceRange().getBegin();
        clang::FullSourceLoc fullLoc(start, sourceManager);
        fileName = sourceManager.getFilename(fullLoc);
    } else {
        fileName = "not_found";
    }
    // TODO Fix line number.
    // const unsigned int lineNum = fullLoc.getSpellingLineNumber();
    return fileName;
}

void NonTargetTracerInstrumentorVisitor::createNewPrototype(const FunctionDecl *decl, 
                                      const std::string &functionName,
                                      bool isMemberFunc) {
    FunctionPrototype newPrototype;

    std::string functionNameInFile = decl->getQualifiedNameAsString() + '-' + std::to_string((*numFuncs));

    newPrototype.filename = getContainingFilename(decl);

    newPrototype.returnType = decl->getReturnType().getAsString();
    newPrototype.functionPrototype += newPrototype.returnType + " " + functionNameToWrapperName(functionName) + "(";

    bool isCXXMethodAndNotStatic = false;
    if (isMemberFunc) {
        const CXXMethodDecl *methodDecl = static_cast<const CXXMethodDecl*>(decl);
        if (methodDecl->isStatic()) {
            newPrototype.innerCallPrefix = decl->getQualifiedNameAsString();
        }
        else {
            isCXXMethodAndNotStatic = true;

            newPrototype.innerCallPrefix = "obj->" + methodDecl->getNameAsString();
            newPrototype.functionPrototype += methodDecl->getThisType(*astContext).getAsString() + " obj";
        }
    }
    // Is there a more succinct way to write this?
    else {
        newPrototype.innerCallPrefix = decl->getQualifiedNameAsString();
    }

    for (unsigned int i = 0, j = decl->getNumParams(); i < j; i++) {
        if (isCXXMethodAndNotStatic && i == 0) {
            newPrototype.functionPrototype +=", ";
        }

        const ParmVarDecl* paramDecl = decl->getParamDecl(i);
        newPrototype.functionPrototype += getEntireParamDeclAsString(paramDecl);

        functionNameInFile += "|" + paramDecl->getNameAsString();
        newPrototype.paramVars.push_back(paramDecl->getNameAsString() + (paramDecl->isParameterPack() ? "..." : ""));

        if (i != (j - 1)) {
            newPrototype.functionPrototype += ", ";
        }
    }

    newPrototype.functionPrototype += ")";
    newPrototype.isMemberCall = isMemberFunc;

    wrapperImplLoc->second += generateWrapperImpl(newPrototype);
    functionNamesStream << currentRootFunc << '_' << functionNameInFile << std::endl;
    functionNamesStream.flush();
}

bool NonTargetTracerInstrumentorVisitor::inRange(clang::SourceRange largeRange, clang::SourceRange smallRange) {
    unsigned start = smallRange.getBegin().getRawEncoding();
    unsigned end = smallRange.getEnd().getRawEncoding();
    return largeRange.getBegin().getRawEncoding() <= start &&
           end <= largeRange.getEnd().getRawEncoding();
}

bool NonTargetTracerInstrumentorVisitor::inRootFunction(const Expr *call) {
    clang::SourceRange range(call->getLocStart(), call->getLocEnd());
    return inRange(currentRootRange, range);
}

bool NonTargetTracerInstrumentorVisitor::inRootFunction(const Stmt *stmt) {
    return inRange(currentRootRange, stmt->getSourceRange());
}

std::string NonTargetTracerInstrumentorVisitor::generateWrapperImpl(FunctionPrototype prototype) {
    std::string implementation;
    implementation += prototype.functionPrototype + " {\n\t";
    if (prototype.returnType != "void") {
        implementation += prototype.returnType + " result;\n\n\t";
    }

    implementation += "TRACE_START();\n\t";
    if (prototype.returnType != "void") {
        implementation += "result = ";
    }

    implementation += prototype.innerCallPrefix + "(";

    for (int i = 0, j = prototype.paramVars.size(); i < j; i++) {
        implementation += prototype.paramVars[i];

        if (i != (j - 1)) {
            implementation += ", ";
        }
    }

    implementation += ");\n\t";

    implementation += "TRACE_END(" + std::to_string((*numFuncs)) + ");\n";

    if (prototype.returnType != "void") {
        implementation += "\treturn result;\n";
    }

    implementation += "}\n\n";

    return implementation;
}

std::vector<std::string> NonTargetTracerInstrumentorVisitor::getFunctionNameAndArgs(
    const clang::FunctionDecl *decl) {
    std::vector<std::string> nameAndArgs;

    nameAndArgs.push_back(decl->getQualifiedNameAsString());

    for (unsigned int i = 0, j = decl->getNumParams(); i < j; i++) {
        const ParmVarDecl* paramDecl = decl->getParamDecl(i);
        nameAndArgs.push_back(paramDecl->getType().getAsString());
    }
    return nameAndArgs;
}

int NonTargetTracerInstrumentorVisitor::isRootFunction(const clang::FunctionDecl *decl) {
    std::vector<std::string> nameAndArgs = getFunctionNameAndArgs(decl);
    for (size_t i = 0; i < rootFunctionNamesAndArgs.size(); ++i) {
        std::vector<std::string> &rootFuncNameAndArgs = rootFunctionNamesAndArgs[i];
        if (nameAndArgs.size() != rootFuncNameAndArgs.size()) {
            continue;
        }
        bool allTheSame = true;
        for (size_t i = 0; i < nameAndArgs.size(); i++) {
            if (nameAndArgs[i] != rootFuncNameAndArgs[i]) {
                allTheSame = false;
                break;
            }
        }
        if (allTheSame) {
            return i;
        }
    }
    return -1;
}

bool NonTargetTracerInstrumentorVisitor::VisitCallExpr(const CallExpr *call) {
    if (!inRootFunction(call)) {
        return true;
    }
    const FunctionDecl *decl = call->getDirectCallee();
    // Exit if call is to a function pointer
    if (!decl || isa<CXXMemberCallExpr>(call)) {
        return true;
    }
    const std::string functionName = decl->getQualifiedNameAsString();
    if (functionName == "SESSION_START" ||
        functionName == "SESSION_END" ||
        functionName == "PATH_INC" ||
        functionName == "PATH_DEC") {
            return true;
    }
    fixFunction(call, functionName, false);

    createNewPrototype(decl, functionName, false);

    (*numFuncs)++;

    return true;
}

bool NonTargetTracerInstrumentorVisitor::VisitCXXMemberCallExpr(const clang::CXXMemberCallExpr *call) {
    if (!inRootFunction(call)) {
        return true;
    }
    if (call->getMethodDecl() == nullptr) {
        return true;
    }
    const std::string functionName = call->getMethodDecl()->getQualifiedNameAsString();
    fixFunction(call, functionName, true);

    createNewPrototype(call->getMethodDecl(), functionName, true);

    (*numFuncs)++;

    return true;
}

void NonTargetTracerInstrumentorVisitor::initForInstru(const clang::FunctionDecl *decl) {
    if (!decl->isThisDeclarationADefinition()) {
        return;
    }

    int rootFuncIndex = isRootFunction(decl);
    if (rootFuncIndex == -1 ||
        (*funcDone)[rootFuncIndex]) {
        return;
    }

    *shouldFlush = true;
    (*funcDone)[rootFuncIndex] = true;
    currentRootFunc = getRootFuncName(rootFuncIndex);
    if (!functionNamesStream.is_open()) {
        functionNamesStream.open(functionNamesFile, std::fstream::app);
    }
    currentRootRange = decl->getSourceRange();

    if (wrapperImplLoc->first.getRawEncoding() == 0 ||
        wrapperImplLoc->first.getRawEncoding() > currentRootRange.getBegin().getRawEncoding()) {
        wrapperImplLoc->first = currentRootRange.getBegin();
    }

    funcStartLocs->push_back(decl->getBody()->getLocStart().getLocWithOffset(1));
}

bool NonTargetTracerInstrumentorVisitor::VisitFunctionDecl(const clang::FunctionDecl *decl) {
    initForInstru(decl);
    return true;
}

// bool NonTargetTracerInstrumentorVisitor::VisitCXXMethodDecl(const clang::CXXMethodDecl *decl) {
//     initForInstru(decl);
//     return true;
// }

NonTargetTracerInstrumentorVisitor::NonTargetTracerInstrumentorVisitor(CompilerInstance &ci,
                            std::shared_ptr<Rewriter> _rewriter,
                            std::vector<std::vector<std::string>> &_rootFunctionNamesAndArgs,
                            std::shared_ptr<std::unordered_map<int, bool>> _funcDone,
                            std::shared_ptr<bool> _shouldFlush,
                            std::shared_ptr<std::pair<clang::SourceLocation, std::string>> _wrapperImplLoc,
                            std::shared_ptr<std::vector<clang::SourceLocation>> _funcStartLocs,
                            std::shared_ptr<int> _numFuncs,
                            std::string _functionNamesFile):
                            astContext(&ci.getASTContext()), 
                            rewriter(_rewriter),
                            rootFunctionNamesAndArgs(_rootFunctionNamesAndArgs),
                            funcDone(_funcDone),
                            shouldFlush(_shouldFlush),
                            wrapperImplLoc(_wrapperImplLoc),
                            funcStartLocs(_funcStartLocs),
                            numFuncs(_numFuncs),
                            functionNamesFile(_functionNamesFile) {
    rewriter->setSourceMgr(astContext->getSourceManager(),
                           astContext->getLangOpts());
}

NonTargetTracerInstrumentorVisitor::~NonTargetTracerInstrumentorVisitor() {}
