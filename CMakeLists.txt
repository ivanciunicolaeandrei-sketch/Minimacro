cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

project(MiniMacro VERSION 0.1.0 LANGUAGES C CXX)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK is not set")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
CPMAddPackage("gh:maxnut/GDReplayFormat#d8d2f0b1193a46cdf47b413dc832616dd052b6a9")
target_link_libraries(${PROJECT_NAME} PRIVATE libGDR)
setup_geode_mod(${PROJECT_NAME})
