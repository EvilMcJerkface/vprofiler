CC=g++
CFLAGS=-I ../llvm/tools/clang/include -I ../llvm/include -I ../build/include -I ../build/tools/clang/include -L ../build/lib -std=c++0x

clangMake: ClangBase.cc ClangBase.h
	$(CC) $(CFLAGS) ClangBase.cc ClangBase.h
