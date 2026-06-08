cmake_minimum_required(VERSION 3.15)
project(rtos_scheduler VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(rtos_scheduler ${SOURCES})

target_include_directories(rtos_scheduler PRIVATE include)

target_compile_options(rtos_scheduler PRIVATE -Wall -Wextra -Wpedantic)
