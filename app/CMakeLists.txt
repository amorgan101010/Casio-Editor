# CasioXWEditor — JUCE standalone GUI application.

# App icon + the params/xwp1.json protocol map, both embedded as binary data so the built
# app has no runtime dependency on a file path (a release binary must work standalone, not
# just from the source tree it was built in).
juce_add_binary_data(CasioXWEditorResources
    SOURCES
        resources/xw-p1.svg
        ../params/xwp1.json
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
    WavePicker.cpp
    ParamPageDisplay.cpp
    SoloSynthPanel.cpp
    EnvelopeDisplay.cpp
    SequencerPanel.cpp
    ArrangerPanel.cpp
    PCMEnginePanel.cpp
    OrganPanel.cpp
    HexLayerPanel.cpp
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
    CasioXWEditorResources
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
