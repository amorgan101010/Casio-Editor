#pragma once

#include "ParamPageDisplay.h"

#include "casioxw/MidiIO.h"
#include "casioxw/Sequence.h"
#include "casioxw/SysExCodec.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <vector>

//==============================================================================
/** One step key in the trig grid, painted like a hardware trig button rather than a stock
    text button: rounded cap, underlined monospace numeral, an amber LED dot when the step holds
    any p-lock, and a structurally thicker outline on the quarter-note steps (1/5/9/13) so bar
    orientation never depends on fill colour alone.

    State model: juce::Button's own toggle state IS the trig on/off (several owners persist it
    from getToggleState()); hasLock/selected are display-only flags pushed via setLockState(). */
class StepKeyButton : public juce::Button
{
public:
    StepKeyButton() : juce::Button ({}) {}

    void setStepIndex (int index) { stepIndex = index; }
    void setLockState (bool hasLockIn, bool selectedIn);

    void paintButton (juce::Graphics&, bool isMouseOver, bool isMouseDown) override;

private:
    int stepIndex = 0;
    bool hasLock = false;
    bool selected = false;
};

//==============================================================================
/** 16-step note sequencer with Elektron-style parameter locks.

    `sequence` is the single source of truth (note/vel/enable/tempo/channel + per-step p-locks +
    per-param base values); every widget writes into it on edit and is refreshed from it. Notes go
    out as channel-voice messages; p-locks go out as NRPN where mapped (SysEx fallback otherwise) —
    a p-lock IS a tone edit scheduled to a step.

    Interaction model (one rule: the edit target is `(selectedStep, editMode)`, surfaced in the
    parameter display's header):
      - `selectedStep < 0` (Base): the param cells edit each parameter's *base* value (the
        track's main sound, applied on every unlocked step). Always editable.
      - a step selected + P-LOCK mode: moving a param cell writes a p-lock to that step, and the
        cell renders inverted (amber) while locked — the Digitakt convention.

    The p-lockable set is far larger than fits on screen, so the cells live in ParamPageDisplay —
    a pageable LCD-style sub-window (2x4 cells per page, page keys underneath). Adding a lockable
    param = one row in kLockables (SequencerPanel.cpp); the pages, playback, and lock UI all
    derive from it.

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

    /** tools/gui-preview only: seed a representative editing state (trigs, p-locks, a selected
        step in P-LOCK mode, a playhead position) so offscreen snapshots can verify the
        state-dependent rendering a fresh panel never shows. Never called by the app itself. */
    void applyPreviewDemoState();

private:
    enum class SaveKind
    {
        solo,
        drums,
        sequenceSet
    };

    struct StepControl
    {
        StepKeyButton select;                 // shows step number; click selects/toggles it
        juce::Slider note { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider gate { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider velocity { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    };

    struct DrumTrackControl
    {
        juce::Label trackLabel;
        juce::TextButton mute { "Mute" };
        juce::ComboBox channel;
        juce::Slider note { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::Slider velocity { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::Label velocityMarker;
        std::array<StepKeyButton, 16> steps;
        int baseVelocity = 100;
        std::array<std::optional<int>, 16> velocityLocks;
        int selectedStep = -1; // per-track p-lock selection target, -1 means base
    };

    void timerCallback() override;
    void play();
    void stop();

    /** Pull every lockable parameter's CURRENT value from the synth and adopt it as that
        parameter's base value — so the sequence's "base sound" is whatever the tone editor /
        hardware actually holds, not the offline defaults in kLockables. Uses the same
        request/drain flow as SoloSynthPanel's Sync (the shared receive queue is only ever
        drained by one panel at a time); only available while stopped, since the playback
        feeder owns this panel's timer while playing. */
    void syncBaseValuesFromSynth();

    void selectStep (int step);                // -1 == Base
    void setPLockMode (bool pLockMode);        // STEP / P-LOCK mode keys both land here
    void onParamEdited (int lockableIndex, int value);
    void refreshParamControls();               // value + locked flags into the param display
    void refreshStepButtons();                 // selected highlight + has-locks LED
    void updateStatusLabel();                  // edit-target readout in the display header
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
    std::unique_ptr<ParamPageDisplay> paramDisplay;   // the pageable p-lock parameter sub-window
    std::array<std::unique_ptr<DrumTrackControl>, 5> drumTrackControls;

    juce::TextButton playStopButton { "Play" };
    juce::TextButton randomizeButton { "Rnd" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton loadButton { "Load" };
    juce::TextButton sequenceDirButton { "Seq Dir" };
    std::unique_ptr<juce::FileChooser> fileChooser;   // kept alive across the async dialog
    juce::Slider tempoSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider channelSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ComboBox rateCombo;
    juce::Label tempoLabel { {}, "BPM" };
    juce::Label channelLabel { {}, "CH" };
    juce::Label rateLabel { {}, "RATE" };

    juce::Random rng;   // seeded from time; drives the Randomize button

    juce::TextButton baseButton { "Base" };
    juce::TextButton syncBaseButton { "Sync" };       // adopt the synth's live values as base
    juce::TextButton stepModeButton { "STEP" };       // grid mode: step keys toggle trigs
    juce::TextButton editButton { "P-LOCK" };         // toggle ON == p-lock edit mode
    juce::TextButton muteSynthButton { "Mute" };
    juce::TextButton clearLocksButton { "Clear Locks" };
    juce::TextButton shiftLeftButton  { "<" };
    juce::TextButton shiftRightButton { ">" };
    juce::TextButton drumControlsButton;
    juce::TextButton synthControlsButton;
    juce::Label statusLabel;                          // footer: file/save/load messages only
    juce::Label drumTracksLabel { {}, "DRUM TRACKS" };
    juce::Label synthLabel { {}, "SOLO SYNTH" };

    juce::Label pitchRowLabel { {}, "Pitch" };
    juce::Label gateRowLabel  { {}, "Gate" };
    juce::Label velocityRowLabel { {}, "Velocity" };

    // Card regions computed by resized(), painted by paint().
    juce::Rectangle<int> drumCardBounds;
    juce::Rectangle<int> synthCardBounds;

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

    // Base-value sync state (message thread only). Keyed "paramId#instance" -> lockable index;
    // non-empty while a base sync is awaiting replies (the shared timer polls the receive queue).
    std::map<juce::String, int> outstandingBaseSync;
    juce::uint32 baseSyncStartedMs = 0;

    bool playing = false;
    int selectedStep = -1;                     // -1 == Base
    int playheadStep = -1;                     // -1 == hidden (stopped / before first step)
    juce::Rectangle<int> playheadLaneBounds;   // shared aligned step columns (drums + synth row)
    juce::File sequenceDefaultDirectory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerPanel)
};
