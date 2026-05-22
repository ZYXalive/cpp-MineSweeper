cmake_minimum_required(VERSION 4.1)
project(mineSweeper)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")

add_executable(mineSweeper main.cpp)
