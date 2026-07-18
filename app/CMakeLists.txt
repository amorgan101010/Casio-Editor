# CasioXWEditor — JUCE standalone GUI application.

# App icon (taskbar/window/dock) embedded as binary data so no runtime file path is needed.
juce_add_binary_data(CasioXWEditorIcon
    SOURCES
        resources/xw-p1.svg
)

juce_add_gui_app(CasioXWEditor
    PRODUCT_NAME   "Casio XW-P1 Editor"
    COMPANY_NAME   "CasioXW"
    NEEDS_CURL         FALSE
    NEEDS_WEB_BROWSER  FALSE
)

target_sources(CasioXWEditor PRIVATE
    Main.cpp
    EditorLookAndFeel.cpp
    ParamControl.cpp
    ParamPageDisplay.cpp
    SoloSynthPanel.cpp
    EnvelopeDisplay.cpp
    SequencerPanel.cpp
    PCMEnginePanel.cpp
)

# Absolute path to the params map so the app finds it regardless of the working directory it's
# launched from (matches the CASIOXW_PARAMS_JSON convention already used by casioxw_tests).
target_compile_definitions(CasioXWEditor PRIVATE
    CASIOXW_PARAMS_JSON="${CMAKE_SOURCE_DIR}/params/xwp1.json"
)

# JUCE_WEB_BROWSER/JUCE_USE_CURL off: headless build, no webkit/curl in the link.
target_compile_definitions(CasioXWEditor PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CasioXWEditor,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CasioXWEditor,JUCE_VERSION>"
)

target_link_libraries(CasioXWEditor PRIVATE
    casioxw_core
    CasioXWEditorIcon
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
