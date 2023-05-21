cmake_minimum_required(VERSION 3.15)

project(UDP-server)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

set(EXEC_NAME server)
set(INCLUDE_DIRS include)
set(SOURCES
        src/${EXEC_NAME}.cpp
        src/test.cpp
        src/data.cpp)
add_executable(${EXEC_NAME} ${SOURCES})
target_include_directories(${EXEC_NAME} PUBLIC ${INCLUDE_DIRS})

set(EXEC_NAME client)
set(INCLUDE_DIRS include)
set(SOURCES
        src/${EXEC_NAME}.cpp
        src/data.cpp)
add_executable(${EXEC_NAME} ${SOURCES})
target_include_directories(${EXEC_NAME} PUBLIC ${INCLUDE_DIRS})
