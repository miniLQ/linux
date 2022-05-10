#!/bin/sh
tracefs=/sys/kernel/debug/tracing
echo nop > $tracefs/current_tracer
echo 0 > $tracefs/tracing_on
echo 2 > $tracefs/max_graph_depth
#echo $$ > $tracefs/set_ftrace_pid
echo function_graph >$tracefs/current_tracer
echo 'update_curr'>$tracefs/set_graph_function
echo 1 >$tracefs/tracing_on
exec "$@"
cat /mnt/hello.c
