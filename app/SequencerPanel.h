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
    out as channel-voice messages; p-locks go out as NRPN where mapped (SysEx fallback otherwise) —
    a p-lock IS a tone edit scheduled to a step.

    Interaction model (one rule: the edit target is `(selectedStep, editMode)`, surfaced in a
    status label):
      - `selectedStep < 0` (Base): the param controls edit each parameter's *base* value (the
        track's main sound, applied on every unlocked step). Always editable.
      - a step selected + Edit ON: moving a param control writes a p-lock to that step.
      - a step selected + Edit OFF: the param controls show that step's locks read-only, with a
        marker on each parameter that is locked (vs inheriting the base).

    Playback uses the roadmap's look-ahead + timestamped-output design (SEQUENCER_HANDOFF.md S3/S4):
    a loose message-thread feeder timer fills a short horizon by calling the pure
    casioxw::scheduleStep() per step and handing the timestamped events to the MIDI output thread
    (casioxw::MidiIO::scheduleBlock), so note timing stays steady even when the feeder is jittery
    under background load. Param changes (p-locks + base) go through a single paramMessages() seam —
    SysEx via the codec today, NRPN/CC later — used by both the scheduled path and the immediate
    audition path. Stop flushes pending output + resets every parameter to base so a p-lock can't
    leave the filter stuck. */
class SequencerPanel : public juce::Component, private juce::Timer
{
public:
    SequencerPanel (casioxw::SysExCodec& codec, casioxw::MidiIO& midiIO);
    ~SequencerPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum class SaveKind
    {
        solo,
        drums,
        sequenceSet
    };

    struct StepControl
    {
        juce::TextButton select;              // shows step number; click selects it
        juce::Slider note { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider gate { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider velocity { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    };

    struct LockRow
    {
        std::unique_ptr<ParamControl> control; // reused tone-editor widget (base/effective value)
        juce::Label lockMarker;                // "LOCKED" accent when locked on the selected step
    };

    struct DrumTrackControl
    {
        juce::Label trackLabel;
        juce::TextButton mute { "Mute" };
        juce::ComboBox channel;
        juce::Slider note { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::Slider velocity { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::Label velocityMarker;
        std::array<juce::TextButton, 16> steps;
        int baseVelocity = 100;
        std::array<std::optional<int>, 16> velocityLocks;
        int selectedStep = -1; // per-track p-lock selection target, -1 means base
    };

    void timerCallback() override;
    void play();
    void stop();

    void selectStep (int step);                // -1 == Base
    void onParamEdited (int lockableIndex, int value);
    void refreshParamControls();               // value + enabled + lock markers from current context
    void refreshStepButtons();                 // selected highlight + has-locks marker
    void updateStatusLabel();
    void randomizeSequence();                  // Randomize button -> casioxw::randomize + resync widgets
    void syncStepWidgetsFromSequence();        // push sequence's note/enable back into the step widgets
    void syncTransportWidgetsFromSequence();   // push channel/tempo/rate back into their widgets
    void saveSequenceToFile();
    void loadSequenceFromFile();
    void saveByKind (SaveKind kind);
    juce::String serializeDrumsToJson() const;
    juce::String serializeSequenceSetToJson (const juce::String& soloFile, const juce::String& drumsFile) const;
    bool applySoloSequenceText (const juce::String& text);
    bool applyDrumSequenceText (const juce::String& text);
    bool applyLoadedText (const juce::String& text, const juce::File& sourceFile);  // parse + adopt + resync
    void chooseSequenceDirectory();
    juce::File settingsFilePath() const;
    void loadSequenceSettings();
    void saveSequenceSettings() const;
    bool hasAnyDrumStepSelected() const;
    void clearDrumSelections();
    void updateClearLocksEnabled();

    void feedLookahead();   // scheduler tick: fill the look-ahead horizon with timestamped events
    void updatePlayheadStep(); // shared step-column playhead for synth + drum lanes

    // The p-lock transport seam. Builds the MIDI message(s) for one parameter change: NRPN where
    // mapped (to cut traffic on the live transport path), with SysEx fallback through the proven
    // codec for anything unmapped.
    std::vector<juce::MidiMessage> paramMessages (const juce::String& paramId, int instance, int value,
                                                  int channel) const;
    void sendParamNow (const juce::String& paramId, int instance, int value);   // immediate (audition/reset)

    casioxw::SysExCodec& codec;
    casioxw::MidiIO& midiIO;

    casioxw::Sequence sequence;                // source of truth

    std::array<std::unique_ptr<StepControl>, 16> stepControls;
    std::vector<std::unique_ptr<LockRow>> lockRows;   // one per sequence.lockable entry
    std::array<std::unique_ptr<DrumTrackControl>, 5> drumTrackControls;

    juce::TextButton playStopButton { "Play" };
    juce::TextButton randomizeButton { "Randomize" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton loadButton { "Load" };
    juce::TextButton sequenceDirButton { "Seq Dir" };
    std::unique_ptr<juce::FileChooser> fileChooser;   // kept alive across the async dialog
    juce::Slider tempoSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider channelSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ComboBox rateCombo;
    juce::Label tempoLabel { {}, "Tempo (BPM)" };
    juce::Label channelLabel { {}, "MIDI Channel" };
    juce::Label rateLabel { {}, "Rate" };

    juce::Random rng;   // seeded from time; drives the Randomize button

    juce::TextButton baseButton { "Base" };
    juce::TextButton editButton { "P-Lock Edit" };
    juce::TextButton muteSynthButton { "Mute Synth" };
    juce::TextButton clearLocksButton { "Clear Step Locks" };
    juce::TextButton shiftLeftButton  { "Shift <" };
    juce::TextButton shiftRightButton { "Shift >" };
    juce::TextButton drumControlsButton;
    juce::TextButton synthControlsButton;
    juce::Label statusLabel;
    juce::Label drumTracksLabel { {}, "Drum Tracks (16-step on/off lanes)" };

    juce::Label pitchRowLabel { {}, "Pitch" };
    juce::Label gateRowLabel  { {}, "Gate" };
    juce::Label velocityRowLabel { {}, "Velocity" };

    // Look-ahead transport state (message thread only — the feeder timer runs there, so no locking
    // against `sequence` is needed; precise dispatch happens on JUCE's output thread instead).
    // transportStartMs is the juce::Time::getMillisecondCounter() wall-clock at which step 0 begins
    // (a few ms in the future, so the first events are validly "in the future"); nextStepStartMs is
    // the next unfed step's start, measured relative to it. prevStepIndex feeds scheduleStep()'s
    // param dedup (-1 only for the very first fed step, to establish the whole sound).
    double transportStartMs = 0.0;
    double nextStepStartMs  = 0.0;
    int    nextStepIndex    = 0;
    int    prevStepIndex    = -1;

    bool playing = false;
    int selectedStep = -1;                     // -1 == Base
    int playheadStep = -1;                     // -1 == hidden (stopped / before first step)
    juce::Rectangle<int> playheadLaneBounds;   // shared aligned step columns (drums + synth row)
    juce::File sequenceDefaultDirectory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerPanel)
};
