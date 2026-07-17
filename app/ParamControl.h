#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "casioxw/NoteNames.h"
#include "casioxw/ParamModel.h"

#include <functional>
#include <memory>

//==============================================================================
/** A single data-driven parameter editor widget.

    Given one ParamInfo + a resolved 1-based instance, ParamControl picks the right JUCE widget
    purely from casioxw::decideControlKind() (Chunk 7a, core/include/casioxw/ParamModel.h) —
    there is no per-param hand-authored subclass anywhere in the app:

      - ControlKind::Toggle           -> juce::ToggleButton (on/off, e.g. tssOSCsw)
      - ControlKind::ComboEnum        -> juce::ComboBox, one enum backs every instance
      - ControlKind::ComboEnumPerOsc  -> juce::ComboBox, enum source depends on the instance
                                          (tssOSCwf: OSC1/2 -> soloSynthWaves, OSC3/4 -> soloPcmWaves)
      - ControlKind::ComboRange       -> juce::ComboBox with plain numeric items (no enum backs it,
                                          e.g. tssLFOsync)
      - ControlKind::Slider           -> juce::Slider bounded by range.min/range.max
      - ControlKind::Disabled         -> a greyed-out label, no interactive control at all
                                          (e.g. tssOSCwf on OSC5/OSC6, which have no wave)

    Dumb by design: it knows nothing about SysEx/MidiIO. The owning panel wires onValueChanged to
    build+send a SysEx edit; ParamControl only reports "the user changed the value to X" (X is
    already in the UI-space SysExCodec::encode()'s `value` parameter expects). */
class ParamControl : public juce::Component
{
public:
    /** @param model     used only during construction, to resolve the enum value list backing
                          a combo (ComboEnum/ComboEnumPerOsc); not retained afterwards.
        @param info      must outlive this ParamControl (SoloSynthPanel holds it via
                          codec.model(), a long-lived member — see app/SoloSynthPanel.h).
        @param instance  1-based instance number (oscillator/LFO/etc — fixed for this control's
                          lifetime; the owning panel rebuilds its ParamControls rather than
                          re-pointing one at a different instance). */
    ParamControl (const casioxw::ParamModel& model, const casioxw::ParamInfo& info, int instance);

    const juce::String& paramId() const noexcept { return info.id; }
    int instanceNumber() const noexcept { return instance; }
    casioxw::ControlKind controlKind() const noexcept { return kind; }

    /** Fired once per user-driven change (toggle click / combo selection / slider drag-release
        or value change) — never fired by setValueFromSync(). */
    std::function<void (int)> onValueChanged;

    /** Update the widget's displayed value from a sync read-back WITHOUT firing
        onValueChanged — the guard against a sync reply feeding back into another SysEx send.
        No-op for ControlKind::Disabled (nothing to display). */
    void setValueFromSync (int value);

    void resized() override;

private:
    const casioxw::ParamInfo& info;
    int instance;
    casioxw::ControlKind kind;

    juce::Label nameLabel;

    // Exactly one of these is non-null, chosen by `kind`. unique_ptr (not stack members) so an
    // absent control costs nothing and Disabled needs none at all.
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<juce::Slider> slider;

    void notify (int uiValue);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamControl)
};
