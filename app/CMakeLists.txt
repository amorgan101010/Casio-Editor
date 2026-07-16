# CasioXWEditor — JUCE standalone GUI application.

juce_add_gui_app(CasioXWEditor
    PRODUCT_NAME   "Casio XW-P1 Editor"
    COMPANY_NAME   "CasioXW"
    NEEDS_CURL         FALSE
    NEEDS_WEB_BROWSER  FALSE
)

target_sources(CasioXWEditor PRIVATE
    Main.cpp
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
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
