# casioxw_core — GUI-less parameter model + SysEx codec + MIDI I/O for the Casio XW-P1.
# NOT a generic reusable library: XW-P1-specific, separated only so it can be
# unit-tested headless. It may use juce_core / juce_audio_basics (MidiMessage,
# byte helpers) / juce_audio_devices (MidiInput/MidiOutput, ALSA backend on Linux)
# but must NEVER depend on a JUCE GUI module.

add_library(casioxw_core STATIC
    src/SysExCodec.cpp
    src/ParamModel.cpp
    src/MidiIO.cpp
    src/NoteNames.cpp
    src/Sequence.cpp
)

target_include_directories(casioxw_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(casioxw_core PUBLIC cxx_std_17)

# Headless module: no curl / no web browser. PUBLIC so consumers (e.g. the test
# target, which compiles its own copy of juce_core) inherit the same config and
# don't pull unresolved curl symbols into their link.
target_compile_definitions(casioxw_core PUBLIC
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

target_link_libraries(casioxw_core
    PUBLIC
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_devices   # MidiInput/MidiOutput (ALSA backend on Linux); still GUI-less
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
