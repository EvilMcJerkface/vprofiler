#include "TracerInstrumentorVisitor.h"
#include "Utils.h"

#include "clang/Lex/Lexer.h"

#include <iostream>

using namespace clang;

void TracerInstrumentorVisitor::fixFunction(const CallExpr *call, const std::string &functionName,
                               bool isMemberCall) {
    // Get args
    std::string newCall = functionName + "_vprofiler(";
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
    newPrototype.functionPrototype += newPrototype.returnType + " " + functionName + "_vprofiler(";

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

    if (shouldCreateNewPrototype(functionName)) {
        createNewPrototype(decl, functionName, false);
    }

    return true;
}

bool TracerInstrumentorVisitor::VisitCXXMemberCallExpr(const CXXMemberCallExpr *call) {
    if (!inTargetFunction(call)) {
        return true;
    }
    if (call->getMethodDecl() == nullptr) {
        return true;
    }
    const std::string functionName = call->getMethodDecl()->getQualifiedNameAsString();
    fixFunction(call, functionName, true);

    if (shouldCreateNewPrototype(functionName)) {
        createNewPrototype(call->getMethodDecl(), functionName, true);
    }

    return true;
}

bool TracerInstrumentorVisitor::VisitStmt(const Stmt *s) {
    if (!inTargetFunction(s)) {
        return true;
    }
    if (isa<IfStmt>(s)) {
        // Cast s to IfStmt to access the then and else clauses
        const IfStmt *If = cast<IfStmt>(s);
        saveCondExpr(If->getCond());
        const Stmt *TH = If->getThen();

        // Add braces if needed to then clause
        AddBraces(TH);

        const Stmt *EL = If->getElse();
        if (EL) {
            // Add braces if needed to else clause
            AddBraces(EL);
        }
    }
    else if (isa<WhileStmt>(s)) {
        const WhileStmt *While = cast<WhileStmt>(s);
        saveCondExpr(While->getCond());
        const Stmt *BODY = While->getBody();
        AddBraces(BODY);
    } else if (isa<ForStmt>(s)) {
        const ForStmt *For = cast<ForStmt>(s);
        saveCondExpr(For->getCond());
        const Stmt *BODY = For->getBody();
        AddBraces(BODY);
    }

    return true;
}

void TracerInstrumentorVisitor::AddBraces(const Stmt *s) {
    // Only perform if statement is not compound
    if (!isa<CompoundStmt>(s))
    {
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

        SourceLocation end = s->getLocEnd();

        // MeasureTokenLength gets us past the last token, and adding 1 gets
        // us past the ';'.
        int offset = Lexer::MeasureTokenLength(end,
                                                rewriter->getSourceMgr(),
                                                rewriter->getLangOpts()) + 1;

        SourceLocation end1 = end.getLocWithOffset(offset);
        rewriter->InsertText(end1, "\n}", true, true);
    }

    // Also note getLocEnd() on a CompoundStmt points ahead of the '}'.
    // Use getLocEnd().getLocWithOffset(1) to point past it.
}

bool TracerInstrumentorVisitor::VisitReturnStmt(const clang::ReturnStmt *stmt) {
    return true;
}

bool TracerInstrumentorVisitor::VisitFunctionDecl(const FunctionDecl *decl) {
    if (decl->getNameInfo().getName().getAsString() != targetFunctionName) {
        return true;
    }

    if (!decl->hasBody()) {
        return true;
    }

    targetFunctionRange = decl->getSourceRange();

    return true;
}

TracerInstrumentorVisitor::TracerInstrumentorVisitor(CompilerInstance &ci,
                            std::shared_ptr<Rewriter> _rewriter,
                            std::string _targetFunctionName):
                            astContext(&ci.getASTContext()), 
                            rewriter(_rewriter),
                            targetFunctionName(SplitString(_targetFunctionName, '|')),
                            instruDoneForCond(false) {
    rewriter->setSourceMgr(astContext->getSourceManager(),
                          astContext->getLangOpts());
}

TracerInstrumentorVisitor::~TracerInstrumentorVisitor() {}
