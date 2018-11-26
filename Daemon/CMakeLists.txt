cmake_minimum_required(VERSION 2.8)

project(Daemon)

find_package(PkgConfig REQUIRED)

pkg_search_module(SPDLOG spdlog REQUIRED)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    [^.]*.cpp
    [^.]*.h
    [^.]*.cmake
    )

add_library(${PROJECT_NAME} ${SOURCES})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)
