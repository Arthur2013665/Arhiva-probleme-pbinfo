cmake_minimum_required(VERSION 3.16)
project(Arhiva_problemelor_pbinfo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Arhiva_problemelor_pbinfo
        main.cpp
        script.cpp
        script.h)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(Arhiva_problemelor_pbinfo stdc++fs)
endif()
