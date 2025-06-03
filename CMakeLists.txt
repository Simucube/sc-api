cmake_minimum_required(VERSION 3.20.0)
project(SimucubeAPI VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

option(SC_API_PYTHON "Enable building Python bindings (Python libraries and pybind11 required)" OFF)
option(SC_API_EXAMPLES "Enable building example applications" ${sc_api_IS_TOP_LEVEL})
option(SC_API_GENERATE_DOCS "Enable generating Doxygen HTML documentation by building target sc-api-generate_docs" ${sc_api_IS_TOP_LEVEL})

option(SC_API_DEFINE_WINNT "Define _WIN32_WINNT" ${sc_api_IS_TOP_LEVEL})
if (SC_API_DEFINE_WINNT)
    add_compile_definitions(_WIN32_WINNT=0x0601)
endif()

# Core part of the API is directly exported from Simucube internal sources
add_subdirectory(core)
include(cmake/dependencies.cmake)

add_library(sc-api
    inc/sc-api/api.h
    inc/sc-api/device_info.h
    inc/sc-api/events.h
    inc/sc-api/ffb.h
    inc/sc-api/session.h
    inc/sc-api/sim_data.h
    inc/sc-api/telemetry.h
    inc/sc-api/time.h
    inc/sc-api/variables.h
    src/api.cpp
    src/events.cpp
)

target_include_directories(sc-api PUBLIC inc)
target_link_libraries(sc-api PUBLIC sc-api-core)


if (SC_API_EXAMPLES)
    add_subdirectory(examples)
endif()

if (SC_API_GENERATE_DOCS)
    find_package(Doxygen)
    if (DOXYGEN_FOUND)
        set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
        set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/docs/Doxyfile)

        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

        add_custom_target(sc-api-generate_docs
                COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                COMMENT "Generating SC-API documentation with Doxygen"
                VERBATIM)
    else ()
        message(WARNING "Doxygen not found. SC-API documentation can not be built. Disable this warning by settings SC_API_GENERATE_DOCS to OFF")
    endif ()
endif ()
