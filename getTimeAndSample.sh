#!/bin/bash

echo "Time" "Sampling_Rate"
for i in `seq 1 20`;
do
	for j in `seq 1  5`
	do
		output="$(./FIRESTARTER -t $i -l 100 -b 3 -r 2>/dev/null| grep runtime)"
		timeTaken="$( echo $output | cut -d" " -f2)"
		cyclesTakenWithParen="$( echo $output | cut -d"(" -f2)"
		cyclesTaken="$( echo $cyclesTakenWithParen | cut -d" " -f1)"
		samplingRate="$(echo $cyclesTaken / $timeTaken  | bc)"

		#echo "${output}"
		#echo "${cyclesTaken}"


		echo "${timeTaken}" "${samplingRate}"
	done
done
