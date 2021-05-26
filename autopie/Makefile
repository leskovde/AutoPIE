# AutoPIE Makefile.
# Clang and LLVM is linked based on the implicit LLVM 11.0.0 install location.
# (Assumming the target `install` has been finished when building LLVM from source)
# 
# The link location should be the following: `/usr/local/lib/`,
# The include location should be the following: `/usr/local/include/`

# Paths.
COMMON_PATH = Common
NAIVE_PATH = NaiveReduction
DELTA_PATH = DeltaReduction
SLICE_EXTRACTOR_PATH = SliceExtractor
VARIABLE_EXTRACTOR_PATH = VariableExtractor
DOCS_PATH = docs

export CXX = clang++

.PHONY: all common naive delta slice variables docs clean

all: common naive delta slice variables

common:
	$(MAKE) -C $(COMMON_PATH) release

naive:
	$(MAKE) -C $(NAIVE_PATH) release

delta:
	$(MAKE) -C $(DELTA_PATH) release

slice:
	$(MAKE) -C $(SLICE_EXTRACTOR_PATH) release

variables:
	$(MAKE) -C $(VARIABLE_EXTRACTOR_PATH) release

docs:
	@doxygen $(DOCS_PATH)/Doxyfile

clean:
	$(MAKE) -C $(COMMON_PATH) clean
	$(MAKE) -C $(NAIVE_PATH) clean
	$(MAKE) -C $(DELTA_PATH) clean
	$(MAKE) -C $(SLICE_EXTRACTOR_PATH) clean
	$(MAKE) -C $(VARIABLE_EXTRACTOR_PATH) clean
