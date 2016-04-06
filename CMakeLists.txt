cmake_minimum_required(VERSION 3.5)
project(Diploma)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    config.h
    db.c
    db.h
    ftfs.c
    log.c
    log.h
    params.h)

add_executable(Diploma ${SOURCE_FILES})