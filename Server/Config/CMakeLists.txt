cmake_minimum_required(VERSION 2.8)

project(Config)

find_package(PkgConfig REQUIRED)
pkg_search_module(GIO REQUIRED gio-2.0)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    [^.]*.cpp
    [^.]*.h
    [^.]*.cmake
    )

add_library(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
    ${GIO_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}
    ${GIO_LIBRARIES})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
