#include "TestVisitor.h"

#include "clang/Lex/Lexer.h"
#include "clang/AST/Stmt.h"

#include <iostream>

using namespace clang;

bool TestVisitor::VisitCallExpr(const CallExpr *call) {
    const FunctionDecl *decl = call->getDirectCallee();
    // Exit if call is to a function pointer
    if (!decl || isa<CXXMemberCallExpr>(call)) {
        return true;
    }
    const std::string functionName = decl->getQualifiedNameAsString();
    llvm::outs() << "Visiting function call " << functionName << "()\n";

    unsigned start = call->getLocStart().getRawEncoding();
    unsigned end = call->getLocEnd().getRawEncoding();
    if (condRange.getBegin().getRawEncoding() <= start &&
        end <= condRange.getEnd().getRawEncoding()) {
        llvm::outs() << "Function call in cond range\n";
    }

    return true;
}

bool TestVisitor::VisitCXXMemberCallExpr(const CXXMemberCallExpr *call) {
    if (call->getMethodDecl() == nullptr) {
        return true;
    }
    const std::string functionName = call->getMethodDecl()->getQualifiedNameAsString();
    llvm::outs() << "Visiting function call " << functionName << "()\n";

    unsigned start = call->getLocStart().getRawEncoding();
    unsigned end = call->getLocEnd().getRawEncoding();
    if (condRange.getBegin().getRawEncoding() <= start &&
        end <= condRange.getEnd().getRawEncoding()) {
        llvm::outs() << "Function call in cond range\n";
    }

    return true;
}

bool TestVisitor::VisitStmt(const Stmt *s) {
    if (isa<IfStmt>(s)) {
        llvm::outs() << "Visiting if statement\n";
        // Cast s to IfStmt to access the then and else clauses
        const IfStmt *If = cast<IfStmt>(s);
        condExpr = If->getCond();
        condRange = condExpr->getSourceRange();
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
        llvm::outs() << "Visiting while statement\n";
        const WhileStmt *While = cast<WhileStmt>(s);
        condExpr = While->getCond();
        condRange = condExpr->getSourceRange();
        const Stmt *BODY = While->getBody();
        AddBraces(BODY);
    } else if (isa<ForStmt>(s)) {
        llvm::outs() << "Visiting for statement\n";
        const ForStmt *For = cast<ForStmt>(s);
        condExpr = For->getCond();
        condRange = condExpr->getSourceRange();
        const Stmt *BODY = For->getBody();
        AddBraces(BODY);
    }

    return true;
}

void TestVisitor::AddBraces(const Stmt *s) {
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

bool TestVisitor::VisitFunctionDecl(const FunctionDecl *decl) {
    std::string functionName = decl->getNameInfo().getName().getAsString();

    if (!decl->hasBody()) {
        llvm::outs() << "Visiting function " << functionName << " declaration\n";
        return true;
    }
        llvm::outs() << "Visiting function " << functionName << " definition\n";

    return true;
}

TestVisitor::TestVisitor(CompilerInstance &ci,
                            std::shared_ptr<Rewriter> _rewriter):
                            astContext(&ci.getASTContext()), 
                            rewriter(_rewriter) {
    rewriter->setSourceMgr(astContext->getSourceManager(),
                          astContext->getLangOpts());
}

TestVisitor::~TestVisitor() {}

std::unique_ptr<TestFrontendActionFactory> CreateTestFrontendActionFactory() {
    return std::unique_ptr<TestFrontendActionFactory>(new TestFrontendActionFactory());
}
