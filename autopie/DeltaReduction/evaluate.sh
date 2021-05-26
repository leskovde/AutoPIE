#!/usr/bin/env bash

binary="build/bin/DeltaReduction"

while IFS='' read -r line 
do
	source_file=$(echo $line | cut -d\; -f1)
	error_line=$(echo $line | cut -d\; -f2)
	error_message=$(echo $line | cut -d\; -f3)
	arguments=$(echo $line | cut -d\; -f4)

	args=$(echo "--loc-line=$error_line --error-message=\"$error_message\" --arguments=\"$arguments\" \"../EvaluationData/$source_file\" --")

	echo "Launching $binary $args"
	eval time $binary $args

done < ../EvaluationData/args.txt
