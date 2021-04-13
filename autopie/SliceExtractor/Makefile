# AutoPIE Makefile.
# Clang and LLVM is linked based on the implicit LLVM 11.0.0 install location.
# (Assumming the target `install` has been finished when building LLVM from source)
# 
# The link location should be the following: `/usr/local/lib/`,
# The include location should be the following: `/usr/local/include/`

CXX = g++
OBJECTS = Context.o Helper.o TheTool.o
CFLAGS = -std=c++17 -fno-rtti

CLANGLIBS = \
		-lpthread \
		-lRemarks -lclang-cpp -lclang \
		-lclangARCMigrate -lclangAST -lclangASTMatchers \
		-lclangAnalysis -lclangBasic -lclangCodeGen -lclangCrossTU \
		-lclangDependencyScanning -lclangDirectoryWatcher -lclangDriver \
		-lclangDynamicASTMatchers -lclangEdit -lclangFormat -lclangFrontend \
		-lclangFrontendTool -lclangHandleCXX -lclangHandleLLVM -lclangIndex \
		-lclangLex -lclangParse -lclangRewrite \
		-lclangRewriteFrontend -lclangSema \
		-lclangSerialization -lclangStaticAnalyzerCheckers -lclangStaticAnalyzerCore \
		-lclangStaticAnalyzerFrontend -lclangTesting -lclangTooling \
		-lclangToolingASTDiff -lclangToolingCore -lclangToolingInclusions \
		-lclangToolingRefactoring -lclangToolingSyntax -lclangTransformer

all: TheTool

TheTool: $(OBJECTS) 
	$(CXX) $(CFLAGS) -g -o $@ $(OBJECTS) $(CLANGLIBS) 

TheTool.o: TheTool.cpp
	$(CXX) $(CFLAGS) -c -g TheTool.cpp $(CLANGLIBS)

Context.o: Context.cpp Context.h
	$(CXX) $(CFLAGS) -c -g Context.cpp $(CLANGLIBS)

Helper.o: Helper.cpp Helper.h
	$(CXX) $(CFLAGS) -c -g Helper.cpp $(CLANGLIBS)

clean:
	      rm -rf *.o *.ll TheTool
