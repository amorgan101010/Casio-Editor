#include "SequencerPanel.h"

#include "casioxw/NoteNames.h"

#include <cmath>

namespace
{
    constexpr int kStepWidth = 56;
    constexpr int kParamControlWidth = 390;
    constexpr int kStepColumnHeight = 22 + 2 + 20 + 56;   // select + gap + gate toggle + note knob

    const juce::Colour kSelectedColour = juce::Colours::orange;
    const juce::Colour kHasLocksColour = juce::Colours::goldenrod.darker (0.4f);
    const juce::Colour kIdleColour     = juce::Colours::darkgrey;

    // The p-lockable parameters this MVP exposes, with musical base defaults (an *open* filter, not
    // a closed one at 0 — otherwise sending base cutoff on every unlocked step would mute the
    // sound the moment playback starts). Add more here to scale the lock lane.
    struct Lockable { const char* paramId; int instance; int base; };
    const Lockable kLockables[] = {
        { "tssFLTFcoff", 1, 127 },   // Total Filter Cutoff — fully open
        { "tssFLTFreso", 1, 0   },   // Total Filter Resonance — none
    };
}

SequencerPanel::SequencerPanel (casioxw::SysExCodec& codecIn, casioxw::MidiIO& midiIOIn)
    : codec (codecIn), midiIO (midiIOIn)
{
    // ---- seed the source-of-truth sequence -------------------------------------------------
    for (auto& step : sequence.steps)
        step.velocity = 100;
    for (const auto& l : kLockables)
        sequence.lockable.push_back (casioxw::LockableParam { l.paramId, l.instance, l.base });

    // ---- step grid -------------------------------------------------------------------------
    for (int i = 0; i < 16; ++i)
    {
        auto sc = std::make_unique<StepControl>();

        sc->select.onClick = [this, i] { selectStep (i); };
        addAndMakeVisible (sc->select);

        sc->enabled.onClick = [this, i]
        {
            sequence.steps[(size_t) i].enabled = stepControls[(size_t) i]->enabled.getToggleState();
        };
        addAndMakeVisible (sc->enabled);

        sc->note.setRange (0.0, 127.0, 1.0);
        sc->note.setValue (60.0, juce::dontSendNotification);
        sc->note.textFromValueFunction = [] (double v) { return casioxw::midiNoteName ((int) v); };
        sc->note.valueFromTextFunction = [] (const juce::String& t) -> double
        {
            const auto n = casioxw::noteNameToMidi (t);
            return n.has_value() ? (double) *n : 0.0;
        };
        sc->note.updateText();
        sc->note.onValueChange = [this, i]
        {
            sequence.steps[(size_t) i].note = (int) stepControls[(size_t) i]->note.getValue();
        };
        addAndMakeVisible (sc->note);

        stepControls[(size_t) i] = std::move (sc);
    }

    // ---- transport -------------------------------------------------------------------------
    playStopButton.onClick = [this] { playing ? stop() : play(); };
    addAndMakeVisible (playStopButton);

    randomizeButton.onClick = [this] { randomizeSequence(); };
    addAndMakeVisible (randomizeButton);

    // Rate / time-scale. Item id == steps-per-beat, so the combo maps straight onto the model.
    rateCombo.addItem ("1/4", 1);
    rateCombo.addItem ("1/8", 2);
    rateCombo.addItem ("1/8T", 3);
    rateCombo.addItem ("1/16", 4);
    rateCombo.addItem ("1/16T", 6);
    rateCombo.addItem ("1/32", 8);
    rateCombo.setSelectedId (4, juce::dontSendNotification);   // 1/16 = current default
    rateCombo.onChange = [this]
    {
        const int spb = rateCombo.getSelectedId();
        if (spb > 0)
            sequence.stepsPerBeat = spb;
    };
    addAndMakeVisible (rateCombo);
    addAndMakeVisible (rateLabel);

    tempoSlider.setRange (40.0, 240.0, 1.0);
    tempoSlider.setValue (120.0, juce::dontSendNotification);
    tempoSlider.onValueChange = [this] { sequence.tempoBpm = (int) tempoSlider.getValue(); };
    addAndMakeVisible (tempoSlider);
    addAndMakeVisible (tempoLabel);

    channelSlider.setRange (1.0, 16.0, 1.0);
    channelSlider.setValue (1.0, juce::dontSendNotification);
    channelSlider.onValueChange = [this] { sequence.channel = (int) channelSlider.getValue(); };
    addAndMakeVisible (channelSlider);
    addAndMakeVisible (channelLabel);

    velocitySlider.setRange (1.0, 127.0, 1.0);
    velocitySlider.setValue (100.0, juce::dontSendNotification);
    velocitySlider.onValueChange = [this]
    {
        const int v = (int) velocitySlider.getValue();
        for (auto& step : sequence.steps)
            step.velocity = v;
    };
    addAndMakeVisible (velocitySlider);
    addAndMakeVisible (velocityLabel);

    // ---- mode row --------------------------------------------------------------------------
    baseButton.onClick = [this] { selectStep (-1); };
    addAndMakeVisible (baseButton);

    editButton.onClick = [this] { refreshParamControls(); updateStatusLabel(); refreshStepButtons(); };
    addAndMakeVisible (editButton);

    clearLocksButton.onClick = [this]
    {
        if (selectedStep >= 0)
        {
            casioxw::clearStepLocks (sequence, selectedStep);
            refreshParamControls();
            refreshStepButtons();
        }
    };
    addAndMakeVisible (clearLocksButton);

    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusLabel);

    // ---- lockable-parameter controls (reuse the tone-editor ParamControl) ------------------
    const auto& model = codec.model();
    for (size_t i = 0; i < sequence.lockable.size(); ++i)
    {
        const auto& lp = sequence.lockable[i];
        const auto* info = model.find (lp.paramId);
        jassert (info != nullptr);   // kLockables ids must exist in xwp1.json
        if (info == nullptr)
            continue;

        // Bound randomized locks by the parameter's real range (both 0-127 today, but read it from
        // metadata so a differently-ranged lockable param added later randomizes correctly).
        sequence.lockable[i].minValue = info->range.min;
        sequence.lockable[i].maxValue = info->range.max;

        auto row = std::make_unique<LockRow>();
        row->control = std::make_unique<ParamControl> (model, *info, lp.instance);
        const int index = (int) i;
        row->control->onValueChanged = [this, index] (int value) { onParamEdited (index, value); };
        addAndMakeVisible (*row->control);

        row->lockMarker.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (row->lockMarker);

        lockRows.push_back (std::move (row));
    }

    setSize (kStepWidth * 16 + 16,
             8 + 28 + 6 + 28 + 8 + 28 + 8 + 20 + 4 + (int) lockRows.size() * 30 + 10 + kStepColumnHeight + 8);

    selectStep (-1);   // start in Base mode
    resized();
}

SequencerPanel::~SequencerPanel()
{
    stop();
}

juce::String SequencerPanel::syncKey (const juce::String& paramId, int instance) const
{
    return paramId + "#" + juce::String (instance);
}

void SequencerPanel::applyParam (const juce::String& paramId, int instance, int value)
{
    midiIO.sendFrame (codec.encode (paramId, instance, value));
    lastApplied[syncKey (paramId, instance)] = value;
}

void SequencerPanel::randomizeSequence()
{
    casioxw::randomize (sequence, rng);
    syncStepWidgetsFromSequence();
    refreshParamControls();   // selected step's locks may have changed
    refreshStepButtons();     // has-locks markers
}

void SequencerPanel::syncStepWidgetsFromSequence()
{
    for (int i = 0; i < 16; ++i)
    {
        auto& sc = *stepControls[(size_t) i];
        // dontSendNotification: these are display updates, not user edits — don't fire the
        // onClick/onValueChange handlers that would write straight back into `sequence`.
        sc.enabled.setToggleState (sequence.steps[(size_t) i].enabled, juce::dontSendNotification);
        sc.note.setValue ((double) sequence.steps[(size_t) i].note, juce::dontSendNotification);
        sc.note.updateText();
    }
}

void SequencerPanel::selectStep (int step)
{
    selectedStep = step;
    clearLocksButton.setEnabled (step >= 0);
    refreshParamControls();
    refreshStepButtons();
    updateStatusLabel();
}

void SequencerPanel::onParamEdited (int lockableIndex, int value)
{
    const auto& lp = sequence.lockable[(size_t) lockableIndex];

    if (selectedStep < 0)
        casioxw::setBaseValue (sequence, lp.paramId, lp.instance, value);
    else
        casioxw::setStepLock (sequence, selectedStep, lp.paramId, lp.instance, value);

    // Audition the edit immediately (and keep the playback dedup cache in step with the synth).
    applyParam (lp.paramId, lp.instance, value);

    refreshParamControls();
    refreshStepButtons();
}

void SequencerPanel::refreshParamControls()
{
    const bool baseMode = selectedStep < 0;
    const bool editable = baseMode || editButton.getToggleState();

    for (size_t i = 0; i < lockRows.size(); ++i)
    {
        const auto& lp = sequence.lockable[i];
        auto& row = *lockRows[i];

        const int value = baseMode
            ? lp.baseValue
            : casioxw::effectiveParamValue (sequence, selectedStep, lp.paramId, lp.instance)
                  .value_or (lp.baseValue);

        row.control->setValueFromSync (value);
        row.control->setEnabled (editable);

        const bool locked = ! baseMode
            && casioxw::findStepLock (sequence.steps[(size_t) selectedStep], lp.paramId, lp.instance) != nullptr;

        if (baseMode)
            row.lockMarker.setText ("base value", juce::dontSendNotification);
        else
            row.lockMarker.setText (locked ? "LOCKED" : "(inherits base)", juce::dontSendNotification);

        row.lockMarker.setColour (juce::Label::textColourId,
                                  locked ? kSelectedColour : juce::Colours::grey);
    }
}

void SequencerPanel::refreshStepButtons()
{
    for (int i = 0; i < 16; ++i)
    {
        const bool hasLocks = ! sequence.steps[(size_t) i].locks.empty();
        auto& btn = stepControls[(size_t) i]->select;
        btn.setButtonText (juce::String (i + 1) + (hasLocks ? " *" : ""));
        btn.setColour (juce::TextButton::buttonColourId,
                       i == selectedStep ? kSelectedColour
                                         : (hasLocks ? kHasLocksColour : kIdleColour));
    }
    baseButton.setColour (juce::TextButton::buttonColourId,
                          selectedStep < 0 ? kSelectedColour : kIdleColour);
}

void SequencerPanel::updateStatusLabel()
{
    juce::String text;
    if (selectedStep < 0)
        text = "Editing BASE sound - applies to every unlocked step";
    else if (editButton.getToggleState())
        text = "LOCKING step " + juce::String (selectedStep + 1) + " - move a knob to lock it here";
    else
        text = "Step " + juce::String (selectedStep + 1) + " locks (read-only) - enable Edit Locks to change";

    statusLabel.setText (text, juce::dontSendNotification);
}

void SequencerPanel::play()
{
    if (playing)
        return;

    if (! midiIO.isOutputOpen())
    {
        statusLabel.setText ("Not connected - open a MIDI output on the Solo Synth tab first",
                              juce::dontSendNotification);
        return;
    }

    playing = true;
    currentStep = 0;
    lastApplied.clear();   // force step 0 to establish every parameter's value fresh
    playStopButton.setButtonText ("Stop");
    timerCallback();       // fire step 0 immediately rather than waiting one full interval
}

void SequencerPanel::stop()
{
    if (! playing)
        return;

    stopTimer();
    playing = false;
    playStopButton.setButtonText ("Play");

    if (soundingNote.has_value())
    {
        midiIO.sendNoteOff (soundingChannel, *soundingNote);
        soundingNote.reset();
    }
    midiIO.sendAllNotesOff (sequence.channel);

    // Reset every parameter to its base so a p-lock can't leave the filter stuck closed/resonant.
    for (const auto& lp : sequence.lockable)
        applyParam (lp.paramId, lp.instance, lp.baseValue);

    updateStatusLabel();
}

void SequencerPanel::timerCallback()
{
    // Parameters first, so the note sounds with the step's locked filter already in place. Only
    // re-send a parameter whose effective value actually changed since it was last applied.
    for (const auto& pv : casioxw::effectiveParamValues (sequence, currentStep))
    {
        const auto key = syncKey (pv.paramId, pv.instance);
        const auto it = lastApplied.find (key);
        if (it == lastApplied.end() || it->second != pv.value)
            applyParam (pv.paramId, pv.instance, pv.value);
    }

    // Release the previously-SENT note before sounding the next (note-off targets what was sent,
    // never the possibly-since-edited current step — so an edit mid-play can't orphan a note).
    if (soundingNote.has_value())
    {
        midiIO.sendNoteOff (soundingChannel, *soundingNote);
        soundingNote.reset();
    }

    if (const auto event = casioxw::stepEvent (sequence, currentStep))
    {
        midiIO.sendNoteOn (event->channel, event->note, event->velocity);
        soundingNote = event->note;
        soundingChannel = event->channel;
    }

    currentStep = (currentStep + 1) % 16;
    startTimer ((int) std::lround (casioxw::stepIntervalMs (sequence)));
}

void SequencerPanel::resized()
{
    auto bounds = getLocalBounds().reduced (8);

    auto transportRow = bounds.removeFromTop (28);
    playStopButton.setBounds (transportRow.removeFromLeft (80));
    transportRow.removeFromLeft (8);
    randomizeButton.setBounds (transportRow.removeFromLeft (100));
    transportRow.removeFromLeft (16);
    tempoLabel.setBounds (transportRow.removeFromLeft (80));
    tempoSlider.setBounds (transportRow.removeFromLeft (150));
    transportRow.removeFromLeft (16);
    rateLabel.setBounds (transportRow.removeFromLeft (40));
    rateCombo.setBounds (transportRow.removeFromLeft (80));

    bounds.removeFromTop (6);
    auto transportRow2 = bounds.removeFromTop (28);
    channelLabel.setBounds (transportRow2.removeFromLeft (90));
    channelSlider.setBounds (transportRow2.removeFromLeft (130));
    transportRow2.removeFromLeft (16);
    velocityLabel.setBounds (transportRow2.removeFromLeft (60));
    velocitySlider.setBounds (transportRow2.removeFromLeft (130));

    bounds.removeFromTop (8);
    auto modeRow = bounds.removeFromTop (28);
    baseButton.setBounds (modeRow.removeFromLeft (70));
    modeRow.removeFromLeft (8);
    editButton.setBounds (modeRow.removeFromLeft (110));
    modeRow.removeFromLeft (8);
    clearLocksButton.setBounds (modeRow.removeFromLeft (140));

    bounds.removeFromTop (8);
    statusLabel.setBounds (bounds.removeFromTop (20));

    bounds.removeFromTop (4);
    for (auto& row : lockRows)
    {
        auto r = bounds.removeFromTop (28);
        row->control->setBounds (r.removeFromLeft (kParamControlWidth));
        r.removeFromLeft (10);
        row->lockMarker.setBounds (r.removeFromLeft (150));
        bounds.removeFromTop (2);
    }

    bounds.removeFromTop (10);
    auto stepRow = bounds.removeFromTop (kStepColumnHeight);
    for (int i = 0; i < 16; ++i)
    {
        auto col = stepRow.removeFromLeft (kStepWidth).reduced (3);
        stepControls[(size_t) i]->select.setBounds (col.removeFromTop (22));
        col.removeFromTop (2);
        stepControls[(size_t) i]->enabled.setBounds (col.removeFromTop (20));
        stepControls[(size_t) i]->note.setBounds (col.removeFromTop (56));   // compact rotary knob
    }
}
