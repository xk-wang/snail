cmake_minimum_required(VERSION 3.5)
project(springsnail)
set(CMAKE_CXX_STANDARD 11)
add_executable(snail conn.cpp fdwrapper.cpp log.hpp mgr.cpp main.cpp)
target_include_directories(snail
        PRIVATE ${PROJECT_SOURCE_DIR}/
        )