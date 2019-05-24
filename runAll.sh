#!/bin/bash

#traces=("traces/full/Financial1.spc"  "traces/full/Financial2.spc"  "traces/full/WebSearch1.spc"  "traces/full/WebSearch2.spc"  "traces/full/WebSearch3.spc")
traces=("traces/full/Financial2.spc"  "traces/full/WebSearch1.spc"  "traces/full/WebSearch2.spc"  "traces/full/WebSearch3.spc")
raids=("saRaid" "wlRaid" "diffRaid")
for trace in "${traces[@]}"
do
	for raid in "${raids[@]}"
	do
		command="./raidnew ${raid} ${trace} | tee ${trace}.${raid}.result"
		echo -e "==========================================================="
		echo -e "==========================================================="
		echo -e "==========================================================="
		echo -e "==========================================================="
		echo -e "==========================================================="
		echo $command
		eval $command
	done
done
