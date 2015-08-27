cmake_minimum_required(VERSION 2.4)
cmake_policy(VERSION 2.4)

add_executable(abio
context.c context.h
event_node.c event_node.h
libab.c libab.h
test.c test.h libab_thread.h libab_thread.c)