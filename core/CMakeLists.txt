cmake_minimum_required(VERSION 3.20.0)
project(sc-api-core VERSION 0.0.1)

add_subdirectory(src/libeddsa)

include(sc-api-core.cmake)

set_property(TARGET sc-api-core PROPERTY CXX_STANDARD 17)

target_sources(sc-api-core PRIVATE
    src/sim_data/participant.cpp
    src/sim_data/session.cpp
    src/sim_data/sim.cpp
    src/sim_data/tire.cpp
    src/sim_data/track.cpp
    src/sim_data/vehicle.cpp
)
target_link_libraries(sc-api-core PRIVATE libeddsa)
