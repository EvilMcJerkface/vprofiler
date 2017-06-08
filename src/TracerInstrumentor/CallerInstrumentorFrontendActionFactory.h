#ifndef CALLER_INSTRUMENTOR_FRONT_END_FACTORY_H
#define CALLER_INSTRUMENTOR_FRONT_END_FACTORY_H

#include "CallerInstrumentorVisitor.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Twine.h"
#include "clang/Tooling/Tooling.h"

#include <system_error>
#include <unistd.h>
#include <errno.h>

class CallerInstrumentorASTConsumer : public clang::ASTConsumer {
    private:
        std::unique_ptr<CallerInstrumentorVisitor> visitor;

    public:
        // Override ctor to pass hash map of fully qualified function names to the function
        // name to which the key functions should be converted to in the source.
        explicit CallerInstrumentorASTConsumer(clang::CompilerInstance &ci, 
                                std::shared_ptr<clang::Rewriter> _rewriter,
                                std::string _targetFunctionName,
                                std::string _callerNameAndArgs,
                                int _targetPathCount,
                                std::shared_ptr<bool> _shouldFlush) {
            
            visitor = std::unique_ptr<CallerInstrumentorVisitor>(new CallerInstrumentorVisitor(ci, 
                                                                    _rewriter,
                                                                    _targetFunctionName,
                                                                    _callerNameAndArgs,
                                                                    _targetPathCount,
                                                                    _shouldFlush));
        }

        ~CallerInstrumentorASTConsumer() {}

        virtual void HandleTranslationUnit(clang::ASTContext &context) {
            visitor->TraverseDecl(context.getTranslationUnitDecl());
        }
};

class CallerInstrumentorFrontendAction : public clang::ASTFrontendAction {
    private:
        // Name of the transformed file.
        std::string filename;
        std::string fileDir;
        
        // Rewriter used by the consumer.
        std::shared_ptr<clang::Rewriter> rewriter;

        // Name of the target function.
        std::string targetFunctionName;

        std::string callerNameAndArgs;

        int targetPathCount;

        clang::FileID fileID;

        std::string backupPath;

        std::shared_ptr<bool> shouldFlush;

    public:
        CallerInstrumentorFrontendAction(std::string _targetFunctionName,
                                         std::string _callerNameAndArgs,
                                         int _targetPathCount,
                                         std::string _backupPath) :
                                            targetFunctionName(_targetFunctionName),
                                            callerNameAndArgs(_callerNameAndArgs),
                                            targetPathCount(_targetPathCount),
                                            backupPath(_backupPath),
                                            shouldFlush(std::make_shared<bool>(false)) {}

        ~CallerInstrumentorFrontendAction() {}

        std::string nextBackupFileName(std::string backupFileName) {
            if (!llvm::sys::fs::exists(llvm::Twine(backupFileName))) {
                return backupFileName;
            }
            int i = 1;
            while (llvm::sys::fs::exists(llvm::Twine(backupFileName + std::to_string(i)))) {
                i++;
            }
            return backupFileName + std::to_string(i);
        }

        std::string writePathFile(const std::string &backupFileName) {
            std::string backupPathsFilename;

            if (backupPath[backupPath.length() - 1] != '/') {
                backupPathsFilename = backupPath + "/CallerFilenames";
            } else {
                backupPathsFilename = backupPath + "CallerFilenames";
            }

            std::string unoverwrittenFileName = nextBackupFileName(backupFileName);
            std::error_code OutErrInfo;
            std::error_code ok;
            llvm::raw_fd_ostream outputFile(llvm::StringRef(backupPathsFilename), 
                                            OutErrInfo, llvm::sys::fs::F_Append); 
            if (OutErrInfo == ok) {
                outputFile << unoverwrittenFileName + '\t' + fileDir + filename + '\n';
                outputFile.close();
            }
            return unoverwrittenFileName;
        }

        void backupFile() {
            std::string backupFileName;
            if (backupPath[backupPath.length() - 1] != '/') {
                backupFileName = backupPath + "/" + filename;
            } else {
                backupFileName = backupPath + filename;
            }

            std::string unoverwrittenFileName = writePathFile(backupFileName);

            std::error_code OutErrInfo;
            std::error_code ok;
            llvm::raw_fd_ostream outputFile(llvm::StringRef(unoverwrittenFileName), 
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

                llvm::raw_fd_ostream outputFile(llvm::StringRef(filename), 
                                                OutErrInfo, llvm::sys::fs::F_None); 

                if (OutErrInfo == ok) {
                    backupFile();
                    const clang::RewriteBuffer *RewriteBuf = rewriter->getRewriteBufferFor(fileID);
                    outputFile << "// TraceTool included header\n#include \"trace_tool.h\"\n\n";
                    std::string content = std::string(RewriteBuf->begin(), RewriteBuf->end());
                    outputFile << content;
                    outputFile.close();
                }
            }
        }

        virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &ci,
                                                                      llvm::StringRef file) {
            rewriter = std::make_shared<clang::Rewriter>();
            rewriter->setSourceMgr(ci.getSourceManager(), ci.getLangOpts());

            filename = file.str();
            llvm::SmallString<64> cwd;
            if (llvm::sys::fs::current_path(cwd) == std::error_code()) {
                fileDir = cwd.str().str();
                if (fileDir[fileDir.length() - 1] != '/') {
                    fileDir += "/";
                }
            }
            fileID = ci.getSourceManager().getMainFileID();

            return std::unique_ptr<CallerInstrumentorASTConsumer>(
                new CallerInstrumentorASTConsumer(ci, rewriter, targetFunctionName, callerNameAndArgs,
                                                  targetPathCount, shouldFlush));
        }
};

class CallerInstrumentorFrontendActionFactory : public clang::tooling::FrontendActionFactory {
    private:
        std::string targetFunctionName;
        std::string callerNameAndArgs;
        int targetPathCount;
        std::string backupPath;

    public:
        CallerInstrumentorFrontendActionFactory(std::string _targetFunctionName,
                                                std::string _callerNameAndArgs,
                                                int _targetPathCount,
                                                std::string _backupPath) :
                                                targetFunctionName(_targetFunctionName),
                                                callerNameAndArgs(_callerNameAndArgs),
                                                targetPathCount(_targetPathCount),
                                                backupPath(_backupPath) {}

        // Creates a CallerInstrumentorFrontendAction to be used by clang tool.
        virtual CallerInstrumentorFrontendAction *create() {
            return new CallerInstrumentorFrontendAction(targetFunctionName, callerNameAndArgs,
                                                        targetPathCount, backupPath);
        }
};

std::unique_ptr<CallerInstrumentorFrontendActionFactory> CreateCallerInstrumentorFrontendActionFactory(
        std::string targetFunctionName, std::string callerNameAndArgs,
        int targetPathCount, std::string backupPath) {
    return std::unique_ptr<CallerInstrumentorFrontendActionFactory>(
        new CallerInstrumentorFrontendActionFactory(targetFunctionName, callerNameAndArgs,
                                                    targetPathCount, backupPath));
}

#endif
