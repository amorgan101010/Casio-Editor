# midi-probe — manual CLI for talking to a real XW-P1 over MIDI. Not part of
# the automated test suite; this is the human-run hardware verification gate.

juce_add_console_app(midi-probe
    PRODUCT_NAME "midi-probe"
)

target_sources(midi-probe PRIVATE
    Main.cpp
)

target_compile_definitions(midi-probe PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(midi-probe PRIVATE
    casioxw_core
    juce::juce_audio_devices
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
