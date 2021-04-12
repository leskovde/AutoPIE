# AutoPIE Makefile.
# Clang and LLVM is linked based on the implicit LLVM 11.0.0 install location.
# (Assumming the target `install` has been finished when building LLVM from source)
# 
# The link location should be the following: `/usr/local/lib/`,
# The include location should be the following: `/usr/local/include/`

# Paths.
SRC_PATH = src
BUILD_PATH = build
BIN_PATH = $(BUILD_PATH)/bin
OBJ_PATH = $(BUILD_PATH)/obj
DOCS_PATH = docs

# Targets.
TARGET = AutoPie
SOURCES = $(wildcard $(SRC_PATH)/*.cpp)
OBJECTS = $(SOURCES:%.cpp=$(OBJ_PATH)/%.o)
DEPENDENCIES = $(OBJECTS:.o=.d)

# Compiler flags.
CXX = g++
CFLAGS = -std=c++17 -fno-rtti
CLANGLIBS = \
		-lpthread \
		-lRemarks -llldb -lclang-cpp -lclang \
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

all: build $(BIN_PATH)/$(TARGET)

$(OBJ_PATH)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) $(CLANGLIBS) -c $< -MMD -o $@

$(BIN_PATH)/$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) -o $(BIN_PATH)/$(TARGET) $^ $(CLANGLIBS)

-include $(DEPENDENCIES)

.PHONY: all build clean release docs info

build:
	@mkdir -p $(BIN_PATH)
	@mkdir -p $(OBJ_PATH)

release: CFLAGS += -O2
release: all

test:
	@$(BIN_PATH)/$(TARGET) --loc-file="example.cpp" --loc-line=4 \
		--error-message="segmentation fault" --ratio=0.5 --dump-dot=true example.cpp --

clean:
	-@rm -rvf $(OBJ_PATH)/*
	-@rm -rvf $(BIN_PATH)/*

docs:
	@doxygen $(DOCS_PATH)/Doxyfile

info:
	@echo "[*] Binaries in:    ${BIN_PATH}"
	@echo "[*] Objects in:     ${OBJ_PATH}"
	@echo "[*] Doxygen in:     ${DOCS_PATH}"
	@echo "[*] Sources:        ${SOURCES}"
	@echo "[*] Objects:        ${OBJECTS}"
	@echo "[*] Dependencies:   ${DEPENDENCIES}"
