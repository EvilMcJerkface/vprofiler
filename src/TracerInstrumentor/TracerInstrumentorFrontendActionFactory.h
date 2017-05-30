#ifndef TRACER_INSTRUMENTOR_FRONT_END_FACTORY_H
#define TRACER_INSTRUMENTOR_FRONT_END_FACTORY_H

#include "TracerInstrumentorVisitor.h"

#include "clang/Tooling/Tooling.h"

class TracerInstrumentorASTConsumer : public clang::ASTConsumer {
    private:
        std::unique_ptr<TracerInstrumentorVisitor> visitor;

    public:
        // Override ctor to pass hash map of fully qualified function names to the function
        // name to which the key functions should be converted to in the source.
        explicit TracerInstrumentorASTConsumer(clang::CompilerInstance &ci, 
                                std::shared_ptr<clang::Rewriter> _rewriter,
                                std::string _targetFunctionName,
                                std::shared_ptr<bool> _shouldFlush,
                                std::shared_ptr<std::pair<clang::SourceLocation, std::string>> _wrapperImplLoc) {
            
            visitor = std::unique_ptr<TracerInstrumentorVisitor>(new TracerInstrumentorVisitor(ci, 
                                                                    _rewriter,
                                                                    _targetFunctionName,
                                                                    _shouldFlush,
                                                                    _wrapperImplLoc));
        }

        ~TracerInstrumentorASTConsumer() {}

        virtual void HandleTranslationUnit(clang::ASTContext &context) {
            visitor->TraverseDecl(context.getTranslationUnitDecl());
        }
};

class TracerInstrumentorFrontendAction : public clang::ASTFrontendAction {
    private:
        // Name of the transformed file.
        std::string filename;
        
        // Rewriter used by the consumer.
        std::shared_ptr<clang::Rewriter> rewriter;

        // Name of the target function.
        std::string targetFunctionName;

        clang::FileID fileID;

        std::string backupPath;

        std::shared_ptr<bool> shouldFlush;

        std::shared_ptr<std::pair<clang::SourceLocation, std::string>> wrapperImplLoc;

    public:
        TracerInstrumentorFrontendAction(std::string _targetFunctionName,
                                         std::string _backupPath) :
                                            targetFunctionName(_targetFunctionName),
                                            backupPath(_backupPath),
                                            shouldFlush(std::make_shared<bool>(false)),
                                            wrapperImplLoc(std::make_shared<std::pair<clang::SourceLocation, std::string>>(std::make_pair(clang::SourceLocation(), ""))) {}

        ~TracerInstrumentorFrontendAction() {}

        void writePathFile(const std::string &backupFilename) {
            std::string backupPathsFilename;

            if (backupPath[backupPath.length() - 1] != '/') {
                backupFileName = backupPath + "/TracerFilenames";
            } else {
                backupFileName = backupPath + "TracerFilenames";
            }

            std::error_code OutErrInfo;
            std::error_code ok;
            llvm::raw_fd_ostream outputFile(llvm::StringRef(backupFileName), 
                                            OutErrInfo, llvm::sys::fs::F_Append); 
            if (OutErrInfo == ok) {
                outputFile << backupFilename + '\t' + filename + '\n';
                outputFile.close();
            }
        }

        void backupFile() {
            size_t lastSlash = filename.rfind("/");
            std::string backupFileName;
            if (lastSlash == std::string::npos) {
                backupFileName = filename;
            } else {
                backupFileName = filename.substr(lastSlash + 1);
            }
            if (backupPath[backupPath.length() - 1] != '/') {
                backupFileName = backupPath + "/" + backupFileName;
            } else {
                backupFileName = backupPath + backupFileName;
            }

            writePathFile(backupFileName);

            std::error_code OutErrInfo;
            std::error_code ok;
            llvm::raw_fd_ostream outputFile(llvm::StringRef(backupFileName), 
                                            OutErrInfo, llvm::sys::fs::F_None); 
            if (OutErrInfo == ok) {
                clang::SourceManager &manager = rewriter->getSourceMgr();
                outputFile << manager.getBufferData(fileID);
                outputFile.close();
            }
        }

        void EndSourceFileAction() override {
            if (*shouldFlush) {
                std::error_code OutErrInfo;
                std::error_code ok;

                rewriter->InsertText(wrapperImplLoc->first, wrapperImplLoc->second, true);

                llvm::raw_fd_ostream outputFile(llvm::StringRef(filename), 
                                                OutErrInfo, llvm::sys::fs::F_None); 

                if (OutErrInfo == ok) {
                    backupFile();
                    const clang::RewriteBuffer *RewriteBuf = rewriter->getRewriteBufferFor(fileID);
                    outputFile << "// TraceTool included header\n#include \"trace_tool.h\"\n\n";
                    outputFile << std::string(RewriteBuf->begin(), RewriteBuf->end());
                    outputFile.close();
                }
            }
        }

        virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &ci,
                                                                      llvm::StringRef file) {
            rewriter = std::make_shared<clang::Rewriter>();
            rewriter->setSourceMgr(ci.getSourceManager(), ci.getLangOpts());

            filename = file.str();
            fileID = ci.getSourceManager().getMainFileID();

            return std::unique_ptr<TracerInstrumentorASTConsumer>(new TracerInstrumentorASTConsumer(ci,
                                                                                                    rewriter,
                                                                                                    targetFunctionName,
                                                                                                    shouldFlush,
                                                                                                    wrapperImplLoc));
        }
};

class TracerInstrumentorFrontendActionFactory : public clang::tooling::FrontendActionFactory {
    private:
        std::string targetFunctionName;
        std::string backupPath;

    public:
        TracerInstrumentorFrontendActionFactory(std::string _targetFunctionName,
                                                std::string _backupPath) :
                                                targetFunctionName(_targetFunctionName),
                                                backupPath(_backupPath) {}

        // Creates a TracerInstrumentorFrontendAction to be used by clang tool.
        virtual TracerInstrumentorFrontendAction *create() {
            return new TracerInstrumentorFrontendAction(targetFunctionName, backupPath);
        }
};

// This is absurdly long, but not sure how to break lines up to make more
// readable
std::unique_ptr<TracerInstrumentorFrontendActionFactory> CreateTracerInstrumentorFrontendActionFactory(
        std::string targetFunctionName, std::string backupPath) {
    return std::unique_ptr<TracerInstrumentorFrontendActionFactory>(
        new TracerInstrumentorFrontendActionFactory(targetFunctionName, backupPath));
}

#endif
