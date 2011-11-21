cmake_minimum_required(VERSION 2.8)
project(PNGMEANCOLOR)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -O2")

# Targets
FILE(GLOB PROGRAM_SRCS "*.cc")
add_executable(png-mean-color ${PROGRAM_SRCS})

## Link libraries
find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIR})
target_link_libraries(png-mean-color ${PNG_LIBRARY})
