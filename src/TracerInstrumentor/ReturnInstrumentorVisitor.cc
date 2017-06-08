#include "ReturnInstrumentorVisitor.h"
#include "Utils.h"

#include "clang/Lex/Lexer.h"

#include <iostream>

using namespace clang;

bool ReturnInstrumentorVisitor::inRange(clang::SourceRange largeRange, clang::SourceRange smallRange) {
    unsigned start = smallRange.getBegin().getRawEncoding();
    unsigned end = smallRange.getEnd().getRawEncoding();
    return largeRange.getBegin().getRawEncoding() <= start &&
           end <= largeRange.getEnd().getRawEncoding();
}

bool ReturnInstrumentorVisitor::inTargetFunction(const Stmt *stmt) {
    return inRange(targetFunctionRange, stmt->getSourceRange());
}

std::vector<std::string> ReturnInstrumentorVisitor::getFunctionNameAndArgs(
    const clang::FunctionDecl *decl) {
    std::vector<std::string> nameAndArgs;

    nameAndArgs.push_back(decl->getQualifiedNameAsString());

    for (unsigned int i = 0, j = decl->getNumParams(); i < j; i++) {
        const ParmVarDecl* paramDecl = decl->getParamDecl(i);
        nameAndArgs.push_back(paramDecl->getNameAsString());
    }
    return nameAndArgs;
}

bool ReturnInstrumentorVisitor::isTargetFunction(const clang::FunctionDecl *decl) {
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

std::string ReturnInstrumentorVisitor::exprToString(const clang::Expr *expr) {
    clang::SourceLocation start = expr->getLocStart();
    clang::SourceLocation end = expr->getLocEnd();
    clang::SourceManager &manager = rewriter->getSourceMgr();
    clang::SourceLocation realEnd(
        clang::Lexer::getLocForEndOfToken(end, 0, manager, rewriter->getLangOpts()));
    return std::string(manager.getCharacterData(start),
        manager.getCharacterData(realEnd) - manager.getCharacterData(start));
}

bool ReturnInstrumentorVisitor::VisitReturnStmt(const clang::ReturnStmt *stmt) {
    if (!inTargetFunction(stmt)) {
        return true;
    }
    *shouldFlush = true;
    const clang::Expr *returnValue = stmt->getRetValue();
    if (returnValue == nullptr) {
        rewriter->InsertText(stmt->getLocStart(), "TRACE_FUNCTION_END();\n\t");
        return true;
    }

    std::string endInstru = "resVprof = " + exprToString(returnValue) + ";\n";
    endInstru += "\tTRACE_FUNCTION_END();\n";
    endInstru += "\treturn resVprof";
    rewriter->ReplaceText(stmt->getSourceRange(), endInstru);

    return true;
}

void ReturnInstrumentorVisitor::initForInstru(const clang::FunctionDecl *decl) {
    if (!decl->isThisDeclarationADefinition()) {
        return;
    }

    if (!isTargetFunction(decl)) {
        return;
    }

    targetFunctionRange = decl->getSourceRange();
}

bool ReturnInstrumentorVisitor::VisitFunctionDecl(const clang::FunctionDecl *decl) {
    initForInstru(decl);
    return true;
}

ReturnInstrumentorVisitor::ReturnInstrumentorVisitor(CompilerInstance &ci,
                            std::shared_ptr<Rewriter> _rewriter,
                            std::string _targetFunctionName,
                            std::shared_ptr<bool> _shouldFlush):
                            astContext(&ci.getASTContext()), 
                            rewriter(_rewriter),
                            targetFunctionNameString(_targetFunctionName),
                            targetFunctionNameAndArgs(SplitString(_targetFunctionName, '|')),
                            shouldFlush(_shouldFlush) {
    rewriter->setSourceMgr(astContext->getSourceManager(),
                          astContext->getLangOpts());
    targetFunctionNameAndArgs[0] = SplitString(targetFunctionNameAndArgs[0], '-')[0];
}

ReturnInstrumentorVisitor::~ReturnInstrumentorVisitor() {}
