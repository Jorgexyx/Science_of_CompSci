#!/bin/bash

echo "iterations value" 
num1=$((18581643431543584 - 400))
for i in `seq 1 10`;
do
	num=$((num1 + 35))
	/TSoCS/FIRESTARTER/FIRESTARTER -t 10 -r | grep "total iterations" | cut -d" " -f3 
	echo $num
done
