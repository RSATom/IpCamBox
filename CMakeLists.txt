cmake_minimum_required(VERSION 2.8)

project(IpCamBox)

if(CMAKE_CROSSCOMPILING)
    set(ENV{PKG_CONFIG_PATH} ${CMAKE_SYSROOT}/usr/lib/arm-linux-gnueabihf/pkgconfig)
    set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_SYSROOT})
    add_definitions(-Wno-psabi)
endif()

if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0501)
endif()

if(MSVC)
  foreach(flag_var
      CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
      CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    if(${flag_var} MATCHES "/MD")
      string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    elseif(${flag_var} MATCHES "/MDd")
      string(REGEX REPLACE "/MDd" "/MTd" ${flag_var} "${${flag_var}}")
    endif(${flag_var} MATCHES "/MD")
  endforeach()
endif()

if(CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-Wall)
endif(CMAKE_COMPILER_IS_GNUCC)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(DEPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${DEPS_DIR}
    ${DEPS_DIR}/asio/asio/include
    ${DEPS_DIR}/rapidjson/include
    ${DEPS_DIR}/RtspRestreamServer/deps
    )
add_definitions(-DASIO_STANDALONE)

add_subdirectory(${DEPS_DIR}/RtspRestreamServer)
add_subdirectory(${DEPS_DIR}/NetworkCore)
add_subdirectory(Common)
add_subdirectory(Protocol)
add_subdirectory(DeviceBox)
add_subdirectory(Daemon)
add_subdirectory(DeviceBoxApp)

if(NOT CMAKE_CROSSCOMPILING)
add_subdirectory(Server)
add_subdirectory(Test)
add_subdirectory(ServerApp)
endif()


#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
