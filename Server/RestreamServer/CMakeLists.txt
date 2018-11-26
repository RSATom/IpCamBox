cmake_minimum_required(VERSION 3.6)

project(RestreamServer)

find_package(PkgConfig REQUIRED)
pkg_search_module(GSTREAMER REQUIRED gstreamer-1.0)
pkg_search_module(GSTREAMER_RTSP REQUIRED gstreamer-rtsp-1.0)
pkg_search_module(GSTREAMER_RTSP_SERVER REQUIRED gstreamer-rtsp-server-1.0)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    [^.]*.cpp
    [^.]*.c
    [^.]*.h
    [^.]*.cmake
    )
if( CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR )
    set( STANDALONE TRUE )
endif()
add_library(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_RTSP_INCLUDE_DIRS}
    ${GSTREAMER_RTSP_SERVER_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}
    gst-interpipe
    RestreamServerLib
    ${GSTREAMER_LDFLAGS}
    ${GSTREAMER_RTSP_LDFLAGS}
    ${GSTREAMER_RTSP_SERVER_LDFLAGS})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
