find_package(Python 3.11 COMPONENTS Interpreter Development.Module REQUIRED)

# If nanobind isn't found via CMAKE_PREFIX_PATH, ask Python for its location.
# This handles the common case where nanobind was installed via pip.
find_package(nanobind CONFIG QUIET)
if (NOT nanobind_FOUND)
    execute_process(
        COMMAND "${Python_EXECUTABLE}" -X utf8 -m nanobind --cmake_dir
        OUTPUT_VARIABLE _nanobind_cmake_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _nanobind_result
        ENCODING UTF-8
    )
    if (_nanobind_result EQUAL 0 AND _nanobind_cmake_dir)
        file(TO_CMAKE_PATH "${_nanobind_cmake_dir}" _nanobind_cmake_dir)
        find_package(nanobind CONFIG REQUIRED PATHS "${_nanobind_cmake_dir}")
    else()
        message(FATAL_ERROR
            "nanobind not found. Install it with: pip install nanobind")
    endif()
endif()

nanobind_add_module(sc-api-python
    module.cpp
    bind_enums.cpp
    bind_exceptions.cpp
    bind_events.cpp
    bind_sim_data.cpp
    bind_api.cpp
    bind_device_info.cpp
    bind_variables.cpp
    bind_telemetry.cpp
    bind_ffb.cpp
    bind_led_control.cpp
)
set_target_properties(sc-api-python PROPERTIES OUTPUT_NAME _native)
target_link_libraries(sc-api-python PRIVATE sc-api)
target_compile_definitions(sc-api-python PRIVATE SC_API_VERSION="${PROJECT_VERSION}")

# ---------------------------------------------------------------------------
# Generate sim_data property tables (C++ header + Python key lists) from JSON
# ---------------------------------------------------------------------------
set(SIM_DATA_TABLES_CPP "${CMAKE_CURRENT_BINARY_DIR}/gen/sim_data_tables_generated.h")
set(SIM_DATA_TABLES_PY "${CMAKE_CURRENT_SOURCE_DIR}/../simucube_api/_sim_data.py")

set(_sim_data_json_deps)
foreach(group vehicle session sim participant track tire)
    list(APPEND _sim_data_json_deps
        "${CMAKE_CURRENT_SOURCE_DIR}/../core/data/sim_data/${group}_properties.json")
endforeach()

add_custom_command(
    OUTPUT ${SIM_DATA_TABLES_CPP} ${SIM_DATA_TABLES_PY}
    COMMAND Python::Interpreter
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/scripts/generate_python_binding_tables.py
        --data-dir ${CMAKE_CURRENT_SOURCE_DIR}/../core/data/sim_data
        --cpp-output ${SIM_DATA_TABLES_CPP}
        --py-output ${SIM_DATA_TABLES_PY}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/scripts/generate_python_binding_tables.py
        ${_sim_data_json_deps}
    VERBATIM
)

target_sources(sc-api-python PRIVATE ${SIM_DATA_TABLES_CPP})
target_include_directories(sc-api-python PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gen)

# ---------------------------------------------------------------------------
# Generate device_info enum bindings from JSON
# ---------------------------------------------------------------------------
set(DEVICE_INFO_BINDINGS_CPP "${CMAKE_CURRENT_BINARY_DIR}/gen/device_info_enums_generated.h")
add_custom_command(
    OUTPUT ${DEVICE_INFO_BINDINGS_CPP}
    COMMAND Python::Interpreter
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/scripts/generate_device_info_bindings.py
        --output ${DEVICE_INFO_BINDINGS_CPP}
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/data/device_info_enums.json
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/scripts/generate_device_info_bindings.py
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/data/device_info_enums.json
    VERBATIM
)
target_sources(sc-api-python PRIVATE ${DEVICE_INFO_BINDINGS_CPP})

install(TARGETS sc-api-python LIBRARY DESTINATION simucube_api)

# Stub generation for IDE/type-checker support (PEP 561).
# Only during scikit-build-core builds (SKBUILD set automatically)
# because nanobind_add_stub must import the built module at build time.
if (SKBUILD)
    nanobind_add_stub(
        sc-api-python_stub
        MODULE _native
        OUTPUT _native.pyi
        PYTHON_PATH $<TARGET_FILE_DIR:sc-api-python>
        DEPENDS sc-api-python
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/_native.pyi DESTINATION simucube_api)
endif()
