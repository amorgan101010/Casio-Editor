#include "SequencerPanel.h"

#include "EditorLookAndFeel.h"
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
    constexpr int kStepGridWidth = kStepWidth * 16;       // 16-step columns are the anchor, always leftmost
    constexpr int kRightPaneWidth = 560;                  // controls/info to the right of step columns
    constexpr int kTrackControlWidth = 430;               // drum-row controls inside right pane
    constexpr int kStepLabelWidth = 62;                   // matches drum lane label width
    constexpr int kDrumTrackRowHeight = 50;
    constexpr int kDrumLabelWidth = 70;
    constexpr int kDrumChannelWidth = 70;
    constexpr int kDrumControlGap = 6;
    constexpr int kKnobCell = 74;                         // rotary knob + text box (bigger than before)
    constexpr int kStepColumnHeight = 20 + 2 + kKnobCell + kKnobCell + kKnobCell;   // select + note + gate + velocity

    // Solarized Dark tokens (EditorColours) — same semantics as before (selected/has-locks/
    // active/idle), just pulled from the app-wide palette instead of ad hoc juce::Colours values.
    const juce::Colour kSelectedColour = EditorColours::selected;
    const juce::Colour kHasLocksColour = EditorColours::hasLocks;
    const juce::Colour kActiveStepColour = EditorColours::filledStep;
    const juce::Colour kIdleColour     = EditorColours::idleStep;

    bool isQuarterStep (int stepIndex)
    {
        return (stepIndex % 4) == 0; // 1,5,9,13 (1-based)
    }

    // The p-lockable parameters this MVP exposes, with musical base defaults (an *open* filter, not
    // a closed one at 0 — otherwise sending base cutoff on every unlocked step would mute the
    // sound the moment playback starts). Add more here to scale the lock lane.
    struct Lockable { const char* paramId; int instance; int base; };
    const Lockable kLockables[] = {
        { "tssFLTFcoff", 1, 127 },   // Total Filter Cutoff — fully open
        { "tssFLTFreso", 1, 0   },   // Total Filter Resonance — none
    };

    struct DrumTrackDef
    {
        const char* label;
        int defaultChannel;
        int defaultNote;
    };

    constexpr DrumTrackDef kDrumTracks[] = {
        { "Drum 1", 8, 36 },
        { "Drum 2", 9, 38 },
        { "Drum 3", 10, 42 },
        { "Drum 4", 11, 46 },
        { "Drum 5", 12, 49 },
    };

    struct NrpnAddress
    {
        int msb = 0;
        int lsb = 0;
    };

    std::optional<NrpnAddress> nrpnAddressForSoloSynthParam (const casioxw::ParamInfo& info, int instance)
    {
        if (info.section != "soloSynth")
            return std::nullopt;

        if (info.block == "TotalFilter")
        {
            const int lsb = info.addr - 72;   // p79 IDs 0x48..0x5A -> NRPN LSB 0x00..0x12 on MSB 0x38
            if (lsb >= 0 && lsb <= 0x12)
                return NrpnAddress { 0x38, lsb };
        }

        if (info.block == "LFO")
        {
            const int lsb = info.addr - 91;   // p79 IDs 0x5B..0x62 -> NRPN LSB 0x00..0x07
            if (instance >= 1 && instance <= 2 && lsb >= 0 && lsb <= 0x07)
                return NrpnAddress { 0x35 + instance, lsb };   // LFO1=0x36, LFO2=0x37
        }

        if (info.block == "OSC" || info.block == "PWM" || info.block == "Etc")
        {
            if (instance >= 1 && instance <= 6 && info.addr >= 0 && info.addr <= 0x48)
                return NrpnAddress { 0x2F + instance, info.addr };   // OSC1..Noise = 0x30..0x35
        }

        return std::nullopt;
    }

    std::vector<juce::MidiMessage> buildNrpnMessages (int channel, int nrpnMsb, int nrpnLsb, int value)
    {
        const int ch = juce::jlimit (1, 16, channel);
        const int v  = juce::jlimit (0, 127, value);
        return {
            juce::MidiMessage::controllerEvent (ch, 62, nrpnLsb),   // NRPN LSB
            juce::MidiMessage::controllerEvent (ch, 63, nrpnMsb),   // NRPN MSB
            juce::MidiMessage::controllerEvent (ch, 6, v),          // Data Entry MSB
        };
    }
}

SequencerPanel::SequencerPanel (casioxw::SysExCodec& codecIn, casioxw::MidiIO& midiIOIn)
    : codec (codecIn), midiIO (midiIOIn)
{
    loadSequenceSettings();

    // ---- seed the source-of-truth sequence -------------------------------------------------
    for (auto& step : sequence.steps)
        step.velocity = 100;
    for (const auto& l : kLockables)
        sequence.lockable.push_back (casioxw::LockableParam { l.paramId, l.instance, l.base });

    // ---- step grid -------------------------------------------------------------------------
    for (int i = 0; i < 16; ++i)
    {
        auto sc = std::make_unique<StepControl>();

        sc->select.onClick = [this, i]
        {
            if (editButton.getToggleState())
                selectStep (selectedStep == i ? -1 : i);
            else
            {
                auto& step = sequence.steps[(size_t) i];
                step.enabled = ! step.enabled;
                refreshStepButtons();
                updateStatusLabel();
            }
        };
        addAndMakeVisible (sc->select);

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

        sc->velocity.setRange (1.0, 127.0, 1.0);
        sc->velocity.setValue (100.0, juce::dontSendNotification);
        sc->velocity.textFromValueFunction = [] (double v) { return juce::String ((int) v); };
        sc->velocity.onValueChange = [this, i]
        {
            sequence.steps[(size_t) i].velocity = (int) stepControls[(size_t) i]->velocity.getValue();
        };
        sc->velocity.updateText();
        sc->velocity.setTextBoxStyle (juce::Slider::TextBoxBelow, false, kStepWidth - 6, 16);
        addAndMakeVisible (sc->velocity);

        stepControls[(size_t) i] = std::move (sc);
    }

    for (auto* l : { &pitchRowLabel, &gateRowLabel, &velocityRowLabel })
    {
        l->setJustificationType (juce::Justification::centredLeft);
        l->setColour (juce::Label::textColourId, EditorColours::textMuted);
        addAndMakeVisible (*l);
    }

    auto initArrowToggle = [this] (juce::TextButton& button, const juce::String& tooltip)
    {
        button.setClickingTogglesState (true);
        button.setToggleState (true, juce::dontSendNotification);
        button.setButtonText (juce::String::fromUTF8 ("▼"));
        button.setTooltip (tooltip);
        button.onClick = [this, &button]
        {
            button.setButtonText (button.getToggleState() ? juce::String::fromUTF8 ("▼")
                                                          : juce::String::fromUTF8 ("▶"));
            resized();
            repaint();
        };
        addAndMakeVisible (button);
    };
    initArrowToggle (drumControlsButton, "Toggle drum controls");
    initArrowToggle (synthControlsButton, "Toggle synth controls");

    // ---- transport -------------------------------------------------------------------------
    playStopButton.onClick = [this] { playing ? stop() : play(); };
    addAndMakeVisible (playStopButton);

    randomizeButton.onClick = [this] { randomizeSequence(); };
    addAndMakeVisible (randomizeButton);

    saveButton.onClick = [this] { saveSequenceToFile(); };
    loadButton.onClick = [this] { loadSequenceFromFile(); };
    sequenceDirButton.onClick = [this] { chooseSequenceDirectory(); };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (sequenceDirButton);

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

    // ---- mode row --------------------------------------------------------------------------
    baseButton.onClick = [this] { selectStep (-1); };
    addAndMakeVisible (baseButton);

    editButton.setClickingTogglesState (true);
    editButton.setButtonText ("Step Activate");
    editButton.onClick = [this]
    {
        const bool pLockMode = editButton.getToggleState();
        editButton.setButtonText (pLockMode ? "P-Lock Edit" : "Step Activate");
        if (! pLockMode)
        {
            selectStep (-1);
            clearDrumSelections();
            refreshStepButtons();
            updateStatusLabel();
            updateClearLocksEnabled();
        }
        else
        {
            refreshParamControls();
            updateStatusLabel();
            refreshStepButtons();
            updateClearLocksEnabled();
        }
    };
    addAndMakeVisible (editButton);

    muteSynthButton.setClickingTogglesState (true);
    addAndMakeVisible (muteSynthButton);

    clearLocksButton.onClick = [this]
    {
        if (selectedStep >= 0 || hasAnyDrumStepSelected())
        {
            if (selectedStep >= 0)
                casioxw::clearStepLocks (sequence, selectedStep);
            for (auto& row : drumTrackControls)
                if (row != nullptr)
                    if (row->selectedStep >= 0)
                        row->velocityLocks[(size_t) row->selectedStep].reset();
            refreshParamControls();
            refreshStepButtons();
            updateClearLocksEnabled();
        }
    };
    addAndMakeVisible (clearLocksButton);

    shiftLeftButton.onClick  = [this] { casioxw::shiftSteps (sequence, -1); syncStepWidgetsFromSequence(); refreshParamControls(); refreshStepButtons(); };
    shiftRightButton.onClick = [this] { casioxw::shiftSteps (sequence,  1); syncStepWidgetsFromSequence(); refreshParamControls(); refreshStepButtons(); };
    addAndMakeVisible (shiftLeftButton);
    addAndMakeVisible (shiftRightButton);

    statusLabel.setColour (juce::Label::textColourId, EditorColours::textHeader);
    addAndMakeVisible (statusLabel);

    // ---- drum-track controls (5 lanes, each with channel + note + 16 step on/off + velocity) ---
    drumTracksLabel.setColour (juce::Label::textColourId, EditorColours::textHeader);
    drumTracksLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (drumTracksLabel);

    const int noteMin = 0;
    const int noteMax = 127;

    for (size_t i = 0; i < std::size (kDrumTracks); ++i)
    {
        const auto& def = kDrumTracks[i];
        auto row = std::make_unique<DrumTrackControl>();
        auto* rowPtr = row.get();

        row->trackLabel.setText (def.label, juce::dontSendNotification);
        row->trackLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (row->trackLabel);

        row->mute.setClickingTogglesState (true);
        addAndMakeVisible (row->mute);

        for (int ch = 1; ch <= 16; ++ch)
            row->channel.addItem (juce::String (ch), ch);
        row->channel.setSelectedId (def.defaultChannel, juce::dontSendNotification);
        addAndMakeVisible (row->channel);

        row->note.setRange ((double) noteMin, (double) noteMax, 1.0);
        row->note.setValue ((double) juce::jlimit (noteMin, noteMax, def.defaultNote), juce::dontSendNotification);
        row->note.textFromValueFunction = [] (double v) { return casioxw::midiNoteName ((int) v); };
        row->note.valueFromTextFunction = [] (const juce::String& t) -> double
        {
            const auto n = casioxw::noteNameToMidi (t);
            return n.has_value() ? (double) *n : 0.0;
        };
        row->note.updateText();
        addAndMakeVisible (row->note);

        row->velocity.setRange (1.0, 127.0, 1.0);
        row->velocity.setValue (100.0, juce::dontSendNotification);
        row->velocity.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 20);
        row->velocity.textFromValueFunction = [] (double v) { return juce::String ((int) v); };
        row->velocity.onValueChange = [this, rowPtr]
        {
            const int v = juce::jlimit (1, 127, (int) rowPtr->velocity.getValue());
            if (editButton.getToggleState() && rowPtr->selectedStep >= 0)
                rowPtr->velocityLocks[(size_t) rowPtr->selectedStep] = v;
            else
                rowPtr->baseVelocity = v;

            refreshStepButtons();
        };
        row->velocityMarker.setJustificationType (juce::Justification::centredLeft);
        row->velocityMarker.setColour (juce::Label::textColourId, EditorColours::textMuted);
        addAndMakeVisible (row->velocity);
        addAndMakeVisible (row->velocityMarker);

        for (size_t step = 0; step < row->steps.size(); ++step)
        {
            auto& b = row->steps[step];
            b.setClickingTogglesState (false);
            b.setToggleState (false, juce::dontSendNotification);
            b.setButtonText (juce::String ((int) step + 1));
            b.setColour (juce::TextButton::buttonOnColourId, kSelectedColour);
            b.onClick = [this, rowPtr, step]
            {
                if (editButton.getToggleState())
                {
                    rowPtr->selectedStep = (rowPtr->selectedStep == (int) step ? -1 : (int) step);
                    if (rowPtr->selectedStep >= 0)
                        selectedStep = -1; // synth and drum step edit targets are mutually exclusive
                    refreshStepButtons();
                    updateStatusLabel();
                    updateClearLocksEnabled();
                }
                else
                {
                    const bool next = ! rowPtr->steps[step].getToggleState();
                    rowPtr->steps[step].setToggleState (next, juce::dontSendNotification);
                    refreshStepButtons();
                    updateStatusLabel();
                }
            };
            addAndMakeVisible (b);
        }

        drumTrackControls[i] = std::move (row);
    }

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

    setSize (kStepGridWidth + 8 + kRightPaneWidth + 16,
             8 + 6 + 28 + 6 + 28 + 8 + 20 + 6 + 28 + 6 + 20 + (int) std::size (kDrumTracks) * (kDrumTrackRowHeight + 2)
                 + 10 + kStepColumnHeight + 8);

    selectStep (-1);   // start in Base mode
    clearDrumSelections();
    resized();
}

SequencerPanel::~SequencerPanel()
{
    stop();
}

void SequencerPanel::paint (juce::Graphics& g)
{
    g.fillAll (EditorColours::chassisBg);

    if (! playheadLaneBounds.isEmpty())
    {
        g.setColour (EditorColours::textHeader.withAlpha (0.55f));
        for (int step = 0; step < 16; ++step)
        {
            if (! isQuarterStep (step))
                continue;
            auto col = playheadLaneBounds.withX (playheadLaneBounds.getX() + step * kStepWidth)
                                        .withWidth (kStepWidth)
                                        .reduced (1, 0);
            g.drawRoundedRectangle (col.toFloat(), 4.0f, 2.4f);
        }
    }

    if (playheadStep < 0 || playheadLaneBounds.isEmpty())
        return;

    const int clamped = juce::jlimit (0, 15, playheadStep);
    auto column = playheadLaneBounds.withX (playheadLaneBounds.getX() + clamped * kStepWidth)
                                    .withWidth (kStepWidth)
                                    .reduced (2, 0);
    // Cyan, not the amber "selected" hue -- playback position and edit focus are different facts
    // and shouldn't share a colour when a step happens to be both at once.
    g.setColour (EditorColours::playhead.withAlpha (0.28f));
    g.fillRoundedRectangle (column.toFloat(), 4.0f);
}

std::vector<juce::MidiMessage> SequencerPanel::paramMessages (const juce::String& paramId,
                                                             int instance, int value, int channel) const
{
    // Prefer NRPN on the realtime path to cut wire traffic and synth-side parse overhead.
    if (const auto* info = codec.model().find (paramId))
        if (const auto nrpn = nrpnAddressForSoloSynthParam (*info, instance))
            if (value >= 0 && value <= 127)   // this path currently handles plain 7-bit ranges
                return buildNrpnMessages (channel, nrpn->msb, nrpn->lsb, value);

    // Fallback for anything unmapped/non-7-bit: keep the proven SysEx path.
    const auto frame = codec.encode (paramId, instance, value);
    if (frame.size() < 3)
        return {};
    // createSysExMessage() re-adds its own F0/F7, so pass only the bytes between them (as sendFrame).
    return { juce::MidiMessage::createSysExMessage (frame.data() + 1, (int) frame.size() - 2) };
}

void SequencerPanel::sendParamNow (const juce::String& paramId, int instance, int value)
{
    for (const auto& m : paramMessages (paramId, instance, value, sequence.channel))
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
        sc.note.setValue ((double) sequence.steps[(size_t) i].note, juce::dontSendNotification);
        sc.note.updateText();
        sc.gate.setValue ((double) sequence.steps[(size_t) i].gatePercent, juce::dontSendNotification);
        sc.gate.updateText();
        sc.velocity.setValue ((double) sequence.steps[(size_t) i].velocity, juce::dontSendNotification);
        sc.velocity.updateText();
    }
}

void SequencerPanel::syncTransportWidgetsFromSequence()
{
    tempoSlider.setValue ((double) sequence.tempoBpm, juce::dontSendNotification);
    channelSlider.setValue ((double) sequence.channel, juce::dontSendNotification);
    rateCombo.setSelectedId (sequence.stepsPerBeat, juce::dontSendNotification);
}

void SequencerPanel::saveSequenceToFile()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Save Solo Sequence (.xwseq)");
    menu.addItem (2, "Save Drum Sequence (.xwdrm)");
    menu.addItem (3, "Save Sequence Set (.xwset)");
    menu.showMenuAsync (juce::PopupMenu::Options(),
                        [this] (int result)
                        {
                            if (result == 1) saveByKind (SaveKind::solo);
                            if (result == 2) saveByKind (SaveKind::drums);
                            if (result == 3) saveByKind (SaveKind::sequenceSet);
                        });
}

void SequencerPanel::loadSequenceFromFile()
{
    const auto fallbackDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    auto baseDir = sequenceDefaultDirectory;
    if (baseDir.existsAsFile())
        baseDir = baseDir.getParentDirectory();
    if (! baseDir.isDirectory())
        baseDir = fallbackDir;
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load sequence",
        baseDir,
        "*.xwseq;*.xwdrm;*.xwset",
        false);

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (! file.existsAsFile())
                return;   // cancelled
            applyLoadedText (file.loadFileAsString(), file);
        });
}

void SequencerPanel::saveByKind (SaveKind kind)
{
    const auto fallbackDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    auto baseDir = sequenceDefaultDirectory;
    if (baseDir.existsAsFile())
        baseDir = baseDir.getParentDirectory();
    if (! baseDir.isDirectory())
        baseDir = fallbackDir;

    juce::String fileName;
    juce::String wildcard;
    juce::String payload;

    if (kind == SaveKind::solo)
    {
        fileName = "solo-sequence.xwseq";
        wildcard = "*.xwseq";
        payload = casioxw::sequenceToJson (sequence);
    }
    else if (kind == SaveKind::drums)
    {
        fileName = "drum-sequence.xwdrm";
        wildcard = "*.xwdrm";
        payload = serializeDrumsToJson();
    }
    else
    {
        fileName = "sequence-set.xwset";
        wildcard = "*.xwset";
    }

    fileChooser = std::make_unique<juce::FileChooser> (
        "Save sequence",
        baseDir.getChildFile (fileName),
        wildcard,
        false);

    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, kind, payload] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File())
                return; // cancelled

            const juce::String ext = kind == SaveKind::solo ? ".xwseq" : (kind == SaveKind::drums ? ".xwdrm" : ".xwset");
            if (! file.hasFileExtension (ext.substring (1)))
                file = file.withFileExtension (ext.substring (1));

            if (kind == SaveKind::sequenceSet)
            {
                const auto baseName = file.getFileNameWithoutExtension();
                const auto soloName = baseName + ".solo.xwseq";
                const auto drumName = baseName + ".drums.xwdrm";
                const auto soloFile = file.getSiblingFile (soloName);
                const auto drumFile = file.getSiblingFile (drumName);
                const auto setPayload = serializeSequenceSetToJson (soloName, drumName);

                const bool okSolo = soloFile.replaceWithText (casioxw::sequenceToJson (sequence));
                const bool okDrums = drumFile.replaceWithText (serializeDrumsToJson());
                const bool okSet = file.replaceWithText (setPayload);

                if (okSolo && okDrums && okSet)
                    statusLabel.setText ("Saved set + refs: " + file.getFileName(), juce::dontSendNotification);
                else
                    statusLabel.setText ("Save failed: " + file.getFullPathName(), juce::dontSendNotification);
            }
            else
            {
                if (file.replaceWithText (payload))
                    statusLabel.setText ("Saved " + file.getFileName(), juce::dontSendNotification);
                else
                    statusLabel.setText ("Save failed: " + file.getFullPathName(), juce::dontSendNotification);
            }
        });
}

juce::String SequencerPanel::serializeDrumsToJson() const
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("format", "casioxw-drum-sequence");
    root->setProperty ("version", 1);
    root->setProperty ("tempoBpm", sequence.tempoBpm);
    root->setProperty ("stepsPerBeat", sequence.stepsPerBeat);

    juce::Array<juce::var> tracks;
    for (const auto& row : drumTrackControls)
    {
        if (row == nullptr)
            continue;

        juce::DynamicObject::Ptr t = new juce::DynamicObject();
        t->setProperty ("channel", row->channel.getSelectedId());
        t->setProperty ("note", (int) row->note.getValue());
        t->setProperty ("baseVelocity", row->baseVelocity);

        juce::Array<juce::var> steps;
        juce::Array<juce::var> locks;
        for (int i = 0; i < 16; ++i)
        {
            steps.add (row->steps[(size_t) i].getToggleState());
            if (const auto v = row->velocityLocks[(size_t) i])
                locks.add (*v);
            else
                locks.add (juce::var());
        }
        t->setProperty ("steps", steps);
        t->setProperty ("velocityLocks", locks);
        tracks.add (juce::var (t.get()));
    }
    root->setProperty ("tracks", tracks);
    return juce::JSON::toString (juce::var (root.get()));
}

juce::String SequencerPanel::serializeSequenceSetToJson (const juce::String& soloFile, const juce::String& drumsFile) const
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("format", "casioxw-sequence-set-ref");
    root->setProperty ("version", 1);
    root->setProperty ("soloFile", soloFile);
    root->setProperty ("drumsFile", drumsFile);
    // Keep inline copies too so old/new loaders can still recover even if sidecars are moved.
    root->setProperty ("solo", juce::JSON::parse (casioxw::sequenceToJson (sequence)));
    root->setProperty ("drums", juce::JSON::parse (serializeDrumsToJson()));
    return juce::JSON::toString (juce::var (root.get()));
}

bool SequencerPanel::applySoloSequenceText (const juce::String& text)
{
    const auto loaded = casioxw::sequenceFromJson (text);
    if (! loaded.has_value())
        return false;

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

    return true;
}

bool SequencerPanel::applyDrumSequenceText (const juce::String& text)
{
    const auto parsed = juce::JSON::parse (text);
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr || obj->getProperty ("format").toString() != "casioxw-drum-sequence")
        return false;

    const auto tempoVar = obj->getProperty ("tempoBpm");
    if (! tempoVar.isVoid())
        sequence.tempoBpm = (int) tempoVar;
    const auto rateVar = obj->getProperty ("stepsPerBeat");
    if (! rateVar.isVoid())
        sequence.stepsPerBeat = (int) rateVar;

    const auto tracks = obj->getProperty ("tracks").getArray();
    if (tracks == nullptr)
        return false;

    const int n = juce::jmin ((int) drumTrackControls.size(), tracks->size());
    for (int i = 0; i < n; ++i)
    {
        auto* t = (*tracks)[i].getDynamicObject();
        if (t == nullptr || drumTrackControls[(size_t) i] == nullptr)
            continue;

        auto& row = *drumTrackControls[(size_t) i];
        const auto chVar = t->getProperty ("channel");
        const auto noteVar = t->getProperty ("note");
        const auto baseVar = t->getProperty ("baseVelocity");
        row.channel.setSelectedId (chVar.isVoid() ? row.channel.getSelectedId() : (int) chVar, juce::dontSendNotification);
        row.note.setValue ((double) (noteVar.isVoid() ? (int) row.note.getValue() : (int) noteVar), juce::dontSendNotification);
        row.baseVelocity = juce::jlimit (1, 127, baseVar.isVoid() ? row.baseVelocity : (int) baseVar);
        row.selectedStep = -1;
        row.velocityLocks.fill (std::nullopt);
        for (int s = 0; s < 16; ++s)
            row.steps[(size_t) s].setToggleState (false, juce::dontSendNotification);

        if (const auto* steps = t->getProperty ("steps").getArray())
        {
            const int stepCount = juce::jmin (16, steps->size());
            for (int s = 0; s < stepCount; ++s)
                row.steps[(size_t) s].setToggleState ((bool) steps->getReference (s), juce::dontSendNotification);
        }

        if (const auto* locks = t->getProperty ("velocityLocks").getArray())
        {
            const int lockCount = juce::jmin (16, locks->size());
            for (int s = 0; s < lockCount; ++s)
            {
                const auto& v = locks->getReference (s);
                row.velocityLocks[(size_t) s] = v.isVoid() ? std::optional<int>() : std::optional<int> ((int) v);
            }
        }
    }
    return true;
}

bool SequencerPanel::applyLoadedText (const juce::String& text, const juce::File& sourceFile)
{
    const auto parsed = juce::JSON::parse (text);
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
    {
        statusLabel.setText ("Load failed: " + sourceFile.getFileName() + " is not valid JSON",
                             juce::dontSendNotification);
        return false;
    }

    const auto format = obj->getProperty ("format").toString();

    bool ok = false;
    if (format == "casioxw-sequence")
    {
        ok = applySoloSequenceText (text);
    }
    else if (format == "casioxw-drum-sequence")
    {
        ok = applyDrumSequenceText (text);
    }
    else if (format == "casioxw-sequence-set-ref" || format == "casioxw-sequence-set")
    {
        ok = true;

        const auto soloRef = obj->getProperty ("soloFile").toString();
        const auto drumsRef = obj->getProperty ("drumsFile").toString();
        if (soloRef.isNotEmpty() && drumsRef.isNotEmpty())
        {
            const auto dir = sourceFile.getParentDirectory();
            const auto soloFile = dir.getChildFile (soloRef);
            const auto drumsFile = dir.getChildFile (drumsRef);
            if (soloFile.existsAsFile() && drumsFile.existsAsFile())
            {
                ok = applySoloSequenceText (soloFile.loadFileAsString()) && ok;
                ok = applyDrumSequenceText (drumsFile.loadFileAsString()) && ok;
            }
            else
            {
                ok = false;
            }
        }
        else
        {
            ok = false;
        }

        // Fallback to embedded content for older/moved set files.
        if (! ok)
        {
            ok = true;
            const auto solo = obj->getProperty ("solo");
            if (solo.getDynamicObject() != nullptr)
                ok = applySoloSequenceText (juce::JSON::toString (solo)) && ok;
            else
                ok = false;
            const auto drums = obj->getProperty ("drums");
            if (drums.getDynamicObject() != nullptr)
                ok = applyDrumSequenceText (juce::JSON::toString (drums)) && ok;
            else
                ok = false;
        }
    }
    else
    {
        // Backward compatibility for very early files that may have lacked `format`.
        ok = applySoloSequenceText (text);
    }

    if (! ok)
    {
        statusLabel.setText ("Load failed: unsupported or invalid sequence file " + sourceFile.getFileName(),
                             juce::dontSendNotification);
        return false;
    }

    syncStepWidgetsFromSequence();
    syncTransportWidgetsFromSequence();
    selectStep (-1);   // back to Base; also refreshes param controls, step markers, status
    clearDrumSelections();
    statusLabel.setText ("Loaded " + sourceFile.getFileName(), juce::dontSendNotification);
    return true;
}

void SequencerPanel::chooseSequenceDirectory()
{
    const auto fallbackDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    auto baseDir = sequenceDefaultDirectory;
    if (baseDir.existsAsFile())
        baseDir = baseDir.getParentDirectory();
    if (! baseDir.isDirectory())
        baseDir = fallbackDir;

    fileChooser = std::make_unique<juce::FileChooser> (
        "Choose sequence folder",
        baseDir,
        "*",
        false);

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto picked = fc.getResult();
            if (picked == juce::File())
                return;

            auto dir = picked.isDirectory() ? picked : picked.getParentDirectory();
            if (! dir.isDirectory())
            {
                statusLabel.setText ("Sequence folder selection failed", juce::dontSendNotification);
                return;
            }

            sequenceDefaultDirectory = dir.getFullPathName();
            saveSequenceSettings();
            statusLabel.setText ("Sequence folder: " + dir.getFullPathName(), juce::dontSendNotification);
        });
}

juce::File SequencerPanel::settingsFilePath() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                    .getChildFile ("CasioXWEditor");
    dir.createDirectory();
    return dir.getChildFile ("sequencer-settings.json");
}

void SequencerPanel::loadSequenceSettings()
{
    sequenceDefaultDirectory = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    const auto settingsFile = settingsFilePath();
    if (! settingsFile.existsAsFile())
        return;

    const auto parsed = juce::JSON::parse (settingsFile.loadFileAsString());
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return;

    const auto path = obj->getProperty ("sequenceDefaultDirectory").toString();
    if (path.isNotEmpty())
    {
        juce::File configured (path);
        if (configured.isDirectory())
            sequenceDefaultDirectory = configured;
    }
}

void SequencerPanel::saveSequenceSettings() const
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("sequenceDefaultDirectory", sequenceDefaultDirectory.getFullPathName());
    settingsFilePath().replaceWithText (juce::JSON::toString (juce::var (root.get())));
}

bool SequencerPanel::hasAnyDrumStepSelected() const
{
    for (const auto& row : drumTrackControls)
        if (row != nullptr && row->selectedStep >= 0)
            return true;
    return false;
}

void SequencerPanel::clearDrumSelections()
{
    for (auto& row : drumTrackControls)
        if (row != nullptr)
            row->selectedStep = -1;
}

void SequencerPanel::updateClearLocksEnabled()
{
    clearLocksButton.setEnabled (editButton.getToggleState() && (selectedStep >= 0 || hasAnyDrumStepSelected()));
}

void SequencerPanel::selectStep (int step)
{
    selectedStep = step;
    if (step >= 0)
        clearDrumSelections();
    updateClearLocksEnabled();
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
    const bool pLockMode = editButton.getToggleState();
    const bool synthStepMode = pLockMode && selectedStep >= 0;
    const bool baseMode = ! synthStepMode;
    const bool editable = true;

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

        const bool locked = synthStepMode
            && casioxw::findStepLock (sequence.steps[(size_t) selectedStep], lp.paramId, lp.instance) != nullptr;

        if (baseMode)
            row.lockMarker.setText ("base value", juce::dontSendNotification);
        else
            row.lockMarker.setText (locked ? "LOCKED" : "(inherits base)", juce::dontSendNotification);

        row.lockMarker.setColour (juce::Label::textColourId,
                                  locked ? kSelectedColour : EditorColours::textMuted);
    }
}

void SequencerPanel::refreshStepButtons()
{
    const bool pLockMode = editButton.getToggleState();

    for (int i = 0; i < 16; ++i)
    {
        const bool hasLocks = ! sequence.steps[(size_t) i].locks.empty();
        const bool enabled = sequence.steps[(size_t) i].enabled;
        auto& btn = stepControls[(size_t) i]->select;
        btn.setButtonText (juce::String (i + 1) + (hasLocks ? " *" : ""));

        if (pLockMode)
        {
            btn.setColour (juce::TextButton::buttonColourId,
                           i == selectedStep ? kSelectedColour
                                             : (hasLocks ? kHasLocksColour
                                                         : (enabled ? kActiveStepColour : kIdleColour)));
        }
        else
        {
            btn.setColour (juce::TextButton::buttonColourId,
                           enabled ? kActiveStepColour : kIdleColour);
        }
    }
    baseButton.setColour (juce::TextButton::buttonColourId,
                          selectedStep < 0 ? kSelectedColour : kIdleColour);

    for (auto& rowPtr : drumTrackControls)
    {
        if (rowPtr == nullptr)
            continue;

        auto& row = *rowPtr;
        const bool drumStepMode = pLockMode && row.selectedStep >= 0;
        int velocity = row.baseVelocity;
        bool locked = false;
        if (drumStepMode)
            if (const auto v = row.velocityLocks[(size_t) row.selectedStep])
            {
                velocity = *v;
                locked = true;
            }

        row.velocity.setValue ((double) velocity, juce::dontSendNotification);
        if (drumStepMode)
            row.velocityMarker.setText (locked ? "LOCKED" : "(inherits base)", juce::dontSendNotification);
        else
            row.velocityMarker.setText ("base value", juce::dontSendNotification);
        row.velocityMarker.setColour (juce::Label::textColourId, locked ? kSelectedColour : EditorColours::textMuted);

        for (int i = 0; i < 16; ++i)
        {
            const bool enabled = row.steps[(size_t) i].getToggleState();
            const bool hasLock = row.velocityLocks[(size_t) i].has_value();
            auto& btn = row.steps[(size_t) i];
            btn.setButtonText (juce::String (i + 1) + (hasLock ? " *" : ""));

            if (pLockMode)
            {
                btn.setColour (juce::TextButton::buttonColourId,
                               i == row.selectedStep ? kSelectedColour
                                                     : (hasLock ? kHasLocksColour
                                                                : (enabled ? kActiveStepColour : kIdleColour)));
            }
            else
            {
                btn.setColour (juce::TextButton::buttonColourId,
                               enabled ? kActiveStepColour : kIdleColour);
            }
        }
    }
}

void SequencerPanel::updateStatusLabel()
{
    juce::String text;
    if (! editButton.getToggleState())
        text = "Step Activate mode - click step buttons to toggle synth triggers on/off";
    else if (selectedStep < 0)
    {
        bool hasDrumTarget = false;
        for (size_t i = 0; i < drumTrackControls.size(); ++i)
        {
            const auto& row = drumTrackControls[i];
            if (row != nullptr && row->selectedStep >= 0)
            {
                text = "P-Lock Edit mode - editing Drum " + juce::String ((int) i + 1)
                     + " step " + juce::String (row->selectedStep + 1) + " velocity";
                hasDrumTarget = true;
                break;
            }
        }
        if (! hasDrumTarget)
            text = "Editing BASE sound - applies to every unlocked step";
    }
    else
        text = "P-Lock Edit mode - editing step " + juce::String (selectedStep + 1) + " locks";

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
    updatePlayheadStep();
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
    for (const auto& row : drumTrackControls)
        if (row != nullptr)
            midiIO.sendAllNotesOff (juce::jlimit (1, 16, row->channel.getSelectedId()));

    // Reset every parameter to its base so a p-lock can't leave the filter stuck closed/resonant.
    for (const auto& lp : sequence.lockable)
        sendParamNow (lp.paramId, lp.instance, lp.baseValue);

    updatePlayheadStep();
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
        const double stepMs = casioxw::stepIntervalMs (sequence);
        const double drumGateMs = juce::jmax (1.0, stepMs * 0.5);
        if (! muteSynthButton.getToggleState())
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
                        for (const auto& m : paramMessages (e.paramId, e.instance, e.value, sequence.channel))
                            buffer.addEvent (m, samplePos);
                        break;
                }
            }

        for (const auto& row : drumTrackControls)
        {
            if (row == nullptr || row->mute.getToggleState() || ! row->steps[(size_t) nextStepIndex].getToggleState())
                continue;

            const int note = juce::jlimit (0, 127, (int) row->note.getValue());
            int velocity = row->baseVelocity;
            if (const auto locked = row->velocityLocks[(size_t) nextStepIndex])
                velocity = *locked;
            velocity = juce::jlimit (1, 127, velocity);
            const int channel = juce::jlimit (1, 16, row->channel.getSelectedId() > 0
                                                         ? row->channel.getSelectedId()
                                                         : sequence.channel);
            const int onPos = (int) std::llround (nextStepStartMs);
            const int offPos = (int) std::llround (nextStepStartMs + drumGateMs);
            buffer.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) velocity), onPos);
            buffer.addEvent (juce::MidiMessage::noteOff (channel, note), offPos);
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
    updatePlayheadStep();
}

void SequencerPanel::updatePlayheadStep()
{
    int nextPlayhead = -1;
    if (playing)
    {
        const double stepMs = casioxw::stepIntervalMs (sequence);
        if (stepMs > 0.0)
        {
            const double now = (double) juce::Time::getMillisecondCounter();
            const double elapsed = now - transportStartMs;
            if (elapsed >= 0.0)
                nextPlayhead = (int) std::floor (elapsed / stepMs) % 16;
        }
    }

    if (nextPlayhead == playheadStep)
        return;

    playheadStep = nextPlayhead;
    repaint (playheadLaneBounds);
}

void SequencerPanel::resized()
{
    auto bounds = getLocalBounds().reduced (8);

    bounds.removeFromTop (6);
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
    transportRow2.removeFromLeft (24);
    saveButton.setBounds (transportRow2.removeFromLeft (80));
    transportRow2.removeFromLeft (8);
    loadButton.setBounds (transportRow2.removeFromLeft (80));
    transportRow2.removeFromLeft (8);
    sequenceDirButton.setBounds (transportRow2.removeFromLeft (84));

    bounds.removeFromTop (8);
    statusLabel.setBounds (bounds.removeFromTop (20));

    bounds.removeFromTop (6);
    auto globalModeRow = bounds.removeFromTop (28);
    editButton.setBounds (globalModeRow.removeFromLeft (110));
    globalModeRow.removeFromLeft (8);
    clearLocksButton.setBounds (globalModeRow.removeFromLeft (140));

    const bool showDrumControls = drumControlsButton.getToggleState();
    const bool showSynthControls = synthControlsButton.getToggleState();

    bounds.removeFromTop (6);
    auto drumHeader = bounds.removeFromTop (20);
    drumTracksLabel.setBounds (drumHeader.removeFromLeft (kStepGridWidth));
    drumHeader.removeFromLeft (8);
    drumControlsButton.setBounds (drumHeader.removeFromLeft (22));
    const int playheadTop = bounds.getY();
    for (const auto& row : drumTrackControls)
    {
        if (row == nullptr)
            continue;
        auto r = bounds.removeFromTop (kDrumTrackRowHeight);
        auto stepCells = r.removeFromLeft (kStepGridWidth);
        if (showDrumControls)
        {
            r.removeFromLeft (8);
            auto controls = r.removeFromLeft (kTrackControlWidth);
            row->trackLabel.setBounds (controls.removeFromLeft (62));
            controls.removeFromLeft (4);
            row->mute.setBounds (controls.removeFromLeft (56));
            controls.removeFromLeft (kDrumControlGap);
            row->channel.setBounds (controls.removeFromLeft (kDrumChannelWidth));
            controls.removeFromLeft (kDrumControlGap);
            auto noteVel = controls;
            row->note.setBounds (noteVel.removeFromTop (22));
            noteVel.removeFromTop (4);
            auto velRow = noteVel.removeFromTop (22);
            row->velocityMarker.setBounds (velRow.removeFromRight (92));
            row->velocity.setBounds (velRow);
        }
        else
        {
            row->trackLabel.setBounds (0, 0, 0, 0);
            row->mute.setBounds (0, 0, 0, 0);
            row->channel.setBounds (0, 0, 0, 0);
            row->note.setBounds (0, 0, 0, 0);
            row->velocity.setBounds (0, 0, 0, 0);
            row->velocityMarker.setBounds (0, 0, 0, 0);
        }

        for (auto& b : row->steps)
        {
            auto cell = stepCells.removeFromLeft (kStepWidth);
            b.setBounds (cell.removeFromTop (20).reduced (3, 0));
        }

        bounds.removeFromTop (2);
    }

    bounds.removeFromTop (10);
    auto stepRow = bounds.removeFromTop (kStepColumnHeight);
    const int playheadColumnsX = stepRow.getX();
    const int playheadBottom = stepRow.getBottom();

    auto stepCols = stepRow.removeFromLeft (kStepGridWidth);
    stepRow.removeFromLeft (8);
    auto rightPane = stepRow;

    // Right pane labels aligned to the Pitch / Gate / Velocity rows of every column.
    if (showSynthControls)
    {
        auto labels = rightPane.removeFromLeft (kStepLabelWidth);
        labels.removeFromTop (20 + 2);                        // skip the select-button row
        pitchRowLabel.setBounds (labels.removeFromTop (kKnobCell));
        gateRowLabel.setBounds (labels.removeFromTop (kKnobCell));
        velocityRowLabel.setBounds (labels.removeFromTop (kKnobCell));

        auto synthControls = rightPane;
        auto modeRow1 = synthControls.removeFromTop (28);
        synthControlsButton.setBounds (modeRow1.removeFromLeft (22));
        modeRow1.removeFromLeft (6);
        baseButton.setBounds (modeRow1.removeFromLeft (70));
        modeRow1.removeFromLeft (8);
        muteSynthButton.setBounds (modeRow1.removeFromLeft (100));

        synthControls.removeFromTop (4);
        auto modeRow2 = synthControls.removeFromTop (28);
        shiftLeftButton.setBounds (modeRow2.removeFromLeft (70));
        modeRow2.removeFromLeft (8);
        shiftRightButton.setBounds (modeRow2.removeFromLeft (70));

        synthControls.removeFromTop (6);
        for (auto& row : lockRows)
        {
            auto r = synthControls.removeFromTop (28);
            const int markerWidth = 140;
            const int controlWidth = juce::jmax (180, r.getWidth() - 10 - markerWidth);
            row->control->setBounds (r.removeFromLeft (controlWidth));
            r.removeFromLeft (10);
            row->lockMarker.setBounds (r.removeFromLeft (markerWidth));
            synthControls.removeFromTop (2);
        }
    }
    else
    {
        synthControlsButton.setBounds (rightPane.removeFromLeft (22));
        baseButton.setBounds (0, 0, 0, 0);
        muteSynthButton.setBounds (0, 0, 0, 0);
        shiftLeftButton.setBounds (0, 0, 0, 0);
        shiftRightButton.setBounds (0, 0, 0, 0);
        for (auto& row : lockRows)
        {
            row->control->setBounds (0, 0, 0, 0);
            row->lockMarker.setBounds (0, 0, 0, 0);
        }
        pitchRowLabel.setBounds (0, 0, 0, 0);
        gateRowLabel.setBounds (0, 0, 0, 0);
        velocityRowLabel.setBounds (0, 0, 0, 0);
    }

    for (int i = 0; i < 16; ++i)
    {
        auto col = stepCols.removeFromLeft (kStepWidth).reduced (3);
        stepControls[(size_t) i]->select.setBounds (col.removeFromTop (20));
        col.removeFromTop (2);
        stepControls[(size_t) i]->note.setBounds (col.removeFromTop (kKnobCell));   // bigger rotary knob
        stepControls[(size_t) i]->gate.setBounds (col.removeFromTop (kKnobCell));   // gate-length knob
        stepControls[(size_t) i]->velocity.setBounds (col.removeFromTop (kKnobCell));
    }

    playheadLaneBounds = { playheadColumnsX, playheadTop, kStepGridWidth, playheadBottom - playheadTop };
}
