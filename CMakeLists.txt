cmake_minimum_required(VERSION 3.20)
set(N injector_shell)
project(${N})

add_executable(${N} main.cpp cmds.cpp)
