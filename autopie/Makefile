COMMON_PATH = Common
NAIVE_PATH = NaiveReduction
DELTA_PATH = DeltaReduction
SLICE_EXTRACTOR_PATH = SliceExtractor
VARIABLE_EXTRACTOR_PATH = VariableExtractor

.PHONY: all common naive delta slice variables clean

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

clean:
	$(MAKE) -C $(COMMON_PATH) clean
	$(MAKE) -C $(NAIVE_PATH) clean
	$(MAKE) -C $(DELTA_PATH) clean
	$(MAKE) -C $(SLICE_EXTRACTOR_PATH) clean
	$(MAKE) -C $(VARIABLE_EXTRACTOR_PATH) clean
