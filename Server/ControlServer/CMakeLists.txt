cmake_minimum_required(VERSION 2.8)

project(ControlServer)

find_package(PkgConfig REQUIRED)
pkg_search_module(GIO REQUIRED gio-2.0)

find_package(OpenSSL REQUIRED)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    [^.]*.cpp
    [^.]*.h
    [^.]*.cmake
    )

#add_executable(${PROJECT_NAME ${SOURCES})
add_library(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${GIO_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}
    NetworkCore Protocol
    ${OPENSSL_LIBRARIES}
    ${GIO_LIBRARIES})
add_dependencies(${PROJECT_NAME} Protocol)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
