cmake_minimum_required(VERSION 3.10)
project(MemoryRiver063 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(code main.cpp)
target_compile_features(code PRIVATE cxx_std_17)
