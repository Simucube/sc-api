cmake_minimum_required(VERSION 3.20.0)
project(sc-api-core VERSION 0.0.1)

# Lightweight INTERFACE target exposing only protocol type definitions.
# Used by consumers that need types.h without linking the full sc-api-core.
add_library(sc-api-core-protocol_types INTERFACE)
target_include_directories(sc-api-core-protocol_types INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/inc)

add_subdirectory(src/libeddsa)

include(sc-api-core.cmake)

set_property(TARGET sc-api-core PROPERTY CXX_STANDARD 17)

target_link_libraries(sc-api-core PRIVATE libeddsa)

find_package(Python COMPONENTS Interpreter REQUIRED)
find_package(Threads REQUIRED QUIET)

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
target_include_directories(sc-api-core PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/gen)

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
