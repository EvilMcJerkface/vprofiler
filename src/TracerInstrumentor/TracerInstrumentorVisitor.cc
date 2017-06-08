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
    wrapperName += nameParts.back() + std::to_string(std::get<2>(*tracerHeaderInfo)) + "_vprofiler";
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

// TODO change name to getParamDeclAsString
std::string TracerInstrumentorVisitor::getEntireParamDeclAsString(const ParmVarDecl *decl) {
    return decl->getType().getAsString() + " " + decl->getNameAsString();
}

std::string TracerInstrumentorVisitor::getFileAndLine(const clang::FunctionDecl *decl) {
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

void TracerInstrumentorVisitor::createNewPrototype(const FunctionDecl *decl, 
                                      const std::string &functionName,
                                      bool isMemberFunc) {
    FunctionPrototype newPrototype;

    std::string functionNameInFile = decl->getQualifiedNameAsString() + '-' + std::to_string(std::get<2>(*tracerHeaderInfo));

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
    functionNamesStream << functionNameInFile << std::endl;
    functionNamesStream.flush();
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

std::string TracerInstrumentorVisitor::generateWrapperImpl(FunctionPrototype prototype) {
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

    implementation += "TRACE_END(" + std::to_string(std::get<2>(*tracerHeaderInfo)) + ");\n";

    if (prototype.returnType != "void") {
        implementation += "\treturn result;\n";
    }

    implementation += "}\n\n";

    return implementation;
}

std::vector<std::string> TracerInstrumentorVisitor::getFunctionNameAndArgs(
    const clang::FunctionDecl *decl) {
    std::vector<std::string> nameAndArgs;

    nameAndArgs.push_back(decl->getQualifiedNameAsString());

    for (unsigned int i = 0, j = decl->getNumParams(); i < j; i++) {
        const ParmVarDecl* paramDecl = decl->getParamDecl(i);
        nameAndArgs.push_back(paramDecl->getNameAsString());
    }
    return nameAndArgs;
}

bool TracerInstrumentorVisitor::isTargetFunction(const clang::FunctionDecl *decl) {
    std::vector<std::string> nameAndArgs = getFunctionNameAndArgs(decl);
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
    if (functionName == "SESSION_START" ||
        functionName == "SESSION_END" ||
        functionName == "PATH_INC" ||
        functionName == "PATH_DEC") {
            return true;
    }
    fixFunction(call, functionName, false);

    createNewPrototype(decl, functionName, false);

    std::get<2>(*tracerHeaderInfo)++;

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

    std::get<2>(*tracerHeaderInfo)++;

    return true;
}

void TracerInstrumentorVisitor::addBraces(const clang::Stmt *s)
{
    // Only perform if statement is not compound
    if (!isa<CompoundStmt>(s)) {
        SourceLocation ST = s->getLocStart();

        // Insert opening brace.  Note the second true parameter to InsertText()
        // says to indent.  Sadly, it will indent to the line after the if, giving:
        // if (expr)
        //   {
        //   stmt;
        //   }
        rewriter->InsertText(ST, "{\n", true, true);

        // Note Stmt::getLocEnd() returns the source location prior to the
        // token at the end of the line.  For instance, for:
        // var = 123;
        //      ^---- getLocEnd() points here.

        SourceLocation END = s->getLocEnd();

        // MeasureTokenLength gets us past the last token, and adding 1 gets
        // us past the ';'.
        int offset = Lexer::MeasureTokenLength(END,
                                                rewriter->getSourceMgr(),
                                                rewriter->getLangOpts()) + 1;

        SourceLocation END1 = END.getLocWithOffset(offset);
        rewriter->InsertText(END1, "\n}", true, true);
    }

    // Also note getLocEnd() on a CompoundStmt points ahead of the '}'.
    // Use getLocEnd().getLocWithOffset(1) to point past it.
}

bool TracerInstrumentorVisitor::VisitStmt(const clang::Stmt *s) {
    if (!inTargetFunction(s)) {
        return true;
    }
    if (isa<IfStmt>(s)) {
        // Cast s to IfStmt to access the then and else clauses
        const IfStmt *If = cast<IfStmt>(s);
        const Stmt *TH = If->getThen();

        // Add braces if needed to then clause
        addBraces(TH);

        const Stmt *EL = If->getElse();
        if (EL) {
            // Add braces if needed to else clause
            addBraces(EL);
        }
    } else if (isa<WhileStmt>(s)) {
        const WhileStmt *While = cast<WhileStmt>(s);
        const Stmt *BODY = While->getBody();
        addBraces(BODY);
    } else if (isa<ForStmt>(s)) {
        const ForStmt *For = cast<ForStmt>(s);
        const Stmt *BODY = For->getBody();
        addBraces(BODY);
    }

    return true;
}

void TracerInstrumentorVisitor::initForInstru(const clang::FunctionDecl *decl) {
    if (!decl->isThisDeclarationADefinition()) {
        return;
    }

    if (!isTargetFunction(decl)) {
        return;
    }

    *shouldFlush = true;
    std::get<2>(*tracerHeaderInfo) = 1;
    functionNamesStream.open(functionNamesFile);
    functionNamesStream << targetFunctionNameString << std::endl;
    functionNamesStream.flush();
    targetFunctionRange = decl->getSourceRange();
    wrapperImplLoc->first = targetFunctionRange.getBegin();

    std::string returnType = decl->getReturnType().getAsString();
    std::get<0>(*tracerHeaderInfo) = decl->getBody()->getLocStart().getLocWithOffset(1);
    std::get<1>(*tracerHeaderInfo) = returnType;

    if (returnType == "void") {
        clang::SourceLocation functionEnd = decl->getBody()->getLocEnd();
        rewriter->InsertText(functionEnd, "\tTRACE_FUNCTION_END();\n", true);
    }
}

bool TracerInstrumentorVisitor::VisitFunctionDecl(const clang::FunctionDecl *decl) {
    initForInstru(decl);
    return true;
}

// bool TracerInstrumentorVisitor::VisitCXXMethodDecl(const clang::CXXMethodDecl *decl) {
//     initForInstru(decl);
//     return true;
// }

TracerInstrumentorVisitor::TracerInstrumentorVisitor(CompilerInstance &ci,
                            std::shared_ptr<Rewriter> _rewriter,
                            std::string _targetFunctionName,
                            std::shared_ptr<bool> _shouldFlush,
                            std::shared_ptr<std::pair<clang::SourceLocation, std::string>> _wrapperImplLoc,
                            std::shared_ptr<std::tuple<clang::SourceLocation, std::string, int>> _tracerHeaderInfo,
                            std::string _functionNamesFile):
                            astContext(&ci.getASTContext()), 
                            rewriter(_rewriter),
                            targetFunctionNameString(_targetFunctionName),
                            targetFunctionNameAndArgs(SplitString(_targetFunctionName, '|')),
                            shouldFlush(_shouldFlush),
                            wrapperImplLoc(_wrapperImplLoc),
                            tracerHeaderInfo(_tracerHeaderInfo),
                            functionNamesFile(_functionNamesFile) {
    rewriter->setSourceMgr(astContext->getSourceManager(),
                          astContext->getLangOpts());
    targetFunctionNameAndArgs[0] = SplitString(targetFunctionNameAndArgs[0], '-')[0];
}

TracerInstrumentorVisitor::~TracerInstrumentorVisitor() {}
