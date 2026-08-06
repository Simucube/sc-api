cmake_minimum_required(VERSION 3.20.0)

# Lets MSVC_DEBUG_INFORMATION_FORMAT select how debug info is stored. Used to
# embed debug info in the static library so that it stays self-contained.
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

project(SimucubeAPI VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

option(SC_API_PYTHON "Enable building Python bindings (Python and nanobind required)" OFF)
option(SC_API_EXAMPLES "Enable building example applications" ${SimucubeAPI_IS_TOP_LEVEL})
option(SC_API_GENERATE_DOCS "Enable generating Doxygen HTML documentation by building target sc-api-generate_docs" ${SimucubeAPI_IS_TOP_LEVEL})
option(SC_API_SHARED "Build sc-api as a shared library (DLL) instead of a static library" ${BUILD_SHARED_LIBS})

# A wheel build only packages the Python extension module. Installing the C++
# SDK as well would copy include/ and lib/ into the wheel.
if (SKBUILD)
    set(SC_API_INSTALL_DEFAULT OFF)
else()
    set(SC_API_INSTALL_DEFAULT ${SimucubeAPI_IS_TOP_LEVEL})
endif()
option(SC_API_INSTALL "Enable install targets" ${SC_API_INSTALL_DEFAULT})

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND SC_API_INSTALL)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Default install path" FORCE)
endif()

include(GNUInstallDirs)

if (MSVC AND NOT SC_API_SHARED AND NOT DEFINED CMAKE_MSVC_DEBUG_INFORMATION_FORMAT)
    # Embed debug info in the objects so that the installed static library is
    # self-contained. Otherwise consumers need a separate .pdb file next to it
    # and get LNK4099 warnings without it. Needs CMake 3.25 or newer.
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
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
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
endif()

if (NOT TARGET libeddsa)
    add_subdirectory(src/libeddsa)
endif()

if (SC_API_SHARED)
    set(SC_API_LIBRARY_TYPE SHARED)
else()
    set(SC_API_LIBRARY_TYPE STATIC)
endif()

add_library(sc-api ${SC_API_LIBRARY_TYPE}
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
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set_target_properties(sc-api PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED TRUE
    # Debug and release artifacts can be installed into the same prefix
    DEBUG_POSTFIX "d"
    POSITION_INDEPENDENT_CODE TRUE)

# C++17 is part of the public interface: every public header requires it.
target_compile_features(sc-api PUBLIC cxx_std_17)

if (SC_API_SHARED)
    # Export every function and class symbol without annotating the sources.
    # Global data still needs SC_API_EXPORT, see cmake/export.h.in.
    set_target_properties(sc-api PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    set(SC_API_BUILT_SHARED 1)
else()
    set(SC_API_BUILT_SHARED 0)
endif()

# Records the build type into the installed headers so that consumers do not
# have to define anything to get the correct declarations.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/export.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/gen/sc-api/export.h
    @ONLY)
target_compile_definitions(sc-api PRIVATE SC_API_BUILDING=1)

# $<BUILD_INTERFACE:> keeps libeddsa out of the exported link interface. It is
# an OBJECT library, so its objects become part of the sc-api binary and there
# is nothing left to link against at install time.
target_link_libraries(sc-api PRIVATE $<BUILD_INTERFACE:libeddsa>)

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
# The property references are header only, so no source files are generated.
set(PROP_GEN_HDR "${CMAKE_CURRENT_BINARY_DIR}/gen/sc-api/sim_data")

function(add_scapi_property_generation group)
    add_custom_command(
        OUTPUT ${PROP_GEN_HDR}/${group}.h
        COMMAND Python::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_property_definitions.py
            --group ${group}
            --header_dir ${PROP_GEN_HDR}
            ${CMAKE_CURRENT_SOURCE_DIR}/data/sim_data/${group}_properties.json
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_property_definitions.py
            ${CMAKE_CURRENT_SOURCE_DIR}/data/sim_data/${group}_properties.json
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/template_scapi/properties.h
        VERBATIM
    )
endfunction()

set(property_groups vehicle session sim participant track tire)
foreach(group ${property_groups})
    add_scapi_property_generation(${group})
endforeach()

# Collect generated headers for sc-api
set(gen_sim_data_headers)
foreach(group ${property_groups})
    list(APPEND gen_sim_data_headers ${PROP_GEN_HDR}/${group}.h)
endforeach()
target_sources(sc-api PRIVATE ${gen_sim_data_headers})

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
    #
    # The install tree is meant to be usable without CMake:
    #
    #   <prefix>/include            single directory to add to the include path
    #   <prefix>/lib                single library to link (sc-api / sc-apid)
    #   <prefix>/bin                sc-api.dll, shared builds only
    #   <prefix>/lib/cmake/sc-api   package config for CMake consumers
    #   <prefix>/lib/pkgconfig      package config for pkg-config consumers
    #
    include(CMakePackageConfigHelpers)

    set(SC_API_CMAKE_CONFIG_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/sc-api)

    install(TARGETS sc-api EXPORT sc-apiTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    if (SC_API_SHARED AND MSVC)
        install(FILES $<TARGET_PDB_FILE:sc-api> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
    endif()

    if (TARGET sc-api-protocol_types)
        install(TARGETS sc-api-protocol_types EXPORT sc-apiTargets)
    endif()

    # Hand written and generated headers are merged into the same tree so that
    # <prefix>/include is the only include path a consumer needs.
    install(DIRECTORY inc/sc-api/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sc-api FILES_MATCHING PATTERN "*.h")
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gen/sc-api/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sc-api FILES_MATCHING PATTERN "*.h")

    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/INTEGRATION.md
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/sc-api)

    # --- CMake package config ---
    install(EXPORT sc-apiTargets
        FILE sc-apiTargets.cmake
        NAMESPACE sc-api::
        DESTINATION ${SC_API_CMAKE_CONFIG_DIR})

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sc-apiConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/sc-apiConfig.cmake
        INSTALL_DESTINATION ${SC_API_CMAKE_CONFIG_DIR})

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/sc-apiConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/sc-apiConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/sc-apiConfigVersion.cmake
        DESTINATION ${SC_API_CMAKE_CONFIG_DIR})

    # --- pkg-config ---
    # Only useful for toolchains without automatic library linking, so the
    # private dependencies match what MinGW and similar toolchains need.
    set(SC_API_PC_LIBDIR ${CMAKE_INSTALL_LIBDIR})
    set(SC_API_PC_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
    if (WIN32)
        set(SC_API_PC_LIBS_PRIVATE "-lws2_32")
    else()
        set(SC_API_PC_LIBS_PRIVATE "-lpthread")
    endif()

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sc-api.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/sc-api.pc
        @ONLY)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sc-api.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()


if (SC_API_EXAMPLES)
    add_subdirectory(examples)
endif()

if (SC_API_PYTHON)
    # Position independent code, which the Python module requires, is already
    # enabled on the sc-api target.
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
