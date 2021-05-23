COMMON_PATH = Common
NAIVE_PATH = NaiveReduction
DELTA_PATH = DeltaReduction
SLICE_EXTRACTOR_PATH = SliceExtractor
VARIABLE_EXTRACTOR_PATH = VariableExtractor

.PHONY: all common naive delta slice variables clean

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

clean:
	$(MAKE) -C $(COMMON_PATH) clean
	$(MAKE) -C $(NAIVE_PATH) clean
	$(MAKE) -C $(DELTA_PATH) clean
	$(MAKE) -C $(SLICE_EXTRACTOR_PATH) clean
	$(MAKE) -C $(VARIABLE_EXTRACTOR_PATH) clean
