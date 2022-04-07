cmake_minimum_required (VERSION 3.10)

project(rdkafka-project)

#if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
#    set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -pthread  -mtls-dialect=gnu2  -Wall -Wno-unknown-pragmas -Wno-sign-compare -Woverloaded-virtual -Wwrite-strings -Wno-unused")
#    set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g3")
#    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
#endif()
#set(CMAKE_CXX_STANDARD 11)

# Determine minimum C
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)

# Show all warning messages
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

#include(cmake/dpdk.cmake)


add_subdirectory(src)

#enable_testing()

#add_subdirectory(tests)

