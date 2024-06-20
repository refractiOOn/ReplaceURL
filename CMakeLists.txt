cmake_minimum_required(VERSION 3.24)
project(ReplaceURL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CXX_SOURCE_FILES
    main.cpp
    scope_guard.hpp
)

add_executable(${PROJECT_NAME} ${CXX_SOURCE_FILES})
