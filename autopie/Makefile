COMMON_PATH = Common
NAIVE_PATH = NaiveReduction
DELTA_PATH = DeltaReduction
SLICE_EXTRACTOR_PATH = SliceExtractor
VARIABLE_EXTRACTOR_PATH = VariableExtractor

.PHONY: all common naive delta slice variables

all: common naive delta slice variables

common:
	$(MAKE) -C $(COMMON_PATH)

naive:
	$(MAKE) -C $(NAIVE_PATH)

delta:
	$(MAKE) -C $(DELTA_PATH)

slice:
	$(MAKE) -C $(SLICE_EXTRACTOR_PATH)

variables:
	$(MAKE) -C $(VARIABLE_EXTRACTOR_PATH)


