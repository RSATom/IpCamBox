cmake_minimum_required(VERSION 2.8)

project(Test)

find_package(PkgConfig REQUIRED)
pkg_search_module(GIO REQUIRED gio-2.0)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    [^.]*.cpp
    [^.]*.h
    [^.]*.cmake
    )

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${GIO_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Server DeviceBox)
