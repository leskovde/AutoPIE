# AutoPIE Makefile.
# Clang and LLVM is linked based on the implicit LLVM 11.0.0 install location.
# (Assumming the target `install` has been finished when building LLVM from source)
# 
# The link location should be the following: `/usr/local/lib/`,
# The include location should be the following: `/usr/local/include/`

# Paths.
SRC_PATH = src
BUILD_PATH = build
OBJ_PATH = $(BUILD_PATH)/obj
DOCS_PATH = docs

# Targets.
SOURCES = $(wildcard $(SRC_PATH)/*.cpp)
OBJECTS = $(SOURCES:%.cpp=$(OBJ_PATH)/%.o)
DEPENDENCIES = $(OBJECTS:.o=.d)

# Compiler flags.
CXX = clang++
CFLAGS = -std=c++17 -fno-rtti
all: build $(OBJECTS)

$(OBJ_PATH)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) -c $< -MMD -o $@

-include $(DEPENDENCIES)

.PHONY: all build clean release info

build:
	@mkdir -p $(OBJ_PATH)

common:
	$(MAKE) -C $(COMMON_PATH)

release: CFLAGS += -O2
release: all

clean:
	-@rm -rvf $(OBJ_PATH)/*

info:
	@echo "[*] Objects in:     ${OBJ_PATH}"
	@echo "[*] Sources:        ${SOURCES}"
	@echo "[*] Objects:        ${OBJECTS}"
	@echo "[*] Dependencies:   ${DEPENDENCIES}"

