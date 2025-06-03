add_executable(libeddsa-selftest-ed25519 selftest-ed25519.c)
add_executable(libeddsa-selftest-x25519 selftest-x25519.c)
add_executable(libeddsa-selftest-x25519_base selftest-x25519_base.c)
add_executable(libeddsa-selftest-convert selftest-convert.c)

target_link_libraries(libeddsa-selftest-ed25519 PRIVATE libeddsa)
target_link_libraries(libeddsa-selftest-x25519 PRIVATE libeddsa)
target_link_libraries(libeddsa-selftest-x25519_base PRIVATE libeddsa)
target_link_libraries(libeddsa-selftest-convert PRIVATE libeddsa)


add_test(NAME libeddsa-test-ed25519 COMMAND libeddsa-selftest-ed25519)
add_test(NAME libeddsa-test-x25519 COMMAND libeddsa-selftest-x25519)
add_test(NAME libeddsa-test-x25519_base COMMAND libeddsa-selftest-x25519_base)
add_test(NAME libeddsa-test-convert COMMAND libeddsa-selftest-convert)

# Additional test that verifies that result matches what python cryptography package produces

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/x25519_ref.bin"
    COMMAND Python::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/reference_x25519.py" "${CMAKE_CURRENT_BINARY_DIR}/x25519_ref.bin"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/reference_x25519.py"
)

add_executable(libeddsa-reference_x25519 reference_x25519.c "${CMAKE_CURRENT_BINARY_DIR}/x25519_ref.bin")
target_link_libraries(libeddsa-reference_x25519 PRIVATE libeddsa)
add_test(NAME libeddsa-test-reference_x25519 COMMAND libeddsa-reference_x25519 "${CMAKE_CURRENT_BINARY_DIR}/x25519_ref.bin")

if (MSVC)
    set_source_files_properties(reference_x25519.c PROPERTIES COMPILE_FLAGS /wd4996)
else()
    set_source_files_properties(
            selftest-convert.c PROPERTIES COMPILE_FLAGS "-Wno-unused-variable"
    )
endif()

