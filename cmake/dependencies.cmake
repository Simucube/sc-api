find_package(Threads REQUIRED QUIET)

#           ASIO
# ========================
if (TARGET sc-api-asio)
    target_link_libraries(sc-api-core PRIVATE sc-api-asio)
else()  # Define target sc-api-asio to override this behavior
    include(FetchContent)

    FetchContent_Declare(
            fetchcontent_asio
            GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
            GIT_TAG        asio-1-30-2
            GIT_SHALLOW)

    FetchContent_MakeAvailable(fetchcontent_asio)

    target_include_directories(sc-api-core PRIVATE ${fetchcontent_asio_SOURCE_DIR}/asio/include)
    target_compile_definitions(sc-api-core PRIVATE ASIO_STANDALONE=1 ASIO_NO_DEPRECATED=1)
    target_link_libraries(sc-api-core PRIVATE Threads::Threads)
endif()
