# Based on https://github.com/phlay/libeddsa

include(CheckPrototypeDefinition)

check_prototype_definition(memset_s
        "errno_t memset_s( void *dest, rsize_t destsz, int ch, rsize_t count )"
        "0"
        "string.h"
        HAVE_MEMSET_S)

check_prototype_definition(explicit_bzero
        "void explicit_bzero(void *b, size_t len)"
        ""
        "string.h"
        HAVE_EXPLICIT_BZERO)


add_library(libeddsa
        inc/libeddsa/eddsa.h
        inc/libeddsa/sha512.h

        src/bitness.h
        src/compat.h
        src/ed.h
        src/fld.h
        src/limb.h
        src/sc.h
        src/burn.h
        src/burn.c
        src/burnstack.h
        src/burnstack.c
        src/ed.c
        src/ed_lookup32.h
        src/ed_lookup64.h
        src/ed25519-sha512.c
        src/fld.c
        src/sc.c
        src/sha512.c
        src/x25519.c
        )


if (CMAKE_CROSSCOMPILING)
    target_compile_options(libeddsa PRIVATE $<$<CONFIG:Debug>:-Os>)
endif ()

if (NOT WIN32)
    # Linux Python modules need position independent code
    set_property(TARGET libeddsa PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

if (MSVC)
    set_source_files_properties(src/fld.c PROPERTIES COMPILE_FLAGS /wd4244)
endif()

if (HAVE_MEMSET_S)
    target_compile_definitions(libeddsa PRIVATE HAVE_MEMSET_S=1)
endif ()
if (HAVE_EXPLICIT_BZERO)
    target_compile_definitions(libeddsa PRIVATE HAVE_EXPLICIT_BZERO=1)
    if (NOT MSVC)
        set_source_files_properties(src/burnstack.c PROPERTIES COMPILE_FLAGS -Wno-implicit-function-declaration)
    endif ()
endif ()

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    # Don't use 64bit calculation and __int128 with clang as that produces undefined symbols
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_definitions(libeddsa PRIVATE NO_AUTO_BITNESS=1)
    else ()
        target_compile_definitions(libeddsa PRIVATE USE_64BIT=1)
    endif ()
endif ()

target_include_directories(libeddsa PUBLIC inc)

if (SC_LINK_UNIT_TESTS)
    add_subdirectory(test)
endif ()

if(NOT CMAKE_CROSSCOMPILING)
    install(DIRECTORY . DESTINATION sc-api-core/src/libeddsa)
endif()
