BUILD_PREFIX := ../build/bin

#CXX := $(BUILD_PREFIX)/clang++
CXX := g++
CXXFLAGS := -fno-rtti -O0 -g -std=c++11
PLUGIN_CXXFLAGS := -fpic

LLVM_CXXFLAGS := $(shell $(BUILD_PREFIX)/llvm-config --cxxflags)
LLVM_LDFLAGS  := $(shell $(BUILD_PREFIX)/llvm-config --ldflags --libs --system-libs)

LLVM_INCLUDES := $(shell $(BUILD_PREFIX)/llvm-config --includedir)
CLANG_INCLUDES := -I../llvm/tools/clang/include \
				  -I../build/tools/clang/include


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
all: ClangBase.o FileFinder.o FunctionFileReader.o WrapperGenerator.o Utils.o OperationLog.o EventAnnotator.cc
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) $(CLANG_INCLUDES) $^ $(CLANG_LIBS) $(LLVM_LDFLAGS) -o EventAnnotator

#CodeTransformer.o: CodeTransformer.cc
	#$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) $(CLANG_INCLUDES) -c $^ -o $@

ClangBase.o: ClangBase.cc
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) $(CLANG_INCLUDES) -c $^ -o $@

FileFinder.o: FileFinder.cc
	$(CXX) $(CXXFLAGS) -c $^ -o $@

FunctionFileReader.o: FunctionFileReader.cc 
	$(CXX) $(CXXFLAGS) -c $^ -o $@

WrapperGenerator.o: WrapperGenerator.cc
	$(CXX) $(CXXFLAGS) -c $^ -o $@

Utils.o: Utils.cc
	$(CXX) $(CXXFLAGS) -c $^ -o $@

OperationLog.o: ../ExecutionTimeTracer/C++/OperationLog.cc
	$(CXX) $(CXXFLAGS) -c $^ -o $@

.PHONY: clean
clean: 
	rm *.gch *.o EventAnnotator CodeTransformer FileFinder FunctionFileReader Utils -rf
