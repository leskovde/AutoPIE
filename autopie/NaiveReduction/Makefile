# AutoPIE Makefile.
# Clang and LLVM is linked based on the implicit LLVM 11.0.0 install location.
# (Assumming the target `install` has been finished when building LLVM from source)
# 
# The link location should be the following: `/usr/local/lib/`,
# The include location should be the following: `/usr/local/include/`

# Paths.
COMMON_PATH = ../Common
COMMON_SRC_PATH = $(COMMON_PATH)/src
SRC_PATH = src
COMMON_BUILD_PATH = $(COMMON_PATH)/build
BUILD_PATH = build
BIN_PATH = $(BUILD_PATH)/bin
COMMON_OBJ_PATH = $(COMMON_BUILD_PATH)/obj
OBJ_PATH = $(BUILD_PATH)/obj
DOCS_PATH = docs

# Targets.
TARGET = NaiveReduction
COMMON_SOURCES = $(addprefix src/, $(notdir $(wildcard $(COMMON_SRC_PATH)/*.cpp)))
SOURCES = $(wildcard $(SRC_PATH)/*.cpp)
COMMON_OBJECTS = $(COMMON_SOURCES:%.cpp=$(COMMON_OBJ_PATH)/%.o)
OBJECTS = $(SOURCES:%.cpp=$(OBJ_PATH)/%.o)
OBJECTS += $(COMMON_OBJECTS)
DEPENDENCIES = $(OBJECTS:.o=.d)
DEPENDENCIES += $(COMMON_OBJECTS:.o=.d)

# Compiler flags.
CXX = clang++
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

all: build common $(BIN_PATH)/$(TARGET)

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

common:
	$(MAKE) -C $(COMMON_PATH)

release: CFLAGS += -O2
release: all

test:
	@$(BIN_PATH)/$(TARGET) --loc-line=10 \
		--error-message="segmentation fault" --ratio=0.75 --dump-dot=true \
		--verbose=true unstructured0.c --

#	cd test_suite; python3 ./runtests.py; cd ..

clean:
	-@rm -rvf $(OBJ_PATH)/*
	-@rm -rvf $(BIN_PATH)/*
	$(MAKE) -C $(COMMON_PATH) clean

docs:
	@doxygen $(DOCS_PATH)/Doxyfile

info:
	@echo "[*] Binaries in:    ${BIN_PATH}"
	@echo "[*] Objects in:     ${OBJ_PATH}"
	@echo "[*] Doxygen in:     ${DOCS_PATH}"
	@echo "[*] Sources:        ${SOURCES}"
	@echo "[*] Objects:        ${OBJECTS}"
	@echo "[*] Dependencies:   ${DEPENDENCIES}"
