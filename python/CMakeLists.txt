find_package(Python 3.11 COMPONENTS Interpreter Development.Module REQUIRED)
find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(_native
    module.cpp
    bind_enums.cpp
    bind_exceptions.cpp
    bind_events.cpp
    bind_api.cpp
)
target_link_libraries(_native PRIVATE sc-api)
target_compile_definitions(_native PRIVATE SC_API_VERSION="${PROJECT_VERSION}")

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
