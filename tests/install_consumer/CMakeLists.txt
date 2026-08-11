# Smoke test for the sc-api install tree.
#
# It deliberately does NOT use find_package(sc-api). It uses only the include
# directory and the library file, exactly like a build system that is not CMake
# would. If this project builds, then the promise of the install tree holds:
# one include directory and one library.

cmake_minimum_required(VERSION 3.20)
project(sc_api_install_consumer CXX)

set(SC_API_SDK_DIR "" CACHE PATH "Directory where sc-api was installed")
if (NOT SC_API_SDK_DIR)
    message(FATAL_ERROR "Set SC_API_SDK_DIR to the sc-api install prefix")
endif()

find_library(SC_API_LIBRARY
    NAMES sc-api sc-apid
    PATHS "${SC_API_SDK_DIR}/lib"
    NO_DEFAULT_PATH
    REQUIRED)

if (NOT EXISTS "${SC_API_SDK_DIR}/include/sc-api/api.h")
    message(FATAL_ERROR "sc-api headers not found in ${SC_API_SDK_DIR}/include")
endif()

message(STATUS "sc-api library: ${SC_API_LIBRARY}")

add_executable(install_consumer main.cpp)
set_property(TARGET install_consumer PROPERTY CXX_STANDARD 17)
set_property(TARGET install_consumer PROPERTY CXX_STANDARD_REQUIRED TRUE)

target_include_directories(install_consumer PRIVATE "${SC_API_SDK_DIR}/include")
target_link_libraries(install_consumer PRIVATE "${SC_API_LIBRARY}")

if (NOT MSVC)
    # MSVC links the socket libraries automatically through pragmas in the
    # object files. Other toolchains need them on the command line.
    target_link_libraries(install_consumer PRIVATE ws2_32)
endif()
