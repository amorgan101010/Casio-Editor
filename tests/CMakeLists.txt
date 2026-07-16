# casioxw_tests — headless test executable (Catch2 v3).
# Home for the golden-file SysEx codec tests in Chunk 5.

add_executable(casioxw_tests
    CoreTests.cpp
)

target_link_libraries(casioxw_tests PRIVATE
    casioxw_core
    Catch2::Catch2WithMain
)

# Register with CTest via Catch2's discovery helper.
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(casioxw_tests)
