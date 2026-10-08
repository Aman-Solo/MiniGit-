cmake_minimum_required(VERSION 3.16)
project(MiniGit VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(minigit main.cpp)

if (MSVC)
    target_compile_options(minigit PRIVATE /W4)
else()
    target_compile_options(minigit PRIVATE -Wall -Wextra -Wpedantic)
endif()
