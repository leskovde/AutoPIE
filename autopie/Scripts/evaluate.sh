#!/usr/bin/env bash

script="./SlicingReduction.py"

while IFS='' read -r line 
do
	source_file=$(echo $line | cut -d\; -f1)
	error_line=$(echo $line | cut -d\; -f2)
	error_message=$(echo $line | cut -d\; -f3)
	arguments=$(echo $line | cut -d\; -f4)
	ratio=0.99

	args=$(echo "--line_number=$error_line --error_message=\"$error_message\" --arguments=\"$arguments\" --reduction_ratio=$ratio --source_file=\"../EvaluationData/$source_file\" --static_slice=True --dynamic_slice=True --delta=True --inject=True")

	echo "Launching python3 $script $args"
	eval time python3 $script $args

done < ../EvaluationData/args.txt
