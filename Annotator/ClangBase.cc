#include "ClangBase.h"

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

bool VProfVisitor::shouldCreateNewPrototype(const std::string &functionName) {
    return prototypeMap->find(functionName) == prototypeMap->end();
}

std::string VProfVisitor::getEntireParamDeclAsString(const ParmVarDecl *decl) {
    std::stringstream ss;
    decl->dump(ss);

    return ss.str();
}

void VProfVisitor::createNewPrototype(const FunctionDecl *decl, 
                                      const std::string &functionName,
                                      bool isMemberFunc) {
    FunctionPrototype newPrototype;
    newPrototype.staticCallName = "";
    newPrototype.nonStaticCallName = "";

    const std::string returnType = decl->getReturnType()->getAsString();
    newPrototype.functionPrototype += returnType + " " + functions[functionName] + "(";
    newPrototype.hasNonVoidReturn = returnType != "void";

    if (isMemberFunc) {
        const CXXMethodDecl *methodDecl = static_cast<const CXXMethodDecl*>(decl);
        if (methodDecl->isStatic()) {
            newPrototype.innerCallPrefix = methodDecl->getQualifiedNameAsString();
        }
        else {
            newPrototype.innerCallPrefix = "obj->" + methodDecl->getNameAsString();
            newPrototype.functionPrototype += methodDecl->getThisType().getAsString() + "* obj";
        }
    }
    // Is there a more succinct way to write this?
    else {
        newPrototype.innerCallPrefix = methodDecl->getNameAsString();
    }

    for (unsigned int i = 0, j = decl->getNumParams(); i < j; i++) {
        if (!newPrototype.isStatic && i == 0) {
            newPrototype.functionPrototype +=", "
        }

        const ParmVarDecl* paramDecl = decl->getParamDecl(i);
        newPrototype.functionPrototype += getEntireDeclAsString(paramDecl);

        paramVars.push_back(paramDecl->getNameAsString() + (paramDecl->isParameterPack() ? "..." : ""));

        if (i != (j - 1)) {
            newPrototype.functionPrototype += ", ";
        }
    }

    newPrototype.functionPrototype += ")";

    prototypeMap[functionName] = newPrototype;
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

        if (shouldCreatePrototype(functionName)) {
            createNewPrototype(functionName, false);
        }
    }

    return true;
}

bool VProfVisitor::VisitCXXMemberCallExpr(const CXXMemberCallExpr *call) {
    const std::string functionName = call->getMethodDecl()->getQualifiedNameAsString();

    if (functions.find(functionName) != functions.end()) {
        fixFunction(call, functionName, true);

        if (shouldCreatePrototype(functionName)) {
            createNewPrototype(functionName, true);
        }
    }

    return true;
}

VProfVisitor::VProfVisitor(std::shared_ptr<clang::CompilerInstance> ci, 
                           std::shared_ptr<clang::Rewriter> _rewriter,
                           std::unordered_map<std::string, std::string> &_functions,
                           std::shared_ptr<std::unordered_map<std::string, FunctionPrototype>> _protoMap):
                           astContext(&ci->getASTContext()), 
                           rewriter(_rewriter), 
                           functions(_functions), 
                           prototypeMap(_protoMap) {

    rewriter->setSourceMgr(astContext->getSourceManager(),
                          astContext->getLangOpts());

}

// TODO put some exception in here if write fails
VProfASTConsumer::~VProfASTConsumer() {
    filename.insert(filename.find("."), "_vprof");

    std::error_code OutErrInfo;
    std::error_code ok;

    llvm::raw_fd_ostream outputFile(llvm::StringRef(outFilename), 
                                    OutErrInfo, llvm::sys::fs::F_None); 

    if (OutErrInfo == ok) {
        const RewriteBuffer *RewriteBuf = rewriter->getRewriteBufferFor(sourceManager->getMainFileID());

        outputFile << "// VProfiler included header\n#include \"VProfilerEventWrappers.h\"\n\n";
        outputFile << std::string(RewriteBuf->begin(), RewriteBuf->end());
    }
}

VProfVisitor::~VProfVisitor() {}
