# gui-preview — offscreen ParamControl/SoloSynthPanel snapshot renderer for debugging GUI
# rendering/layout without a live display. Not part of the shipped app.

juce_add_console_app(gui-preview
    PRODUCT_NAME "gui-preview"
)

target_sources(gui-preview PRIVATE
    Main.cpp
    ../../app/EditorLookAndFeel.cpp
    ../../app/ParamControl.cpp
    ../../app/WavePicker.cpp
    ../../app/ParamPageDisplay.cpp
    ../../app/SoloSynthPanel.cpp
    ../../app/SequencerPanel.cpp
    ../../app/ArrangerPanel.cpp
    ../../app/EnvelopeDisplay.cpp
    ../../app/PCMEnginePanel.cpp
    ../../app/OrganPanel.cpp
    ../../app/HexLayerPanel.cpp
)

target_include_directories(gui-preview PRIVATE
    ../../app
)

target_compile_definitions(gui-preview PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(gui-preview PRIVATE
    casioxw_core
    CasioXWEditorResources
    juce::juce_gui_basics
    juce::juce_audio_devices
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
