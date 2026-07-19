#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "casioxw/ParamModel.h"

#include <functional>
#include <vector>

//==============================================================================
/** Searchable, virtualized replacement for juce::ComboBox when the backing enum list is large.

    juce::ComboBox's dropdown is a juce::PopupMenu, which materializes one real ItemComponent
    PER ENTRY (each doing its own text-width measurement) synchronously at popup-open time, with
    no virtualization -- fine for the app's other enums (top out around 19 entries: Env Clock
    Trigger, LFO Wave, etc.) but it locks up the message thread for seconds on a wave-number
    param's enum, which runs from the low hundreds (Hex Layer's Wave, 789 entries) into the
    thousands (Solo Synth's PCM wave picker, 2158 entries) -- reported live by the owner as the
    whole app freezing after the dropdown briefly appears. See .wolf/buglog.json.

    WavePicker shows a button with the current selection's label; clicking it opens a
    juce::CallOutBox containing a search box over a juce::ListBox. ListBox only ever creates a
    row component for what's actually on screen, so opening it costs O(visible rows), not
    O(all entries), regardless of list size. */
class WavePicker : public juce::Component
{
public:
    WavePicker();
    ~WavePicker() override;

    /** entries must outlive this WavePicker -- it points into casioxw::ParamModel's owned enum
        tables (params/xwp1.json's "enums", loaded once for the app's lifetime), the same
        non-ownership convention ParamControl already uses for its ParamInfo&. May be nullptr
        (button becomes inert) if the enum name didn't resolve to anything in the model. */
    void setEntries (const std::vector<casioxw::EnumEntry>* entriesIn);

    /** value is clamped into the entries' known value range before display, same defensive
        clamp ParamControl::setValueFromSync already applied to the old ComboBox path (a synced
        wire value can legitimately fall just outside the UI's known domain). */
    void setSelectedValue (int value, juce::NotificationType notification);
    int getSelectedValue() const noexcept { return selectedValue; }

    /** Fired only when the user picks a row in the popup -- never by setSelectedValue() with
        dontSendNotification, matching ParamControl's onValueChanged/setValueFromSync contract. */
    std::function<void (int)> onValueChanged;

    void resized() override;

    /** Preview/instrumentation-only (tools/gui-preview): opens the popup exactly as a real click
        on the button would, so the freeze-class fix (juce::ComboBox's PopupMenu -> this
        ListBox-backed picker) can be timed/exercised offscreen without a real mouse click. Not
        called anywhere in the shipped app. */
    void triggerOpenForPreview() { showPicker(); }

private:
    const std::vector<casioxw::EnumEntry>* entries = nullptr;
    int selectedValue = -1;

    juce::TextButton button { "-" };
    juce::Component::SafePointer<juce::CallOutBox> activeCallout;   // dismissed in the destructor

    void updateButtonText();
    void showPicker();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavePicker)
};
