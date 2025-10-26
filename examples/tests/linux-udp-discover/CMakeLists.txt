cmake_minimum_required(VERSION 3.20)

# set the project name
project(linux-udp-discover)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")


# build sketch as executable
add_executable (linux-udp-discover linux-udp-discover.cpp)

