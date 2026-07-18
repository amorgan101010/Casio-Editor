#include "SequencerPanel.h"

#include "casioxw/NoteNames.h"
#include "casioxw/Scheduler.h"

#include <cmath>

namespace
{
    // ---- Look-ahead transport tuning -------------------------------------------------------
    // The feeder timer runs on the (jittery) message thread; precise dispatch is JUCE's output
    // thread. The window must exceed the worst feeder stall to avoid an audible gap, but a bigger
    // window also delays how soon a live edit / tempo change takes effect. These are conservative
    // starting points — the owner can tune them on the actual game-loaded machine.
    constexpr int    kSchedulerTickMs = 12;      // feeder fires ~every 12 ms
    constexpr double kLookaheadMs     = 60.0;    // schedule this far ahead of now
    constexpr double kStartLeadMs     = 15.0;    // put step 0 slightly in the future (valid timestamp)
    constexpr double kScheduleSampleRate = 1000.0;   // 1 "sample" == 1 ms, so sample pos == timeMs

    constexpr int kStepWidth = 58;
    constexpr int kLeftGutter = 46;                       // row labels (On / Pitch / Gate)
    constexpr int kParamControlWidth = 390;
    constexpr int kKnobCell = 74;                         // rotary knob + text box (bigger than before)
    constexpr int kStepColumnHeight = 20 + 2 + 20 + kKnobCell + kKnobCell;   // select + on + note + gate

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
        sc->note.setTextBoxStyle (juce::Slider::TextBoxBelow, false, kStepWidth - 6, 16);
        addAndMakeVisible (sc->note);

        sc->gate.setRange (1.0, 100.0, 1.0);
        sc->gate.setValue (90.0, juce::dontSendNotification);
        sc->gate.textFromValueFunction = [] (double v) { return juce::String ((int) v) + "%"; };
        sc->gate.onValueChange = [this, i]
        {
            sequence.steps[(size_t) i].gatePercent = (int) stepControls[(size_t) i]->gate.getValue();
        };
        sc->gate.updateText();
        sc->gate.setTextBoxStyle (juce::Slider::TextBoxBelow, false, kStepWidth - 6, 16);
        addAndMakeVisible (sc->gate);

        stepControls[(size_t) i] = std::move (sc);
    }

    for (auto* l : { &onRowLabel, &pitchRowLabel, &gateRowLabel })
    {
        l->setJustificationType (juce::Justification::centredRight);
        l->setColour (juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible (*l);
    }

    // ---- transport -------------------------------------------------------------------------
    playStopButton.onClick = [this] { playing ? stop() : play(); };
    addAndMakeVisible (playStopButton);

    randomizeButton.onClick = [this] { randomizeSequence(); };
    addAndMakeVisible (randomizeButton);

    saveButton.onClick = [this] { saveSequenceToFile(); };
    loadButton.onClick = [this] { loadSequenceFromFile(); };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);

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

    shiftLeftButton.onClick  = [this] { casioxw::shiftSteps (sequence, -1); syncStepWidgetsFromSequence(); refreshParamControls(); refreshStepButtons(); };
    shiftRightButton.onClick = [this] { casioxw::shiftSteps (sequence,  1); syncStepWidgetsFromSequence(); refreshParamControls(); refreshStepButtons(); };
    addAndMakeVisible (shiftLeftButton);
    addAndMakeVisible (shiftRightButton);

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

    setSize (kLeftGutter + kStepWidth * 16 + 16,
             8 + 28 + 6 + 28 + 8 + 28 + 8 + 20 + 4 + (int) lockRows.size() * 30 + 10 + kStepColumnHeight + 8);

    selectStep (-1);   // start in Base mode
    resized();
}

SequencerPanel::~SequencerPanel()
{
    stop();
}

std::vector<juce::MidiMessage> SequencerPanel::paramMessages (const juce::String& paramId,
                                                             int instance, int value) const
{
    // The single p-lock transport seam. TODAY: one SysEx frame via the proven codec. The NRPN/CC
    // map (prefer NRPN — covers the whole Solo Synth param set at ~1/3 the bytes; CC where
    // hardware-verified absolute; SysEx fallback) is the next chunk and replaces only this body.
    const auto frame = codec.encode (paramId, instance, value);
    if (frame.size() < 3)
        return {};
    // createSysExMessage() re-adds its own F0/F7, so pass only the bytes between them (as sendFrame).
    return { juce::MidiMessage::createSysExMessage (frame.data() + 1, (int) frame.size() - 2) };
}

void SequencerPanel::sendParamNow (const juce::String& paramId, int instance, int value)
{
    for (const auto& m : paramMessages (paramId, instance, value))
        midiIO.sendMessageNow (m);
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
        sc.gate.setValue ((double) sequence.steps[(size_t) i].gatePercent, juce::dontSendNotification);
        sc.gate.updateText();
    }
}

void SequencerPanel::syncTransportWidgetsFromSequence()
{
    tempoSlider.setValue ((double) sequence.tempoBpm, juce::dontSendNotification);
    channelSlider.setValue ((double) sequence.channel, juce::dontSendNotification);
    // Velocity is a single global "set all" control; there's no one value for a per-step-varied
    // sequence, so show step 0's as representative (the per-step values still play correctly).
    velocitySlider.setValue ((double) sequence.steps[0].velocity, juce::dontSendNotification);
    rateCombo.setSelectedId (sequence.stepsPerBeat, juce::dontSendNotification);
}

void SequencerPanel::saveSequenceToFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Save sequence",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("sequence.xwseq"),
        "*.xwseq");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File())
                return;   // cancelled
            if (! file.hasFileExtension ("xwseq"))
                file = file.withFileExtension ("xwseq");

            if (file.replaceWithText (casioxw::sequenceToJson (sequence)))
                statusLabel.setText ("Saved " + file.getFileName(), juce::dontSendNotification);
            else
                statusLabel.setText ("Save failed: " + file.getFullPathName(), juce::dontSendNotification);
        });
}

void SequencerPanel::loadSequenceFromFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load sequence",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.xwseq");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (! file.existsAsFile())
                return;   // cancelled
            applyLoadedText (file.loadFileAsString(), file.getFileName());
        });
}

void SequencerPanel::applyLoadedText (const juce::String& text, const juce::String& name)
{
    const auto loaded = casioxw::sequenceFromJson (text);
    if (! loaded.has_value())
    {
        statusLabel.setText ("Load failed: " + name + " is not a sequence file", juce::dontSendNotification);
        return;
    }

    // Adopt the loaded musical content, but keep THIS panel's lockable set + controls intact
    // (they're fixed by kLockables and already have the metadata min/max seeded). Only import each
    // known lockable param's base value, matched by id.
    sequence.steps        = loaded->steps;
    sequence.channel      = loaded->channel;
    sequence.tempoBpm     = loaded->tempoBpm;
    sequence.stepsPerBeat = loaded->stepsPerBeat;
    for (auto& lp : sequence.lockable)
        for (const auto& llp : loaded->lockable)
            if (llp.paramId == lp.paramId && llp.instance == lp.instance)
                lp.baseValue = llp.baseValue;

    syncStepWidgetsFromSequence();
    syncTransportWidgetsFromSequence();
    selectStep (-1);   // back to Base; also refreshes param controls, step markers, status
    statusLabel.setText ("Loaded " + name, juce::dontSendNotification);
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

    // Audition immediately only when stopped, or editing the Base sound. A base edit during
    // playback still needs the immediate send (the scheduler's step-to-step dedup won't re-emit an
    // unchanging base). A LOCK edit during playback must NOT audition globally — under look-ahead it
    // would bleed the locked value across the whole pattern; the scheduler plays it when that step
    // next comes round (<= one loop), which is the correct p-lock feel anyway.
    if (! playing || selectedStep < 0)
        sendParamNow (lp.paramId, lp.instance, value);

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
    midiIO.startPlaybackThread();   // JUCE's high-res output thread dispatches the timestamps

    transportStartMs = (double) juce::Time::getMillisecondCounter() + kStartLeadMs;
    nextStepStartMs  = 0.0;
    nextStepIndex    = 0;
    prevStepIndex    = -1;          // first fed step establishes every parameter's value fresh

    playStopButton.setButtonText ("Stop");
    feedLookahead();                // prime the horizon before the first timer tick
    startTimer (kSchedulerTickMs);
}

void SequencerPanel::stop()
{
    if (! playing)
        return;

    stopTimer();
    playing = false;
    playStopButton.setButtonText ("Play");

    // Discard everything still queued for future dispatch (incl. not-yet-fired note-offs), then
    // release + reset explicitly since those dropped note-offs won't arrive on their own.
    midiIO.stopPlaybackThread();
    midiIO.sendAllNotesOff (sequence.channel);

    // Reset every parameter to its base so a p-lock can't leave the filter stuck closed/resonant.
    for (const auto& lp : sequence.lockable)
        sendParamNow (lp.paramId, lp.instance, lp.baseValue);

    updateStatusLabel();
}

void SequencerPanel::feedLookahead()
{
    // Fill everything whose start time falls within [now, now + lookahead). The feeder only has to
    // stay ahead of the horizon; the output thread delivers each event at its exact timestamp, so
    // jitter in this (message-thread) callback doesn't move the notes.
    const double now     = (double) juce::Time::getMillisecondCounter();
    const double horizon = now + kLookaheadMs;

    while (transportStartMs + nextStepStartMs < horizon)
    {
        juce::MidiBuffer buffer;
        for (const auto& e : casioxw::scheduleStep (sequence, nextStepIndex, prevStepIndex, nextStepStartMs))
        {
            const int samplePos = (int) std::llround (e.timeMs);   // 1 sample == 1 ms (kScheduleSampleRate)
            switch (e.type)
            {
                case casioxw::ScheduledEvent::Type::noteOn:
                    buffer.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.velocity), samplePos);
                    break;
                case casioxw::ScheduledEvent::Type::noteOff:
                    buffer.addEvent (juce::MidiMessage::noteOff (e.channel, e.note), samplePos);
                    break;
                case casioxw::ScheduledEvent::Type::paramChange:
                    for (const auto& m : paramMessages (e.paramId, e.instance, e.value))
                        buffer.addEvent (m, samplePos);
                    break;
            }
        }

        if (! buffer.isEmpty())
            midiIO.scheduleBlock (buffer, transportStartMs, kScheduleSampleRate);

        prevStepIndex   = nextStepIndex;
        nextStepIndex   = (nextStepIndex + 1) % 16;
        nextStepStartMs += casioxw::stepIntervalMs (sequence);   // per-step read = live tempo/rate changes
    }
}

void SequencerPanel::timerCallback()
{
    feedLookahead();
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
    transportRow2.removeFromLeft (24);
    saveButton.setBounds (transportRow2.removeFromLeft (70));
    transportRow2.removeFromLeft (8);
    loadButton.setBounds (transportRow2.removeFromLeft (70));

    bounds.removeFromTop (8);
    auto modeRow = bounds.removeFromTop (28);
    baseButton.setBounds (modeRow.removeFromLeft (70));
    modeRow.removeFromLeft (8);
    editButton.setBounds (modeRow.removeFromLeft (110));
    modeRow.removeFromLeft (8);
    clearLocksButton.setBounds (modeRow.removeFromLeft (140));
    modeRow.removeFromLeft (24);
    shiftLeftButton.setBounds (modeRow.removeFromLeft (70));
    modeRow.removeFromLeft (8);
    shiftRightButton.setBounds (modeRow.removeFromLeft (70));

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

    // Left gutter: row labels aligned to the On / Pitch / Gate rows of every column.
    {
        auto gutter = stepRow.removeFromLeft (kLeftGutter);
        gutter.removeFromLeft (2);
        gutter.removeFromRight (4);
        gutter.removeFromTop (20 + 2);                        // skip the select-button row
        onRowLabel.setBounds (gutter.removeFromTop (20));
        pitchRowLabel.setBounds (gutter.removeFromTop (kKnobCell));
        gateRowLabel.setBounds (gutter.removeFromTop (kKnobCell));
    }

    for (int i = 0; i < 16; ++i)
    {
        auto col = stepRow.removeFromLeft (kStepWidth).reduced (3);
        stepControls[(size_t) i]->select.setBounds (col.removeFromTop (20));
        col.removeFromTop (2);
        stepControls[(size_t) i]->enabled.setBounds (col.removeFromTop (20));
        stepControls[(size_t) i]->note.setBounds (col.removeFromTop (kKnobCell));   // bigger rotary knob
        stepControls[(size_t) i]->gate.setBounds (col.removeFromTop (kKnobCell));   // gate-length knob
    }
}
