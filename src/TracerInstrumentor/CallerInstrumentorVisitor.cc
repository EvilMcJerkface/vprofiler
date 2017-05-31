#include "CallerInstrumentorVisitor.h"
#include "Utils.h"

#include "clang/Lex/Lexer.h"

#include <iostream>

using namespace clang;

bool CallerInstrumentorVisitor::inRange(clang::SourceRange largeRange, clang::SourceRange smallRange) {
    unsigned start = smallRange.getBegin().getRawEncoding();
    unsigned end = smallRange.getEnd().getRawEncoding();
    return largeRange.getBegin().getRawEncoding() <= start &&
           end <= largeRange.getEnd().getRawEncoding();
}

bool CallerInstrumentorVisitor::inCaller(const Expr *call) {
    clang::SourceRange range(call->getLocStart(), call->getLocEnd());
    return inRange(callerRange, range);
}

bool CallerInstrumentorVisitor::inCaller(const Stmt *stmt) {
    return inRange(callerRange, stmt->getSourceRange());
}

std::vector<std::string> CallerInstrumentorVisitor::getFunctionNameAndArgs(
    const clang::FunctionDecl *decl) {
    std::vector<std::string> nameAndArgs;

    nameAndArgs.push_back(decl->getQualifiedNameAsString());

    for (unsigned int i = 0, j = decl->getNumParams(); i < j; i++) {
        const ParmVarDecl* paramDecl = decl->getParamDecl(i);
        nameAndArgs.push_back(paramDecl->getNameAsString());
    }
    return nameAndArgs;
}

bool CallerInstrumentorVisitor::isTargetFunction(const clang::FunctionDecl *decl) {
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

bool CallerInstrumentorVisitor::isCaller(const clang::FunctionDecl *decl) {
    std::vector<std::string> nameAndArgs = getFunctionNameAndArgs(decl);
    if (nameAndArgs.size() != callerNameAndArgs.size()) {
        return false;
    }
    for (size_t i = 0; i < nameAndArgs.size(); i++) {
        if (nameAndArgs[i] != callerNameAndArgs[i]) {
            return false;
        }
    }
    return true;
}

void CallerInstrumentorVisitor::insertPathCountUpdates(const clang::CallExpr *call) {
    std::string var_set = "TARGET_PATH_SET(" + std::to_string(targetPathCount) + ");\n\t";
    std::string pathCountIncrement = "PATH_INC(" + std::to_string(targetPathCount - 1) + ");\n\t";
    rewriter->InsertText(call->getLocStart(), var_set + pathCountIncrement, true);

    std::string pathCountDecrement = "\n\tPATH_DEC(" + std::to_string(targetPathCount - 1) + ");\n";
    int offset = Lexer::MeasureTokenLength(call->getLocEnd(),
                                            rewriter->getSourceMgr(),
                                            rewriter->getLangOpts()) + 1;

    SourceLocation realEnd = call->getLocEnd().getLocWithOffset(offset);
    rewriter->InsertText(realEnd, pathCountDecrement, true);
}

bool CallerInstrumentorVisitor::VisitCallExpr(const CallExpr *call) {
    if (!inCaller(call)) {
        return true;
    }
    const FunctionDecl *decl = call->getDirectCallee();
    // Exit if call is to a function pointer
    if (!decl || isa<CXXMemberCallExpr>(call)) {
        return true;
    }

    if (functionIndex == targetInCallerIndex) {
        insertPathCountUpdates(call);
    }

    functionIndex++;

    return true;
}

bool CallerInstrumentorVisitor::VisitCXXMemberCallExpr(const clang::CXXMemberCallExpr *call) {
    if (!inCaller(call)) {
        return true;
    }
    if (call->getMethodDecl() == nullptr) {
        return true;
    }

    if (functionIndex == targetInCallerIndex) {
        insertPathCountUpdates(call);
    }

    functionIndex++;

    return true;
}

void CallerInstrumentorVisitor::addBraces(const clang::Stmt *s)
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

bool CallerInstrumentorVisitor::VisitStmt(const clang::Stmt *s) {
    if (!inCaller(s)) {
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

void CallerInstrumentorVisitor::initForInstru(const clang::FunctionDecl *decl) {
    if (!decl->isThisDeclarationADefinition()) {
        return;
    }

    if (!isCaller(decl)) {
        return;
    }

    *shouldFlush = true;
    functionIndex = 1;
    callerRange = decl->getSourceRange();
}

bool CallerInstrumentorVisitor::VisitFunctionDecl(const clang::FunctionDecl *decl) {
    initForInstru(decl);
    return true;
}

CallerInstrumentorVisitor::CallerInstrumentorVisitor(CompilerInstance &ci,
                            std::shared_ptr<Rewriter> _rewriter,
                            std::string _targetFunctionName,
                            std::string _callerNameAndArgs,
                            int _targetPathCount,
                            std::shared_ptr<bool> _shouldFlush):
                            astContext(&ci.getASTContext()), 
                            rewriter(_rewriter),
                            targetFunctionNameString(_targetFunctionName),
                            targetFunctionNameAndArgs(SplitString(_targetFunctionName, '|')),
                            callerNameAndArgs(SplitString(_callerNameAndArgs, '|')),
                            targetPathCount(_targetPathCount),
                            shouldFlush(_shouldFlush) {
    rewriter->setSourceMgr(astContext->getSourceManager(),
                          astContext->getLangOpts());
    std::vector<std::string> nameParts = SplitString(targetFunctionNameAndArgs[0], '-');
    targetFunctionNameAndArgs[0] = nameParts[0];
    if (nameParts.size() > 1) {
        targetInCallerIndex = std::stoi(nameParts[1]);
    }
}

CallerInstrumentorVisitor::~CallerInstrumentorVisitor() {}
