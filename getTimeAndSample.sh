#!/bin/bash

printf "\n--------------------------\n"
echo "RUNNING FIRESTARTER"
echo "--------------------------"

output="$(./FIRESTARTER -t 10 -l 100 -b 3 -r | grep runtime)"
timeTaken="$( echo $output | cut -d" " -f2)"
cyclesTakenWithParen="$( echo $output | cut -d"(" -f2)"
cyclesTaken="$( echo $cyclesTakenWithParen | cut -d" " -f1)"
samplingRate="$(echo $cyclesTaken / $timeTaken  | bc)"

#echo "${output}"
#echo "${cyclesTaken}"

echo "${timeTaken}" "${samplingRate}"
