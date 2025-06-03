add_executable(sc-api-example-overview overview.cpp)
target_link_libraries(sc-api-example-overview PRIVATE sc-api)

add_executable(sc-api-example-effect_pipelines effect_pipelines.cpp)
target_link_libraries(sc-api-example-effect_pipelines PRIVATE sc-api)

add_executable(sc-api-example-game_loop game_loop.cpp)
target_link_libraries(sc-api-example-game_loop PRIVATE sc-api)

add_executable(sc-api-example-variable_definitions variable_definitions.cpp)
target_link_libraries(sc-api-example-variable_definitions PRIVATE sc-api)

add_executable(sc-api-example-pedal_state pedal_state.cpp)
target_link_libraries(sc-api-example-pedal_state PRIVATE sc-api)
