cmake_minimum_required(VERSION 2.8)

project(Protocol)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    [^.]*.cpp
    [^.]*.h
    [^.]*.cmake
    )

file(GLOB PROTOBUFERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    [^.]*.proto
    )

find_package(Protobuf REQUIRED)
PROTOBUF_GENERATE_CPP(PROTO_SOURCES PROTO_HEADERS ${PROTOBUFERS})

add_library(${PROJECT_NAME} ${SOURCES} ${PROTOBUFERS} ${PROTO_SOURCES} ${PROTO_HEADERS})
target_link_libraries(${PROJECT_NAME} ${PROTOBUF_LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
