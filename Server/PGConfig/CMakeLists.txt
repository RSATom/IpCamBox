cmake_minimum_required(VERSION 2.8)

project(PGConfig)

find_package(PkgConfig REQUIRED)
pkg_search_module(PQ REQUIRED libpq)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    [^.]*.cpp
    [^.]*.h
    [^.]*.cmake
    [^.]*.sql
    )

add_library(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PQ_INCLUDE_DIRS}
    $<TARGET_PROPERTY:Config,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(${PROJECT_NAME}
    Config
    ${PQ_LIBRARIES})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
