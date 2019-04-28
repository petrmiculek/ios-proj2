cmake_minimum_required(VERSION 3.13)
project(ios_proj2)

set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic -Wextra -O0 -lrt -lpthread")
include_directories(.)

add_executable(ios_proj2
        error_msg.c
        error_msg.h
        proj2.c proj2.h)
