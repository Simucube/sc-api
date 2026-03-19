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

nanobind_add_module(_native
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
target_link_libraries(_native PRIVATE sc-api)
target_compile_definitions(_native PRIVATE SC_API_VERSION="${PROJECT_VERSION}")

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

target_sources(_native PRIVATE ${SIM_DATA_TABLES_CPP})
target_include_directories(_native PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gen)

install(TARGETS _native LIBRARY DESTINATION simucube_api)

# Stub generation for IDE/type-checker support (PEP 561).
# Only during scikit-build-core builds (SKBUILD set automatically)
# because nanobind_add_stub must import the built module at build time.
if (SKBUILD)
    nanobind_add_stub(
        _native_stub
        MODULE _native
        OUTPUT _native.pyi
        PYTHON_PATH $<TARGET_FILE_DIR:_native>
        DEPENDS _native
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/_native.pyi DESTINATION simucube_api)
endif()
