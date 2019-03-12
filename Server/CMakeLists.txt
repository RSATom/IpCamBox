cmake_minimum_required(VERSION 2.8)

project(Server)

add_subdirectory(Config)
add_subdirectory(FileConfig)
if(NOT CMAKE_CROSSCOMPILING)
add_subdirectory(PGConfig)
endif()
add_subdirectory(ControlServer)
add_subdirectory(RestreamServer)

target_include_directories(ControlServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(RestreamServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})


find_package(PkgConfig REQUIRED)
pkg_search_module(GSTREAMER_RTSP_SERVER REQUIRED gstreamer-rtsp-server-1.0)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    [^.]*.cpp
    [^.]*.h
    [^.]*.cmake
    )

add_library(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GSTREAMER_RTSP_SERVER_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}
    ControlServer
    RestreamServer
    Config
    FileConfig
    Common)
if(CMAKE_CROSSCOMPILING)
    target_link_libraries(${PROJECT_NAME} pcre resolv) # why?
elseif()
    target_link_libraries(${PROJECT_NAME} PGConfig)
endif()
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
