#!/bin/bash

printf "\n--------------------------\n"
echo "RUNNING FIRESTARTER"
echo "--------------------------"

for i in `seq 1 20`;
do
output="$(./FIRESTARTER -t 2 -l 100 -b 3 -r 2>/dev/null| grep runtime)"
timeTaken="$( echo $output | cut -d" " -f2)"
cyclesTakenWithParen="$( echo $output | cut -d"(" -f2)"
cyclesTaken="$( echo $cyclesTakenWithParen | cut -d" " -f1)"
samplingRate="$(echo $cyclesTaken / $timeTaken  | bc)"

#echo "${output}"
#echo "${cyclesTaken}"

echo "${timeTaken}" "${samplingRate}"
done
