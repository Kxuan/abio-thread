cmake_minimum_required(VERSION 2.4)
cmake_policy(VERSION 2.4)


#SET(CMAKE_BUILD_TYPE Profile)

if(CMAKE_BUILD_TYPE STREQUAL "Profile")
SET( CMAKE_VERBOSE_MAKEFILE on )
SET(CMAKE_CXX_FLAGS_PROFILE "${CMAKE_CXX_FLAGS_PROFILE} -g -pg")
SET(CMAKE_C_FLAGS_PROFILE "${CMAKE_C_FLAGS_PROFILE} -g -pg")

endif()

add_executable(abio
context.c context.h
event_node.c event_node.h
libab.c libab.h
test.c test.h libab_thread.h libab_thread.c)