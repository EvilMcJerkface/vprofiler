#ifndef NON_TARGET_TRACER_INSTRUMENTOR_FRONT_END_FACTORY_H
#define NON_TARGET_TRACER_INSTRUMENTOR_FRONT_END_FACTORY_H

#include <system_error>

#include "NonTargetTracerInstrumentorVisitor.h"
#include "Utils.h"

#include "llvm/ADT/SmallString.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Basic/LangOptions.h"

class NonTargetTracerInstrumentorASTConsumer : public clang::ASTConsumer {
    private:
        std::unique_ptr<NonTargetTracerInstrumentorVisitor> visitor;

    public:
        // Override ctor to pass hash map of fully qualified function names to the function
        // name to which the key functions should be converted to in the source.
        explicit NonTargetTracerInstrumentorASTConsumer(clang::CompilerInstance &ci, 
                                std::shared_ptr<clang::Rewriter> _rewriter,
                                std::vector<std::vector<std::string>> &_rootFunctionNamesAndArgs,
                                std::shared_ptr<std::unordered_map<int, bool>> _funcDone,
                                std::shared_ptr<bool> _shouldFlush,
                                std::shared_ptr<std::pair<clang::SourceLocation, std::string>> _wrapperImplLoc,
                                std::shared_ptr<std::vector<clang::SourceLocation>> _funcStartLocs,
                                std::shared_ptr<int> _numFuncs,
                                std::string _functionNamesFile) {
            
            visitor = std::unique_ptr<NonTargetTracerInstrumentorVisitor>(new NonTargetTracerInstrumentorVisitor(ci, 
                                                                    _rewriter,
                                                                    _rootFunctionNamesAndArgs,
                                                                    _funcDone,
                                                                    _shouldFlush,
                                                                    _wrapperImplLoc,
                                                                    _funcStartLocs,
                                                                    _numFuncs,
                                                                    _functionNamesFile));
        }

        ~NonTargetTracerInstrumentorASTConsumer() {}

        virtual void HandleTranslationUnit(clang::ASTContext &context) {
            visitor->TraverseDecl(context.getTranslationUnitDecl());
        }
};

class NonTargetTracerInstrumentorFrontendAction : public clang::ASTFrontendAction {
    private:
        // Name of the transformed file.
        std::string filename;

        std::string fileDir;
        
        // Rewriter used by the consumer.
        std::shared_ptr<clang::Rewriter> rewriter;

        std::vector<std::vector<std::string>> &rootFunctionNamesAndArgs;

        std::shared_ptr<std::unordered_map<int, bool>> funcDone;

        clang::FileID fileID;

        std::string backupPath;

        std::string functionNamesFile;

        std::shared_ptr<int> numFuncs;

        std::shared_ptr<bool> shouldFlush;

        std::shared_ptr<std::pair<clang::SourceLocation, std::string>> wrapperImplLoc;

        std::shared_ptr<std::vector<clang::SourceLocation>> funcStartLocs;

    public:
        NonTargetTracerInstrumentorFrontendAction(std::vector<std::vector<std::string>> &_rootFunctionNamesAndArgs,
                                                  std::shared_ptr<std::unordered_map<int, bool>> _funcDone,
                                                  std::string _backupPath,
                                                  std::string _functionNamesFile,
                                                  std::shared_ptr<int> _numFuncs) :
                                                rootFunctionNamesAndArgs(_rootFunctionNamesAndArgs),
                                                funcDone(_funcDone),
                                                backupPath(_backupPath),
                                                functionNamesFile(_functionNamesFile),
                                                numFuncs(_numFuncs),
                                                shouldFlush(std::make_shared<bool>(false)),
                                                wrapperImplLoc(std::make_shared<std::pair<clang::SourceLocation, std::string>>(std::make_pair(clang::SourceLocation(), ""))),
                                                funcStartLocs(std::make_shared<std::vector<clang::SourceLocation>>()) {}

        ~NonTargetTracerInstrumentorFrontendAction() {}

        void writePathFile(const std::string &backupFilename) {
            std::string backupPathsFilename;

            if (backupPath[backupPath.length() - 1] != '/') {
                backupPathsFilename = backupPath + "/TracerFilenames";
            } else {
                backupPathsFilename = backupPath + "TracerFilenames";
            }

            std::error_code OutErrInfo;
            std::error_code ok;
            llvm::raw_fd_ostream outputFile(llvm::StringRef(backupPathsFilename),
                                            OutErrInfo, llvm::sys::fs::F_Append);
            if (OutErrInfo == ok) {
                outputFile << backupFilename + '\t' + fileDir + filename + '\n';
                outputFile.close();
            }
        }

        void backupFile() {
            std::string backupFileName;
            if (backupPath[backupPath.length() - 1] != '/') {
                backupFileName = backupPath + "/" + filename;
            } else {
                backupFileName = backupPath + filename;
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

        void insertHeader() {
            std::string startInstru = "\n\tNUM_FUNCS_SET(" + std::to_string(*numFuncs) + ");\n";
            for (size_t i = 0; i < funcStartLocs->size(); ++i) {
                rewriter->InsertText(funcStartLocs->at(i), startInstru, true);
            }
        }

        void EndSourceFileAction() override {
            if (*shouldFlush) {
                std::error_code OutErrInfo;
                std::error_code ok;

                rewriter->InsertText(wrapperImplLoc->first, wrapperImplLoc->second, true);
                insertHeader();

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

            return std::unique_ptr<NonTargetTracerInstrumentorASTConsumer>(new NonTargetTracerInstrumentorASTConsumer(ci,
                                                                                                    rewriter,
                                                                                                    rootFunctionNamesAndArgs,
                                                                                                    funcDone,
                                                                                                    shouldFlush,
                                                                                                    wrapperImplLoc,
                                                                                                    funcStartLocs,
                                                                                                    numFuncs,
                                                                                                    functionNamesFile));
        }
};

class NonTargetTracerInstrumentorFrontendActionFactory : public clang::tooling::FrontendActionFactory {
    private:
        std::string backupPath;
        std::string functionNamesFile;
        std::shared_ptr<int> numFuncs;
        std::shared_ptr<std::unordered_map<int, bool>> funcDone;
        std::vector<std::vector<std::string>> rootFunctionNamesAndArgs;

    public:
        NonTargetTracerInstrumentorFrontendActionFactory(std::string _rootFunctionNames,
                                                std::string _backupPath,
                                                std::string _functionNamesFile) :
                                                backupPath(_backupPath),
                                                functionNamesFile(_functionNamesFile),
                                                numFuncs(std::make_shared<int>(1)),
                                                funcDone(std::make_shared<std::unordered_map<int, bool>>()) {
            std::vector<std::string> rootFuncs = SplitString(_rootFunctionNames, ',');
            for (size_t i = 0; i < rootFuncs.size(); ++i) {
                std::string rootFunc = rootFuncs[i];
                std::vector<std::string> nameAndArgs = SplitString(rootFunc, '|');
                nameAndArgs[0] = SplitString(nameAndArgs[0], '-')[0];
                rootFunctionNamesAndArgs.push_back(nameAndArgs);
                (*funcDone)[i] = false;
            }
        }

        // Creates a NonTargetTracerInstrumentorFrontendAction to be used by clang tool.
        virtual NonTargetTracerInstrumentorFrontendAction *create() {
            return new NonTargetTracerInstrumentorFrontendAction(
                rootFunctionNamesAndArgs, funcDone, backupPath, functionNamesFile, numFuncs);
        }
};

// This is absurdly long, but not sure how to break lines up to make more
// readable
std::unique_ptr<NonTargetTracerInstrumentorFrontendActionFactory> CreateNonTargetTracerInstrumentorFrontendActionFactory(
        std::string rootFunctionNames, std::string backupPath, std::string functionNamesFile) {
    return std::unique_ptr<NonTargetTracerInstrumentorFrontendActionFactory>(
        new NonTargetTracerInstrumentorFrontendActionFactory(rootFunctionNames, backupPath, functionNamesFile));
}

#endif
