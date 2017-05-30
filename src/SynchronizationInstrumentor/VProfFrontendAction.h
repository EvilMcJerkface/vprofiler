#ifndef VPROFFRONTENDACTION_H
#define VPROFFRONTENDACTION_H

// Clang libs
#include "clang/Frontend/FrontendAction.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include <unordered_map>
#include <string>
#include <memory>

// VProf libs
#include "ClangBase.h"

class VProfFrontendAction : public clang::ASTFrontendAction {
    private:
        // Maps qualified function name to vprof wrapper name.
        std::shared_ptr<std::unordered_map<std::string, std::string>> functionNameMap;

        // Maps qualified function name to FunctionPrototype object.
        std::shared_ptr<std::unordered_map<std::string, FunctionPrototype>> prototypeMap;

        // Name of the transformed file.
        std::string filename;
        
        // Rewriter used by the consumer.
        std::shared_ptr<clang::Rewriter> rewriter;

        clang::FileID fileID;

        std::string backupPath;

        std::shared_ptr<bool> shouldFlush;

    public:
        VProfFrontendAction(std::shared_ptr<std::unordered_map<std::string, 
                                   std::string>> _functionNameMap,
                                   std::shared_ptr<std::unordered_map<std::string,
                                   FunctionPrototype>> _prototypeMap,
                                   std::string _backupPath):
                                   functionNameMap(_functionNameMap),
                                   prototypeMap(_prototypeMap),
                                   backupPath(_backupPath),
                                   shouldFlush(std::make_shared<bool>(false)) {}

        ~VProfFrontendAction() {}

        void writePathFile(const std::string &backupFilename) {
            std::string backupPathsFilename;

            if (backupPath[backupPath.length() - 1] != '/') {
                backupFileName = backupPath + "/SynchronizationFilenames";
            } else {
                backupFileName = backupPath + "SynchronizationFilenames";
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

                llvm::raw_fd_ostream outputFile(llvm::StringRef(filename),
                                                OutErrInfo, llvm::sys::fs::F_None);

                if (OutErrInfo == ok) {
                    backupFile();

                    const clang::RewriteBuffer *RewriteBuf = rewriter->getRewriteBufferFor(fileID);
                    outputFile << "// VProfiler included header\n#include \"VProfEventWrappers.h\"\n\n";
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

            return std::unique_ptr<VProfASTConsumer>(new VProfASTConsumer(ci,
                                                                          rewriter,
                                                                          functionNameMap,
                                                                          prototypeMap,
                                                                          filename,
                                                                          shouldFlush));
        }
};

#endif
