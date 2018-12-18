cmake_minimum_required(VERSION 2.8)

project(DeviceBox)

find_package(PkgConfig REQUIRED)
pkg_search_module(GSTREAMER REQUIRED gstreamer-1.0)
pkg_search_module(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)
pkg_search_module(GIO REQUIRED gio-2.0)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_search_module(SPDLOG spdlog REQUIRED)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    [^.]*.cpp
    [^.]*.h
    [^.]*.cmake
    )

add_library(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CURL_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}
    ${CMAKE_THREAD_LIBS_INIT}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    ${GIO_LIBRARIES})

if(CMAKE_CROSSCOMPILING)
    target_link_libraries(${PROJECT_NAME} pcre resolv) # why?
endif()

target_link_libraries(${PROJECT_NAME} NetworkCore Protocol Common)
add_dependencies(${PROJECT_NAME} Protocol)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)
