#pragma once

#include "ParamControl.h"

#include "casioxw/MidiIO.h"
#include "casioxw/Sequence.h"
#include "casioxw/SysExCodec.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <vector>

/** 16-step note sequencer with Elektron-style parameter locks.

    `sequence` is the single source of truth (note/vel/enable/tempo/channel + per-step p-locks +
    per-param base values); every widget writes into it on edit and is refreshed from it. Notes go
    out as channel-voice messages; p-locks go out as SysEx via the shared SysExCodec — a p-lock IS
    a tone edit scheduled to a step.

    Interaction model (one rule: the edit target is `(selectedStep, editMode)`, surfaced in a
    status label):
      - `selectedStep < 0` (Base): the param controls edit each parameter's *base* value (the
        track's main sound, applied on every unlocked step). Always editable.
      - a step selected + Edit ON: moving a param control writes a p-lock to that step.
      - a step selected + Edit OFF: the param controls show that step's locks read-only, with a
        marker on each parameter that is locked (vs inheriting the base).

    Timer-driven, not the roadmap's look-ahead scheduler — accepted MVP shortcut. Playback resolves
    each step's effective parameter values (casioxw::effectiveParamValues) and only re-sends the
    ones that changed since last applied (the parameter analogue of the note-off dedup). Stop
    resets every parameter to its base so a p-lock can't leave the filter stuck. */
class SequencerPanel : public juce::Component, private juce::Timer
{
public:
    SequencerPanel (casioxw::SysExCodec& codec, casioxw::MidiIO& midiIO);
    ~SequencerPanel() override;

    void resized() override;

private:
    struct StepControl
    {
        juce::TextButton select;              // shows step number; click selects it
        juce::ToggleButton enabled;
        juce::Slider note { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
    };

    struct LockRow
    {
        std::unique_ptr<ParamControl> control; // reused tone-editor widget (base/effective value)
        juce::Label lockMarker;                // "LOCKED" accent when locked on the selected step
    };

    void timerCallback() override;
    void play();
    void stop();

    void selectStep (int step);                // -1 == Base
    void onParamEdited (int lockableIndex, int value);
    void refreshParamControls();               // value + enabled + lock markers from current context
    void refreshStepButtons();                 // selected highlight + has-locks marker
    void updateStatusLabel();

    juce::String syncKey (const juce::String& paramId, int instance) const;
    void applyParam (const juce::String& paramId, int instance, int value);  // send + track lastApplied

    casioxw::SysExCodec& codec;
    casioxw::MidiIO& midiIO;

    casioxw::Sequence sequence;                // source of truth

    std::array<std::unique_ptr<StepControl>, 16> stepControls;
    std::vector<std::unique_ptr<LockRow>> lockRows;   // one per sequence.lockable entry

    juce::TextButton playStopButton { "Play" };
    juce::Slider tempoSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider channelSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider velocitySlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label tempoLabel { {}, "Tempo (BPM)" };
    juce::Label channelLabel { {}, "MIDI Channel" };
    juce::Label velocityLabel { {}, "Velocity" };

    juce::TextButton baseButton { "Base" };
    juce::ToggleButton editButton { "Edit Locks" };
    juce::TextButton clearLocksButton { "Clear Step Locks" };
    juce::Label statusLabel;

    int currentStep = 0;
    bool playing = false;
    int selectedStep = -1;                     // -1 == Base

    std::optional<int> soundingNote;
    int soundingChannel = 1;
    std::map<juce::String, int> lastApplied;   // keyed by syncKey(); parameter dedup for playback

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerPanel)
};
