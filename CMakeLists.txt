cmake_minimum_required(VERSION 4.0)
project(stoplight C)

set(CMAKE_C_STANDARD 11)

add_executable(stoplight main.c
        traffic_light.c)
