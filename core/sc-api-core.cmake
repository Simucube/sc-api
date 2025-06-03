add_library(sc-api-core
    inc/sc-api/core/result.h
    inc/sc-api/core/protocol/actions.h
    inc/sc-api/core/protocol/core.h
    inc/sc-api/core/protocol/bson_shm_blocks.h
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

target_include_directories(sc-api-core PUBLIC inc)
