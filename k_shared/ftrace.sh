#!/bin/sh
mkdir -p /data/

echo 8 > /proc/sys/kernel/printk

echo -1000 > /proc/self/oom_score_adj
cd /sys/kernel/debug/tracing

echo 0 > tracing_on
echo 0 > trace

echo 1 > /sys/kernel/debug/tracing/events/oom/oom_score_adj_update/enable 
echo 1 > /sys/kernel/debug/tracing/events/vmscan/mm_shrink_slab_start/enable 
echo 1 > /sys/kernel/debug/tracing/events/vmscan/mm_shrink_slab_end/enable 

echo 1 > tracing_on

TIMES=0
while true
do 
	dmesg |grep "min_adj 0"
	if [ $? -eq 0]
	then
		cat /sys/kernel/debug/tracing/trace > /data/ftrace_.$TIMES
		dmesg > /data/kmsg.txt.$TIMES

		let TIMES+=1

		dmesg -cat
		echo > trace
	fi
	sleep 2
done