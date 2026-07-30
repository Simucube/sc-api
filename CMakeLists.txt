cmake_minimum_required(VERSION 3.20.0)
project(SimucubeAPI VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

option(SC_API_PYTHON "Enable building Python bindings (Python and nanobind required)" OFF)
option(SC_API_EXAMPLES "Enable building example applications" ${SimucubeAPI_IS_TOP_LEVEL})
option(SC_API_GENERATE_DOCS "Enable generating Doxygen HTML documentation by building target sc-api-generate_docs" ${SimucubeAPI_IS_TOP_LEVEL})
option(SC_API_INSTALL "Enable install targets" ${SimucubeAPI_IS_TOP_LEVEL})

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND SC_API_INSTALL)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Default install path" FORCE)
endif()

option(SC_API_DEFINE_WINNT "Define _WIN32_WINNT" ${SimucubeAPI_IS_TOP_LEVEL})
if (SC_API_DEFINE_WINNT)
    add_compile_definitions(_WIN32_WINNT=0x0601)
endif()


find_package(Threads REQUIRED QUIET)
if (SC_API_PYTHON)
    find_package(Python 3.11 COMPONENTS Interpreter Development.Module REQUIRED)
else()
    find_package(Python 3.11 COMPONENTS Interpreter REQUIRED)
endif()

# Lightweight INTERFACE target exposing only protocol type definitions.
# Used by consumers that need types.h without linking the full sc-api.
if (NOT TARGET sc-api-protocol_types)
    add_library(sc-api-protocol_types INTERFACE)
    target_include_directories(sc-api-protocol_types INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<INSTALL_INTERFACE:include>)
endif()

if (NOT TARGET libeddsa)
    add_subdirectory(src/libeddsa)
endif()

add_library(sc-api
    inc/sc-api/result.h
    inc/sc-api/protocol/actions.h
    inc/sc-api/protocol/core.h
    inc/sc-api/protocol/bson_shm_blocks.h
    inc/sc-api/protocol/dash_stream.h
    inc/sc-api/protocol/variables.h
    inc/sc-api/device_info.h
    inc/sc-api/device_info_fwd.h

    inc/sc-api/variables.h src/variables.cpp

    inc/sc-api/session.h src/session.cpp
    src/compatibility.h src/compatibility.cpp
    src/device_info_internal.h src/device_info.cpp
    src/shm_bson_data_provider.h src/shm_bson_data_provider.cpp
    inc/sc-api/action.h src/action.cpp

    src/crypto/gcm.h
    src/crypto/gcm.c
    src/crypto/aes.h
    src/crypto/aes.c
    src/crypto/ecdh.h
    src/crypto/ecdh.c
    inc/sc-api/protocol/security.h
    inc/sc-api/protocol/variable_types.h
    inc/sc-api/util/bson_builder.h src/util/bson_builder.cpp
    inc/sc-api/util/bson_reader.h src/util/bson_reader.cpp
    inc/sc-api/protocol/telemetry.h

    inc/sc-api/sim_data_builder.h src/sim_data_builder.cpp

    inc/sc-api/telemetry.h
    src/telemetry.cpp
    src/security_impl.h
    src/security_impl.cpp
    inc/sc-api/api_core.h
    src/api.cpp
    inc/sc-api/api.h
    src/api_core.cpp
    inc/sc-api/ffb.h
    src/ffb.cpp
    inc/sc-api/led_control.h
    src/led_control.cpp
    inc/sc-api/dash_stream.h
    src/dash_stream.cpp
    inc/sc-api/time.h
    src/time.cpp
    inc/sc-api/compatibility.h
    src/command_parsing.h src/command_parsing.cpp

    inc/sc-api/device.h
    src/device.cpp
    inc/sc-api/type.h
    src/type.cpp

    inc/sc-api/sim_data.h src/sim_data.cpp

    inc/sc-api/protocol/commands.h
    inc/sc-api/events.h src/events.cpp
    inc/sc-api/session_fwd.h
    inc/sc-api/command.h
    src/command.cpp
)

# Suppress warnings on 3rd party components
if (MSVC)
    set_source_files_properties(src/crypto/gcm.c PROPERTIES COMPILE_FLAGS /wd4267)
    set_source_files_properties(src/util/bson_reader.cpp PROPERTIES COMPILE_FLAGS /wd4018)
endif()

target_include_directories(sc-api PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:include>)

set_property(TARGET sc-api PROPERTY CXX_STANDARD 17)

target_link_libraries(sc-api PRIVATE libeddsa)

# --- Telemetry references generation ---
set(TELEMETRY_REFS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gen/sc-api/telemetry_references.h")
add_custom_command(OUTPUT ${TELEMETRY_REFS_OUTPUT}
    COMMAND Python::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_telemetry_refs.py
        --template ${CMAKE_CURRENT_SOURCE_DIR}/scripts/template/telemetry_references.h
        --output ${TELEMETRY_REFS_OUTPUT}
        ${CMAKE_CURRENT_SOURCE_DIR}/data/telemetry_public.json
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_telemetry_refs.py
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/template/telemetry_references.h
        ${CMAKE_CURRENT_SOURCE_DIR}/data/telemetry_public.json
    VERBATIM
)
target_sources(sc-api PRIVATE ${TELEMETRY_REFS_OUTPUT})
target_include_directories(sc-api PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/gen>)

# --- Device info enum generation ---
set(DEVICE_INFO_DEFS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gen/sc-api/device_info_definitions.h")
add_custom_command(OUTPUT ${DEVICE_INFO_DEFS_OUTPUT}
    COMMAND Python::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_device_info_defs.py
        --template ${CMAKE_CURRENT_SOURCE_DIR}/scripts/template/device_info_definitions.h
        --output ${DEVICE_INFO_DEFS_OUTPUT}
        ${CMAKE_CURRENT_SOURCE_DIR}/data/device_info_enums.json
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_device_info_defs.py
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/template/device_info_definitions.h
        ${CMAKE_CURRENT_SOURCE_DIR}/data/device_info_enums.json
    VERBATIM
)
target_sources(sc-api PRIVATE ${DEVICE_INFO_DEFS_OUTPUT})

# --- Property definitions generation ---
set(PROP_GEN_HDR "${CMAKE_CURRENT_BINARY_DIR}/gen/sc-api/sim_data")
set(PROP_GEN_SRC "${CMAKE_CURRENT_BINARY_DIR}/gen_src/sim_data")

function(add_scapi_property_generation group)
    add_custom_command(
        OUTPUT ${PROP_GEN_HDR}/${group}.h ${PROP_GEN_SRC}/${group}.cpp
        COMMAND Python::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_property_definitions.py
            --group ${group}
            --header_dir ${PROP_GEN_HDR}
            --source_dir ${PROP_GEN_SRC}
            ${CMAKE_CURRENT_SOURCE_DIR}/data/sim_data/${group}_properties.json
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_property_definitions.py
            ${CMAKE_CURRENT_SOURCE_DIR}/data/sim_data/${group}_properties.json
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/template_scapi/properties.h
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/template_scapi/properties.cpp
        VERBATIM
    )
endfunction()

set(property_groups vehicle session sim participant track tire)
foreach(group ${property_groups})
    add_scapi_property_generation(${group})
endforeach()

# Collect generated sources for sc-api
set(gen_sim_data_sources)
set(gen_sim_data_headers)
foreach(group ${property_groups})
    list(APPEND gen_sim_data_headers ${PROP_GEN_HDR}/${group}.h)
    list(APPEND gen_sim_data_sources ${PROP_GEN_SRC}/${group}.cpp)
endforeach()
target_sources(sc-api PRIVATE ${gen_sim_data_headers} ${gen_sim_data_sources})

#           ASIO
# ========================
if (TARGET sc-api-asio)
    target_link_libraries(sc-api PRIVATE sc-api-asio)
else ()  # Define target sc-api-asio to override this behavior
    include(FetchContent)

    FetchContent_Declare(
            fetchcontent_asio
            GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
            GIT_TAG asio-1-30-2
            GIT_SHALLOW)

    FetchContent_MakeAvailable(fetchcontent_asio)

    target_include_directories(sc-api PRIVATE ${fetchcontent_asio_SOURCE_DIR}/asio/include)
    target_compile_definitions(sc-api PRIVATE ASIO_STANDALONE=1 ASIO_NO_DEPRECATED=1)
    target_link_libraries(sc-api PRIVATE Threads::Threads)

    if (MINGW)
        target_link_libraries(sc-api PRIVATE ws2_32)
    endif ()
endif ()

if (SC_API_INSTALL)
    # --- Install ---
    include(CMakePackageConfigHelpers)

    install(TARGETS sc-api EXPORT sc-apiTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)

    if (TARGET sc-api-protocol_types)
        install(TARGETS sc-api-protocol_types EXPORT sc-apiTargets)
    endif()

    install(DIRECTORY inc/sc-api/ DESTINATION include/sc-api FILES_MATCHING PATTERN "*.h")
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gen/sc-api/ DESTINATION include/sc-api FILES_MATCHING PATTERN "*.h")

    install(EXPORT sc-apiTargets
        FILE sc-apiTargets.cmake
        NAMESPACE sc-api::
        DESTINATION lib/cmake/sc-api)

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sc-apiConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/sc-apiConfig.cmake
        INSTALL_DESTINATION lib/cmake/sc-api)

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/sc-apiConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/sc-apiConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/sc-apiConfigVersion.cmake
        DESTINATION lib/cmake/sc-api)
endif()


if (SC_API_EXAMPLES)
    add_subdirectory(examples)
endif()

if (SC_API_PYTHON)
    add_subdirectory(python)
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
