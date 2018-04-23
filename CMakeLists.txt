cmake_minimum_required(VERSION 3.6)
project(FTServer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
set(SOURCE_FILES ftserver.cpp)
set(DIRECTOR)
add_executable(FTServer ${SOURCE_FILES})