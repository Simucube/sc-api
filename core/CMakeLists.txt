cmake_minimum_required(VERSION 3.20.0)
project(sc-api-core VERSION 0.0.1)

# Lightweight INTERFACE target exposing only protocol type definitions.
# Used by consumers that need types.h without linking the full sc-api-core.
if (NOT TARGET sc-api-core-protocol_types)
    add_library(sc-api-core-protocol_types INTERFACE)
    target_include_directories(sc-api-core-protocol_types INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<INSTALL_INTERFACE:include>)
endif()

if (NOT TARGET libeddsa)
    add_subdirectory(src/libeddsa)
endif()

add_library(sc-api-core
    inc/sc-api/core/result.h
    inc/sc-api/core/protocol/actions.h
    inc/sc-api/core/protocol/core.h
    inc/sc-api/core/protocol/bson_shm_blocks.h
    inc/sc-api/core/protocol/dash_stream.h
    inc/sc-api/core/protocol/variables.h
    inc/sc-api/core/device_info.h
    inc/sc-api/core/device_info_fwd.h

    inc/sc-api/core/variables.h src/variables.cpp

    inc/sc-api/core/session.h src/session.cpp
    src/compatibility.h src/compatibility.cpp
    src/device_info_internal.h src/device_info.cpp
    src/shm_bson_data_provider.h src/shm_bson_data_provider.cpp
    inc/sc-api/core/action.h src/action.cpp

    src/crypto/gcm.h
    src/crypto/gcm.c
    src/crypto/aes.h
    src/crypto/aes.c
    src/crypto/ecdh.h
    src/crypto/ecdh.c
    inc/sc-api/core/protocol/security.h
    src/session.cpp
    inc/sc-api/core/protocol/variable_types.h
    inc/sc-api/core/util/bson_builder.h src/util/bson_builder.cpp
    inc/sc-api/core/util/bson_reader.h src/util/bson_reader.cpp
    inc/sc-api/core/protocol/telemetry.h

    inc/sc-api/core/sim_data_builder.h src/sim_data_builder.cpp

    inc/sc-api/core/telemetry.h
    src/telemetry.cpp
    src/security_impl.h
    src/security_impl.cpp
    inc/sc-api/core/api_core.h
    src/api.cpp
    inc/sc-api/core/api.h
    src/api_core.cpp
    inc/sc-api/core/ffb.h
    src/ffb.cpp
    inc/sc-api/core/led_control.h
    src/led_control.cpp
    inc/sc-api/core/dash_stream.h
    src/dash_stream.cpp
    inc/sc-api/core/time.h
    src/time.cpp
    inc/sc-api/core/compatibility.h
    src/command_parsing.h src/command_parsing.cpp

    inc/sc-api/core/device.h
    src/device.cpp
    inc/sc-api/core/type.h
    src/type.cpp

    inc/sc-api/core/sim_data.h src/sim_data.cpp

    inc/sc-api/core/protocol/commands.h
    inc/sc-api/core/events.h src/events.cpp
    inc/sc-api/core/session_fwd.h
    inc/sc-api/core/command.h
    src/command.cpp
)

# Suppress warnings on 3rd party components
if (MSVC)
    set_source_files_properties(src/crypto/gcm.c PROPERTIES COMPILE_FLAGS /wd4267)
    set_source_files_properties(src/util/bson_reader.cpp PROPERTIES COMPILE_FLAGS /wd4018)
endif()

target_include_directories(sc-api-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:include>)

set_property(TARGET sc-api-core PROPERTY CXX_STANDARD 17)

target_link_libraries(sc-api-core PRIVATE libeddsa)

# --- Telemetry references generation ---
set(TELEMETRY_REFS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gen/sc-api/core/telemetry_references.h")
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
target_sources(sc-api-core PRIVATE ${TELEMETRY_REFS_OUTPUT})
target_include_directories(sc-api-core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/gen>)

# --- Device info enum generation ---
set(DEVICE_INFO_DEFS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gen/sc-api/core/device_info_definitions.h")
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
target_sources(sc-api-core PRIVATE ${DEVICE_INFO_DEFS_OUTPUT})

# --- Property definitions generation ---
set(PROP_GEN_HDR "${CMAKE_CURRENT_BINARY_DIR}/gen/sc-api/core/sim_data")
set(PROP_GEN_SRC "${CMAKE_CURRENT_BINARY_DIR}/gen_src/core/sim_data")

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

# Collect generated sources for sc-api-core
set(gen_sim_data_sources)
set(gen_sim_data_headers)
foreach(group ${property_groups})
    list(APPEND gen_sim_data_headers ${PROP_GEN_HDR}/${group}.h)
    list(APPEND gen_sim_data_sources ${PROP_GEN_SRC}/${group}.cpp)
endforeach()
target_sources(sc-api-core PRIVATE ${gen_sim_data_headers} ${gen_sim_data_sources})

# --- Install ---
install(TARGETS sc-api-core EXPORT sc-apiTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)
if (TARGET sc-api-core-protocol_types)
    install(TARGETS sc-api-core-protocol_types EXPORT sc-apiTargets)
endif()
install(DIRECTORY inc/sc-api/ DESTINATION include/sc-api FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gen/sc-api/ DESTINATION include/sc-api FILES_MATCHING PATTERN "*.h")

#           ASIO
# ========================
if (TARGET sc-api-asio)
    target_link_libraries(sc-api-core PRIVATE sc-api-asio)
else ()  # Define target sc-api-asio to override this behavior
    include(FetchContent)

    FetchContent_Declare(
            fetchcontent_asio
            GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
            GIT_TAG asio-1-30-2
            GIT_SHALLOW)

    FetchContent_MakeAvailable(fetchcontent_asio)

    target_include_directories(sc-api-core PRIVATE ${fetchcontent_asio_SOURCE_DIR}/asio/include)
    target_compile_definitions(sc-api-core PRIVATE ASIO_STANDALONE=1 ASIO_NO_DEPRECATED=1)
    target_link_libraries(sc-api-core PRIVATE Threads::Threads)

    if (MINGW)
        target_link_libraries(sc-api-core PRIVATE ws2_32)
    endif ()
endif ()
