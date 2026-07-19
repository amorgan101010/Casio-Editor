# casioxw_tests — headless test executable (Catch2 v3).
# Home for the golden-file SysEx codec tests in Chunk 5.

add_executable(casioxw_tests
    CoreTests.cpp
    GoldenTests.cpp
    MidiIOTests.cpp
    ParamModelTests.cpp
    PcmMelodyTests.cpp
    OrganTests.cpp
    HexLayerTests.cpp
    NoteNamesTests.cpp
    SequenceTests.cpp
    SchedulerTests.cpp
    SongTests.cpp
)

target_link_libraries(casioxw_tests PRIVATE
    casioxw_core
    Catch2::Catch2WithMain
)

# Absolute paths to the params map and the golden vectors, so the headless test
# reads the real committed files regardless of the working directory.
target_compile_definitions(casioxw_tests PRIVATE
    CASIOXW_PARAMS_JSON="${CMAKE_SOURCE_DIR}/params/xwp1.json"
    CASIOXW_GOLDEN_JSON="${CMAKE_SOURCE_DIR}/tests/golden/xwp1_golden.json"
)

# Register with CTest via Catch2's discovery helper.
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(casioxw_tests)
