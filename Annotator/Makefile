CXX := g++
CXXFLAGS := -fno-rtti -O0 -g -std=c++11
PLUGIN_CXXFLAGS := -fpic

LLVM_BIN_PATH := ../build/bin

LLVM_CXXFLAGS := $(shell $(LLVM_BIN_PATH)/llvm-config --cxxflags)
LLVM_LDFLAGS  := $(shell $(LLVM_BIN_PATH)/llvm-config --ldflags --libs --system-libs)

CLANG_INCLUDES := \
	-I/home/jrectorb/repos/vprofiler/llvm/tools/clang/include \
	-I/home/jrectorb/repos/vprofiler/build/tools/clang/include

CLANG_LIBS := \
	-Wl,--start-group \
	-lclangAST \
	-lclangASTMatchers \
	-lclangAnalysis \
	-lclangBasic \
	-lclangDriver \
	-lclangEdit \
	-lclangFrontend \
	-lclangFrontendTool \
	-lclangLex \
	-lclangParse \
	-lclangSema \
	-lclangEdit \
	-lclangRewrite \
	-lclangRewriteFrontend \
	-lclangStaticAnalyzerFrontend \
	-lclangStaticAnalyzerCheckers \
	-lclangStaticAnalyzerCore \
	-lclangSerialization \
	-lclangToolingCore \
	-lclangTooling \
	-lclangFormat \
	-Wl,--end-group

.PHONY: all
all: ClangBase.o CodeTransformer.o FileFinder.o FunctionFileReader.o Utils.o EventAnnotator.cc
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) $(CLANG_INCLUDES) $^ $(CLANG_LIBS) $(LLVM_LDFLAGS) -o EventAnnotator

#ClangBase: ClangBase.cc ClangBase.h
	#$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) $(CLANG_INCLUDES) $^ $(CLANG_LIBS) $(LLVM_LDFLAGS) -o $@

CodeTransformer.o: CodeTransformer.cc
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) $(CLANG_INCLUDES) -c $^ $(CLANG_LIBS) $(LLVM_LDFLAGS) -o $@

ClangBase.o: ClangBase.cc
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) $(CLANG_INCLUDES) -c $^ $(CLANG_LIBS) $(LLVM_LDFLAGS) -o $@

FileFinder.o: FileFinder.cc
	$(CXX) $(CXXFLAGS) -c $^ -o $@

FunctionFileReader.o: FunctionFileReader.cc 
	$(CXX) $(CXXFLAGS) -c $^ -o $@

Utils.o: Utils.cc
	$(CXX) $(CXXFLAGS) -c $^ -o $@

.PHONY: clean
clean: 
	rm *.o -rf
