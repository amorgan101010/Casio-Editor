#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "WavePicker.h"
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
                                          -- EXCEPT wave-number params (vt=="wf"), which get a
                                          WavePicker instead (see WavePicker.h): their enum runs
                                          into the hundreds/thousands, and juce::ComboBox's
                                          PopupMenu-backed dropdown freezes the message thread at
                                          that size (materializes one live Component + does a
                                          text measurement PER ITEM, unbounded, at popup-open time)
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
    /** Chunk 7e item 3: how a ControlKind::Slider should render. Purely a visual layout choice
        made by the caller (SoloSynthPanel, from a PER-PARAM check — see
        casioxw::envelopeStageIds()) — NOT part of casioxw::decideControlKind()'s decision (which
        widget KIND a param needs at all) and has no effect on any ControlKind other than Slider;
        ignored (stays Default/list-style) otherwise. Replaced the Chunk 7d `bool asKnob` flag now
        that there are three mutually-exclusive render styles rather than two. */
    enum class RenderMode
    {
        Default,       // full-width juce::Slider::LinearHorizontal row (list style, unchanged)
        Knob,          // compact rotary knob (Chunk 7d item 2) — non-envelope-stage Slider params
        VerticalFader  // compact juce::Slider::LinearVertical (Chunk 7e item 3) — the 9
                       // envelope-stage params, laid out side-by-side in a fader-bank row
    };

    /** @param model     used only during construction, to resolve the enum value list backing
                          a combo (ComboEnum/ComboEnumPerOsc); not retained afterwards.
        @param info      must outlive this ParamControl (SoloSynthPanel holds it via
                          codec.model(), a long-lived member — see app/SoloSynthPanel.h).
        @param instance  1-based instance number (oscillator/LFO/etc — fixed for this control's
                          lifetime; the owning panel rebuilds its ParamControls rather than
                          re-pointing one at a different instance).
        @param mode      see RenderMode above.
        @param labelOverride  when non-empty, shown instead of the param's own display name —
                          for panels showing several SIMULTANEOUS instances of the SAME param
                          side by side (e.g. the Drawbar Organ's 9 "Position" faders, one per
                          foot length), where the param name alone can't tell them apart the way
                          SoloSynthPanel's one-instance-at-a-time nav can. Empty (default)
                          preserves every existing call site's behaviour unchanged.
        @param invertVerticalFader  only meaningful when mode == VerticalFader: when true, the
                          bottom of the fader's travel is range.max and the top is range.min
                          (the physical-drawbar convention — pulled all the way down/out = loudest
                          — rather than a mixing-console fader's usual up-is-more). Value
                          semantics (getValue()/onValueChanged/setValueFromSync) are completely
                          unaffected — only the visual thumb position flips. Ignored otherwise. */
    ParamControl (const casioxw::ParamModel& model, const casioxw::ParamInfo& info, int instance,
                  RenderMode mode = RenderMode::Default, juce::String labelOverride = {},
                  bool invertVerticalFader = false);

    const juce::String& paramId() const noexcept { return info.id; }
    int instanceNumber() const noexcept { return instance; }
    casioxw::ControlKind controlKind() const noexcept { return kind; }

    /** Which RenderMode this control actually ended up using (mode as requested, unless kind !=
        Slider, in which case it's always Default — see the constructor). Used by SoloSynthPanel
        to decide which controls participate in which wrapping grid (knob vs. vertical-fader) vs.
        a plain full-width relayout on viewport resize. */
    RenderMode renderMode() const noexcept { return mode; }

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
    RenderMode mode = RenderMode::Default;   // only ever != Default when kind == ControlKind::Slider

    juce::Label nameLabel;

    // Exactly one of these is non-null, chosen by `kind`. unique_ptr (not stack members) so an
    // absent control costs nothing and Disabled needs none at all.
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<juce::Slider> slider;
    std::unique_ptr<WavePicker> wavePicker;   // ComboEnum(PerOsc) + vt=="wf" only, see class doc above

    void notify (int uiValue);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamControl)
};
