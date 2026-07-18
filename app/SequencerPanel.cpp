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
    constexpr int kLaneLabelWidth = 64;                   // shared label gutter: drum names + Pitch/Gate/Vel
    constexpr int kCardWidth = 470;                       // right-side control cards (drum / synth)
    constexpr int kDrumTrackRowHeight = 52;
    constexpr int kPcmTrackRowHeight = 30;                // compact: label + mute + channel only
    constexpr int kDrumKeyHeight = 34;                    // drum trig keys, tall enough to read as keys
    constexpr int kSelectKeyHeight = 24;                  // synth select/trig row
    constexpr int kKnobCell = 74;                         // rotary knob + text box (bigger than before)
    constexpr int kStepColumnHeight = kSelectKeyHeight + 2 + kKnobCell * 3;   // select + note + gate + velocity
    constexpr int kSynthSectionHeight = 306;              // fits the card (header + LCD display + page keys)
    constexpr int kToolbarRowHeight = 30;                 // transport toolbar rows (wrapping flow)
    constexpr int kFooterHeight = 18;                     // file save/load message line
    constexpr int kSectionGap = 6;

    // Solarized Dark tokens (EditorColours). Step-key state colours now live in
    // StepKeyButton::paintButton; these two remain for the Base button + lock markers.
    const juce::Colour kSelectedColour = EditorColours::selected;
    const juce::Colour kIdleColour     = EditorColours::idleStep;

    // The p-lockable parameter set, organised into ParamPageDisplay pages (Digitakt-style: one
    // page of 8 cells on screen at a time). Base defaults are musical/neutral — the sequencer
    // sends every base value when playback starts, so anything that would audibly change the
    // patch defaults to "no effect" (open filter, zero depths) rather than 0-means-silence.
    // Adding a lockable param = one row here; pages, playback, and lock UI all derive from it.
    struct Lockable { const char* paramId; int instance; int base; const char* shortName; int page; };
    const char* const kLockPageNames[] = { "FLTR", "FLT2", "ENV", "LFO" };
    const Lockable kLockables[] = {
        // FLTR — the live filter page.
        { "tssFLTFcoff",  1, 127, "CUT",  0 },   // cutoff fully open
        { "tssFLTFreso",  1, 0,   "RES",  0 },
        { "tssFLTFtype",  1, 0,   "TYP",  0 },
        { "tssFLTFEdep",  1, 0,   "EDEP", 0 },   // env depth 0 keeps the ENV page inert by default
        { "tssFLTFtch",   1, 0,   "TCH",  0 },
        { "tssFLTFkeyf",  1, 0,   "KEYF", 0 },
        { "tssFLTFlfo1D", 1, 0,   "LFO1", 0 },
        { "tssFLTFlfo2D", 1, 0,   "LFO2", 0 },
        // FLT2 — remaining TotalFilter params (rounds out the block: key-follow base note,
        // envelope init level/clock-trigger/retrigger), neutral bases matching the FLTR/ENV pages.
        // Kept next to FLTR (owner's call) rather than tacked on at the end.
        { "tssFLTFkeyfB", 1, 60,  "KF.B", 1 },   // note ref; moot while KEYF (amount) is 0
        { "tssFLTFENViL", 1, 127, "I.LV", 1 },   // matches the flat-at-max envelope shape below
        { "tssFLTFEclk",  1, 0,   "ECLK", 1 },   // Off
        { "tssFLTFErtrg", 1, 0,   "RTRG", 1 },   // off
        // ENV — filter envelope stages (flat-at-max shape until sculpted).
        { "tssFLTFENVaT",  1, 0,   "ATK",  2 },
        { "tssFLTFENVaL",  1, 127, "A.LV", 2 },
        { "tssFLTFENVdT",  1, 0,   "DEC",  2 },
        { "tssFLTFENVsL",  1, 127, "SUS",  2 },
        { "tssFLTFENVr1T", 1, 0,   "R1.T", 2 },
        { "tssFLTFENVr1L", 1, 127, "R1.L", 2 },
        { "tssFLTFENVr2T", 1, 0,   "R2.T", 2 },
        { "tssFLTFENVr2L", 1, 0,   "R2.L", 2 },
        // LFO 1 — depth 0 keeps it silent until dialled in.
        { "tssLFOwf",    1, 0,  "WAVE", 3 },
        { "tssLFOrate",  1, 64, "RATE", 3 },
        { "tssLFOdep",   1, 0,  "DEP",  3 },
        { "tssLFOdelay", 1, 0,  "DLY",  3 },
        { "tssLFOrise",  1, 0,  "RISE", 3 },
        { "tssLFOmdep",  1, 0,  "MDEP", 3 },
        { "tssLFOsync",  1, 0,  "SYNC", 3 },
        { "tssLFOclk",   1, 0,  "CLK",  3 },
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

    struct PcmTrackDef
    {
        const char* label;
        int defaultChannel;
    };

    // The Step Sequencer's remaining note parts (XWP1_1B_EN.pdf p.E-49): Drum 1-5 are parts 8-12
    // above; Bass/Solo 1/Solo 2/Chords are parts 13-16, mixer channels 13ch-16ch 1:1. Any of these
    // can hold a PCM Melody tone (or any other tone type) in a configured Performance.
    constexpr PcmTrackDef kPcmTracks[] = {
        { "Bass",   13 },
        { "Solo 1", 14 },
        { "Solo 2", 15 },
        { "Chords", 16 },
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

    // Controller numbers per reference/midi-spec.md §1.4 — the spec's tables are HEX, so NRPN
    // select is CC 0x62/0x63 (98/99 decimal), LSB first on this device, value via Data Entry CC
    // 0x06. bug-109: these were passed as DECIMAL 62/63, so every "NRPN" went to two unrelated
    // controllers and the bare Data Entry then landed on whatever RPN/NRPN pointer was active —
    // randomly rewriting params like pitch-bend range. The trailing RPN Null (0x7F/0x7F, spec:
    // "Null (deselect)") parks the pointer after each write so no stray Data Entry, ours or
    // anyone's, can ever re-hit the last-selected parameter.
    std::vector<juce::MidiMessage> buildNrpnMessages (int channel, int nrpnMsb, int nrpnLsb, int value)
    {
        const int ch = juce::jlimit (1, 16, channel);
        const int v  = juce::jlimit (0, 127, value);
        return {
            juce::MidiMessage::controllerEvent (ch, 0x62, nrpnLsb),   // NRPN LSB
            juce::MidiMessage::controllerEvent (ch, 0x63, nrpnMsb),   // NRPN MSB
            juce::MidiMessage::controllerEvent (ch, 0x06, v),         // Data Entry MSB
            juce::MidiMessage::controllerEvent (ch, 0x64, 0x7F),      // RPN LSB \ Null — deselect
            juce::MidiMessage::controllerEvent (ch, 0x65, 0x7F),      // RPN MSB /
        };
    }
}

//==============================================================================
/** The Rnd options call-out: edits the panel's RandomizeOptions (and the combo-params flag)
    in place — no apply step, the next Rnd click uses whatever this shows. */
class RandomizeOptionsComponent : public juce::Component
{
public:
    RandomizeOptionsComponent (casioxw::RandomizeOptions& optionsIn, bool& randomizeCombosIn)
        : options (optionsIn), randomizeCombos (randomizeCombosIn)
    {
        using Scale = casioxw::RandomizeOptions::Scale;

        auto initLabel = [this] (juce::Label& l, const char* text)
        {
            l.setText (text, juce::dontSendNotification);
            l.setFont (EditorFonts::header (11.0f));
            l.setColour (juce::Label::textColourId, EditorColours::textMuted);
            addAndMakeVisible (l);
        };
        initLabel (scaleLabel, "SCALE");
        initLabel (rootLabel, "ROOT");
        initLabel (rangeLabel, "NOTES");
        initLabel (trigLabel, "TRIGS");
        initLabel (lockLabel, "LOCKS");

        scaleCombo.addItem ("Minor Pentatonic", 1 + (int) Scale::minorPentatonic);
        scaleCombo.addItem ("Major Pentatonic", 1 + (int) Scale::majorPentatonic);
        scaleCombo.addItem ("Natural Minor",    1 + (int) Scale::naturalMinor);
        scaleCombo.addItem ("Major",            1 + (int) Scale::major);
        scaleCombo.addItem ("Chromatic",        1 + (int) Scale::chromatic);
        scaleCombo.setSelectedId (1 + (int) options.scale, juce::dontSendNotification);
        scaleCombo.onChange = [this] { options.scale = (Scale) (scaleCombo.getSelectedId() - 1); };
        addAndMakeVisible (scaleCombo);

        static const char* const noteNames[] = { "C", "C#", "D", "D#", "E", "F",
                                                 "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 12; ++i)
            rootCombo.addItem (noteNames[i], i + 1);
        rootCombo.setSelectedId (options.rootNote + 1, juce::dontSendNotification);
        rootCombo.onChange = [this] { options.rootNote = rootCombo.getSelectedId() - 1; };
        addAndMakeVisible (rootCombo);

        noteRange.setSliderStyle (juce::Slider::TwoValueHorizontal);
        noteRange.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        noteRange.setRange (0.0, 127.0, 1.0);
        noteRange.setMinAndMaxValues ((double) options.noteMin, (double) options.noteMax,
                                      juce::dontSendNotification);
        noteRange.onValueChange = [this]
        {
            options.noteMin = (int) noteRange.getMinValue();
            options.noteMax = (int) noteRange.getMaxValue();
            updateRangeReadout();
        };
        addAndMakeVisible (noteRange);
        rangeReadout.setFont (EditorFonts::mono (11.0f));
        rangeReadout.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (rangeReadout);
        updateRangeReadout();

        auto initPercent = [this] (juce::Slider& s, float& target)
        {
            s.setSliderStyle (juce::Slider::LinearHorizontal);
            s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 46, 18);
            s.setRange (0.0, 100.0, 1.0);
            s.textFromValueFunction = [] (double v) { return juce::String ((int) v) + "%"; };
            s.setValue ((double) (target * 100.0f), juce::dontSendNotification);
            s.updateText();
            float* boundTarget = &target;   // capture the object, not the (soon-dead) reference
            s.onValueChange = [&s, boundTarget] { *boundTarget = (float) (s.getValue() / 100.0); };
            addAndMakeVisible (s);
        };
        initPercent (trigDensity, options.trigDensity);
        initPercent (lockDensity, options.lockDensity);

        combosToggle.setButtonText ("Also lock combo/switch params");
        combosToggle.setToggleState (randomizeCombos, juce::dontSendNotification);
        combosToggle.onClick = [this] { randomizeCombos = combosToggle.getToggleState(); };
        addAndMakeVisible (combosToggle);

        setSize (300, 6 * kRow + 10);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (8, 5);
        auto layoutRow = [&b] (juce::Label& l, juce::Component& c)
        {
            auto r = b.removeFromTop (kRow).reduced (0, 3);
            l.setBounds (r.removeFromLeft (52));
            c.setBounds (r);
        };
        layoutRow (scaleLabel, scaleCombo);
        layoutRow (rootLabel, rootCombo);
        {
            auto r = b.removeFromTop (kRow).reduced (0, 3);
            rangeLabel.setBounds (r.removeFromLeft (52));
            rangeReadout.setBounds (r.removeFromRight (72));
            noteRange.setBounds (r);
        }
        layoutRow (trigLabel, trigDensity);
        layoutRow (lockLabel, lockDensity);
        combosToggle.setBounds (b.removeFromTop (kRow).reduced (0, 3));
    }

private:
    static constexpr int kRow = 30;

    void updateRangeReadout()
    {
        rangeReadout.setText (casioxw::midiNoteName (options.noteMin) + " - "
                                  + casioxw::midiNoteName (options.noteMax),
                              juce::dontSendNotification);
    }

    casioxw::RandomizeOptions& options;
    bool& randomizeCombos;

    juce::Label scaleLabel, rootLabel, rangeLabel, trigLabel, lockLabel, rangeReadout;
    juce::ComboBox scaleCombo, rootCombo;
    juce::Slider noteRange, trigDensity, lockDensity;
    juce::ToggleButton combosToggle;
};

//==============================================================================
void StepKeyButton::setLockState (bool hasLockIn, bool selectedIn)
{
    if (hasLock == hasLockIn && selected == selectedIn)
        return;
    hasLock = hasLockIn;
    selected = selectedIn;
    repaint();
}

void StepKeyButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isMouseDown)
{
    auto b = getLocalBounds().toFloat().reduced (1.5f);
    const bool on = getToggleState();

    auto fill = selected ? EditorColours::selected
              : on       ? EditorColours::filledStep
                         : EditorColours::idleStep;
    if (isMouseOver) fill = fill.brighter (0.08f);
    if (isMouseDown) fill = fill.brighter (0.16f);
    g.setColour (fill);
    g.fillRoundedRectangle (b, 4.0f);

    // Quarter-note steps carry a structurally thicker/brighter outline — bar orientation must
    // never depend on fill colour alone (it also encodes trig/lock/selection state).
    const bool quarter = (stepIndex % 4) == 0;
    g.setColour (quarter ? EditorColours::textHeader.withAlpha (0.6f)
                         : EditorColours::border.withAlpha (0.45f));
    g.drawRoundedRectangle (b, 4.0f, quarter ? 2.2f : 1.0f);

    g.setColour ((selected || on) ? EditorColours::base03 : EditorColours::base00);
    g.setFont (EditorFonts::mono (12.0f, true));
    g.drawText (juce::String (stepIndex + 1), getLocalBounds().translated (0, -2),
                juce::Justification::centred);
    g.fillRect (juce::Rectangle<float> (b.getCentreX() - 5.0f, b.getCentreY() + 6.0f, 10.0f, 1.8f));

    if (hasLock)
    {
        g.setColour (EditorColours::hasLocks);
        g.fillEllipse (b.getRight() - 8.5f, b.getY() + 3.0f, 5.0f, 5.0f);
    }
}

//==============================================================================
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

        sc->select.setStepIndex (i);
        sc->select.onClick = [this, i]
        {
            if (editButton.getToggleState())
                selectStep (selectedStep == i ? -1 : i);
            else
            {
                auto& step = activeMelodicTrack->steps[(size_t) i];
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
            activeMelodicTrack->steps[(size_t) i].note = (int) stepControls[(size_t) i]->note.getValue();
        };
        sc->note.setTextBoxStyle (juce::Slider::TextBoxBelow, false, kStepWidth - 6, 16);
        addAndMakeVisible (sc->note);

        sc->gate.setRange (1.0, 100.0, 1.0);
        sc->gate.setValue (90.0, juce::dontSendNotification);
        sc->gate.textFromValueFunction = [] (double v) { return juce::String ((int) v) + "%"; };
        sc->gate.onValueChange = [this, i]
        {
            activeMelodicTrack->steps[(size_t) i].gatePercent = (int) stepControls[(size_t) i]->gate.getValue();
        };
        sc->gate.updateText();
        sc->gate.setTextBoxStyle (juce::Slider::TextBoxBelow, false, kStepWidth - 6, 16);
        addAndMakeVisible (sc->gate);

        sc->velocity.setRange (1.0, 127.0, 1.0);
        sc->velocity.setValue (100.0, juce::dontSendNotification);
        sc->velocity.textFromValueFunction = [] (double v) { return juce::String ((int) v); };
        sc->velocity.onValueChange = [this, i]
        {
            activeMelodicTrack->steps[(size_t) i].velocity = (int) stepControls[(size_t) i]->velocity.getValue();
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
        l->setFont (EditorFonts::header (11.0f));
        l->setText (l->getText().toUpperCase(), juce::dontSendNotification);
        addAndMakeVisible (*l);
    }

    for (auto* l : { &tempoLabel, &rateLabel, &channelLabel })
    {
        l->setJustificationType (juce::Justification::centredRight);
        l->setColour (juce::Label::textColourId, EditorColours::textMuted);
        l->setFont (EditorFonts::header (11.0f));
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
    initArrowToggle (pcmControlsButton, "Toggle PCM track controls");
    initArrowToggle (synthControlsButton, "Toggle synth controls");

    // ---- transport -------------------------------------------------------------------------
    playStopButton.onClick = [this] { playing ? stop() : play(); };
    addAndMakeVisible (playStopButton);

    randomizeButton.onClick = [this] { randomizeSequence(); };
    addAndMakeVisible (randomizeButton);

    rndOptionsButton.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xbe"));   // small down triangle
    rndOptionsButton.setTooltip ("Randomize options (scale, note range, densities)");
    rndOptionsButton.onClick = [this] { showRandomizeOptions(); };
    addAndMakeVisible (rndOptionsButton);

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

    syncBaseButton.setTooltip ("Adopt the synth's current values as the sequence's base sound");
    syncBaseButton.onClick = [this] { syncBaseValuesFromSynth(); };
    addAndMakeVisible (syncBaseButton);

    // STEP / P-LOCK: a two-key segmented mode switch (radio pair) instead of one button whose
    // caption flips — the active mode is always the lit key, never a caption to parse.
    stepModeButton.setClickingTogglesState (true);
    stepModeButton.setRadioGroupId (0x5EC7);
    stepModeButton.setToggleState (true, juce::dontSendNotification);
    stepModeButton.onClick = [this] { setPLockMode (false); };
    addAndMakeVisible (stepModeButton);

    editButton.setClickingTogglesState (true);
    editButton.setRadioGroupId (0x5EC7);
    editButton.onClick = [this] { setPLockMode (true); };
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

    statusLabel.setColour (juce::Label::textColourId, EditorColours::textMuted);
    statusLabel.setFont (EditorFonts::mono (11.0f));
    addAndMakeVisible (statusLabel);

    // ---- drum-track controls (5 lanes, each with channel + note + 16 step on/off + velocity) ---
    for (auto* l : { &drumTracksLabel, &pcmTracksLabel, &synthLabel })
    {
        l->setColour (juce::Label::textColourId, EditorColours::textHeader);
        l->setJustificationType (juce::Justification::centredLeft);
        l->setFont (EditorFonts::header (12.0f));
        addAndMakeVisible (*l);
    }

    // Focus selector: which melodic track (Solo Synth or a PCM track) the shared step column
    // below shows/edits. synthFocusButton lives beside synthLabel; each PCM row's own focusButton
    // (wired below) shares this radio group.
    synthFocusButton.setClickingTogglesState (true);
    synthFocusButton.setRadioGroupId (0x5EC8);
    synthFocusButton.setToggleState (true, juce::dontSendNotification);
    synthFocusButton.setTooltip ("Show the Solo Synth's steps in the grid above");
    synthFocusButton.onClick = [this] { setMelodicFocus (-1); };
    addAndMakeVisible (synthFocusButton);

    const int noteMin = 0;
    const int noteMax = 127;

    for (size_t i = 0; i < std::size (kDrumTracks); ++i)
    {
        const auto& def = kDrumTracks[i];
        auto row = std::make_unique<DrumTrackControl>();
        auto* rowPtr = row.get();

        row->trackLabel.setText (def.label, juce::dontSendNotification);
        row->trackLabel.setJustificationType (juce::Justification::centredLeft);
        row->trackLabel.setColour (juce::Label::textColourId, EditorColours::textMuted);
        row->trackLabel.setFont (EditorFonts::header (11.0f));
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
            b.setStepIndex ((int) step);
            b.setClickingTogglesState (false);
            b.setToggleState (false, juce::dontSendNotification);
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

    // ---- PCM-track controls (4 melodic lanes: Bass/Solo 1/Solo 2/Chords, XWP1_1B_EN.pdf p.E-49).
    // Compact rows only (label/mute/channel) in this pass; full per-step note/gate/velocity editing
    // comes via a focus selector onto the shared step column in a follow-up chunk.
    for (size_t i = 0; i < std::size (kPcmTracks); ++i)
    {
        const auto& def = kPcmTracks[i];
        auto row = std::make_unique<PcmTrackControl>();

        for (auto& step : row->track.steps)
            step.velocity = 100;
        row->track.channel = def.defaultChannel;

        row->trackLabel.setText (def.label, juce::dontSendNotification);
        row->trackLabel.setJustificationType (juce::Justification::centredLeft);
        row->trackLabel.setColour (juce::Label::textColourId, EditorColours::textMuted);
        row->trackLabel.setFont (EditorFonts::header (11.0f));
        addAndMakeVisible (row->trackLabel);

        row->mute.setClickingTogglesState (true);
        addAndMakeVisible (row->mute);

        for (int ch = 1; ch <= 16; ++ch)
            row->channel.addItem (juce::String (ch), ch);
        row->channel.setSelectedId (def.defaultChannel, juce::dontSendNotification);
        row->channel.onChange = [rowPtr = row.get()]
        {
            rowPtr->track.channel = juce::jlimit (1, 16, rowPtr->channel.getSelectedId());
        };
        addAndMakeVisible (row->channel);

        row->focusButton.setClickingTogglesState (true);
        row->focusButton.setRadioGroupId (0x5EC8);
        row->focusButton.setTooltip (juce::String ("Show ") + def.label + "'s steps in the grid above");
        row->focusButton.onClick = [this, idx = (int) i] { setMelodicFocus (idx); };
        addAndMakeVisible (row->focusButton);

        pcmTrackControls[i] = std::move (row);
    }

    // ---- the pageable p-lock parameter display (the "screen") ------------------------------
    const auto& model = codec.model();
    paramDisplay = std::make_unique<ParamPageDisplay> (model);
    {
        std::vector<ParamPageDisplay::Page> pages;
        for (const auto* name : kLockPageNames)
            pages.push_back ({ name, {} });

        for (size_t i = 0; i < sequence.lockable.size(); ++i)
        {
            const auto& l = kLockables[i];
            const auto* info = model.find (l.paramId);
            jassert (info != nullptr);   // kLockables ids must exist in xwp1.json
            if (info == nullptr)
                continue;

            // Bound randomized locks by the parameter's real range (read from metadata so a
            // differently-ranged lockable randomizes correctly).
            sequence.lockable[i].minValue = info->range.min;
            sequence.lockable[i].maxValue = info->range.max;

            // Continuous params only for Rnd by default — a per-step random filter TYPE or LFO
            // wave is the chaos the owner vetoed; combo/toggle params opt in via the call-out.
            if (casioxw::decideControlKind (*info, l.instance) == casioxw::ControlKind::Slider)
                continuousLockables.push_back ((int) i);

            pages[(size_t) l.page].cells.push_back ({ info, l.instance, l.shortName, (int) i });
        }

        paramDisplay->setPages (std::move (pages));
        paramDisplay->onValueEdited = [this] (int index, int value) { onParamEdited (index, value); };
    }
    addAndMakeVisible (*paramDisplay);

    setSize (8 + kStepGridWidth + kSectionGap + kLaneLabelWidth + kSectionGap + kCardWidth + 8,
             8 + kToolbarRowHeight + 8 + 20 + 4
                 + (int) std::size (kDrumTracks) * (kDrumTrackRowHeight + 2)
                 + 10 + 20 + 4
                 + (int) std::size (kPcmTracks) * (kPcmTrackRowHeight + 2)
                 + 10 + juce::jmax (kStepColumnHeight, kSynthSectionHeight)
                 + 6 + kFooterHeight + 8);

    selectStep (-1);   // start in Base mode
    clearDrumSelections();
    resized();
}

SequencerPanel::~SequencerPanel()
{
    // The call-out lives on the desktop, not in this component tree — it must not outlive the
    // options/flag members its content component references.
    if (activeCallout != nullptr)
        activeCallout->dismiss();
    stop();
}

void SequencerPanel::applyPreviewDemoState()
{
    for (int i : { 0, 4, 6, 8, 12, 14 })
        sequence.steps[(size_t) i].enabled = true;
    casioxw::setStepLock (sequence, 4, "tssFLTFcoff", 1, 40);
    casioxw::setStepLock (sequence, 4, "tssFLTFreso", 1, 90);
    casioxw::setStepLock (sequence, 12, "tssLFOdep", 1, 64);

    editButton.setToggleState (true, juce::dontSendNotification);
    stepModeButton.setToggleState (false, juce::dontSendNotification);
    selectedStep = 4;
    playheadStep = 8;

    if (auto& row = drumTrackControls[0])
        for (int i : { 0, 4, 8, 12 })
            row->steps[(size_t) i].setToggleState (true, juce::dontSendNotification);
    if (auto& row = drumTrackControls[1])
    {
        row->steps[2].setToggleState (true, juce::dontSendNotification);
        row->velocityLocks[10] = 45;
    }

    syncStepWidgetsFromSequence();
    refreshParamControls();
    refreshStepButtons();
    updateStatusLabel();
    updateClearLocksEnabled();
}

void SequencerPanel::applyPcmFocusPreviewState()
{
    if (auto& row = pcmTrackControls[0])   // Bass
    {
        for (int i : { 0, 3, 6, 10 })
        {
            row->track.steps[(size_t) i].enabled = true;
            row->track.steps[(size_t) i].note = 36 + i;
        }
        row->mute.setToggleState (false, juce::dontSendNotification);
    }
    if (auto& row = pcmTrackControls[2])   // Solo 2 -- muted, to check the mute button renders distinctly
        row->mute.setToggleState (true, juce::dontSendNotification);

    setMelodicFocus (0);   // shows the shared step column now reads Bass's steps, not the Solo Synth's
}

bool SequencerPanel::verifyPcmRoundTripForPreview()
{
    if (pcmTrackControls[0] == nullptr || pcmTrackControls[1] == nullptr)
        return false;

    auto& bass = pcmTrackControls[0]->track;
    auto& solo1 = pcmTrackControls[1]->track;

    bass.channel = 13;
    for (int i = 0; i < 16; ++i)
    {
        bass.steps[(size_t) i] = { 30 + i, 90 + i, i % 3 == 0, 40 + i, {} };
        solo1.steps[(size_t) i] = { 60 - i, 20 + i, i % 2 == 0, 10 + i, {} };
    }
    solo1.channel = 14;

    const auto expectedBass  = bass;
    const auto expectedSolo1 = solo1;

    const auto json = serializePcmTracksToJson();

    // Clobber the live tracks so a false pass (comparing against unchanged data) is impossible.
    bass  = casioxw::Sequence {};
    solo1 = casioxw::Sequence {};

    if (! applyPcmTracksText (json))
        return false;

    auto stepsMatch = [] (const casioxw::Step& a, const casioxw::Step& b)
    {
        return a.note == b.note && a.velocity == b.velocity
            && a.enabled == b.enabled && a.gatePercent == b.gatePercent;
    };

    if (bass.channel != expectedBass.channel || solo1.channel != expectedSolo1.channel)
        return false;

    for (int i = 0; i < 16; ++i)
        if (! stepsMatch (bass.steps[(size_t) i], expectedBass.steps[(size_t) i])
            || ! stepsMatch (solo1.steps[(size_t) i], expectedSolo1.steps[(size_t) i]))
            return false;

    return true;
}

void SequencerPanel::paint (juce::Graphics& g)
{
    g.fillAll (EditorColours::chassisBg);

    // Right-side control cards: a surface between chassis and widget fill, so panel-coloured
    // buttons/combos still read against them. (Quarter-step cues live on the trig keys
    // themselves now — StepKeyButton paints those outlines.)
    const auto cardColour = EditorColours::chassisBg.interpolatedWith (EditorColours::panelBg, 0.55f);
    for (const auto& card : { drumCardBounds, pcmCardBounds, synthCardBounds })
    {
        if (card.isEmpty())
            continue;
        g.setColour (cardColour);
        g.fillRoundedRectangle (card.toFloat(), 8.0f);
        g.setColour (EditorColours::border.withAlpha (0.3f));
        g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 8.0f, 1.0f);
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
    auto options = randomizeOptions;
    options.lockableIndices = randomizeComboParams ? std::vector<int>{}   // empty == all eligible
                                                   : continuousLockables;
    casioxw::randomize (sequence, rng, options);
    syncStepWidgetsFromSequence();
    refreshParamControls();   // selected step's locks may have changed
    refreshStepButtons();     // has-locks markers
}

void SequencerPanel::showRandomizeOptions()
{
    auto content = std::make_unique<RandomizeOptionsComponent> (randomizeOptions, randomizeComboParams);
    activeCallout = &juce::CallOutBox::launchAsynchronously (std::move (content),
                                                             rndOptionsButton.getScreenBounds(),
                                                             nullptr);
}

void SequencerPanel::syncStepWidgetsFromSequence()
{
    // Reads from `*activeMelodicTrack`, not always `sequence` — the shared step column shows
    // whichever melodic track (Solo Synth or a focused PCM track) currently has focus.
    for (int i = 0; i < 16; ++i)
    {
        auto& sc = *stepControls[(size_t) i];
        const auto& step = activeMelodicTrack->steps[(size_t) i];
        // dontSendNotification: these are display updates, not user edits — don't fire the
        // onClick/onValueChange handlers that would write straight back into the track.
        sc.note.setValue ((double) step.note, juce::dontSendNotification);
        sc.note.updateText();
        sc.gate.setValue ((double) step.gatePercent, juce::dontSendNotification);
        sc.gate.updateText();
        sc.velocity.setValue ((double) step.velocity, juce::dontSendNotification);
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
    menu.addItem (3, "Save PCM Tracks (.xwpcm)");
    menu.addItem (4, "Save Sequence Set (.xwset)");
    menu.showMenuAsync (juce::PopupMenu::Options(),
                        [this] (int result)
                        {
                            if (result == 1) saveByKind (SaveKind::solo);
                            if (result == 2) saveByKind (SaveKind::drums);
                            if (result == 3) saveByKind (SaveKind::pcm);
                            if (result == 4) saveByKind (SaveKind::sequenceSet);
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
    else if (kind == SaveKind::pcm)
    {
        fileName = "pcm-tracks.xwpcm";
        wildcard = "*.xwpcm";
        payload = serializePcmTracksToJson();
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

            const juce::String ext = kind == SaveKind::solo ? ".xwseq"
                                     : kind == SaveKind::drums ? ".xwdrm"
                                     : kind == SaveKind::pcm ? ".xwpcm"
                                                              : ".xwset";
            if (! file.hasFileExtension (ext.substring (1)))
                file = file.withFileExtension (ext.substring (1));

            if (kind == SaveKind::sequenceSet)
            {
                const auto baseName = file.getFileNameWithoutExtension();
                const auto soloName = baseName + ".solo.xwseq";
                const auto drumName = baseName + ".drums.xwdrm";
                const auto pcmName = baseName + ".pcm.xwpcm";
                const auto soloFile = file.getSiblingFile (soloName);
                const auto drumFile = file.getSiblingFile (drumName);
                const auto pcmFile = file.getSiblingFile (pcmName);
                const auto setPayload = serializeSequenceSetToJson (soloName, drumName, pcmName);

                const bool okSolo = soloFile.replaceWithText (casioxw::sequenceToJson (sequence));
                const bool okDrums = drumFile.replaceWithText (serializeDrumsToJson());
                const bool okPcm = pcmFile.replaceWithText (serializePcmTracksToJson());
                const bool okSet = file.replaceWithText (setPayload);

                if (okSolo && okDrums && okPcm && okSet)
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

juce::String SequencerPanel::serializePcmTracksToJson() const
{
    // Each PCM track IS a casioxw::Sequence, so reuse sequenceToJson() per track instead of
    // hand-rolling per-field JSON (same round-trip guarantees as the Solo Synth save).
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("format", "casioxw-pcm-tracks");
    root->setProperty ("version", 1);

    juce::Array<juce::var> tracks;
    for (const auto& row : pcmTrackControls)
        tracks.add (row == nullptr ? juce::var()
                                    : juce::JSON::parse (casioxw::sequenceToJson (row->track)));
    root->setProperty ("tracks", tracks);
    return juce::JSON::toString (juce::var (root.get()));
}

juce::String SequencerPanel::serializeSequenceSetToJson (const juce::String& soloFile, const juce::String& drumsFile,
                                                         const juce::String& pcmFile) const
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("format", "casioxw-sequence-set-ref");
    root->setProperty ("version", 1);
    root->setProperty ("soloFile", soloFile);
    root->setProperty ("drumsFile", drumsFile);
    root->setProperty ("pcmFile", pcmFile);
    // Keep inline copies too so old/new loaders can still recover even if sidecars are moved.
    root->setProperty ("solo", juce::JSON::parse (casioxw::sequenceToJson (sequence)));
    root->setProperty ("drums", juce::JSON::parse (serializeDrumsToJson()));
    root->setProperty ("pcm", juce::JSON::parse (serializePcmTracksToJson()));
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

bool SequencerPanel::applyPcmTracksText (const juce::String& text)
{
    const auto parsed = juce::JSON::parse (text);
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr || obj->getProperty ("format").toString() != "casioxw-pcm-tracks")
        return false;

    const auto* tracks = obj->getProperty ("tracks").getArray();
    if (tracks == nullptr)
        return false;

    const int n = juce::jmin ((int) pcmTrackControls.size(), tracks->size());
    for (int i = 0; i < n; ++i)
    {
        if (pcmTrackControls[(size_t) i] == nullptr)
            continue;
        auto& row = *pcmTrackControls[(size_t) i];

        const auto& trackVar = (*tracks)[i];
        if (trackVar.getDynamicObject() == nullptr)
            continue;   // this track slot wasn't saved (older/shorter file) -- leave it as-is

        const auto loaded = casioxw::sequenceFromJson (juce::JSON::toString (trackVar));
        if (! loaded.has_value())
            continue;

        row.track.steps        = loaded->steps;
        row.track.channel      = loaded->channel;
        row.track.tempoBpm     = loaded->tempoBpm;
        row.track.stepsPerBeat = loaded->stepsPerBeat;
        row.channel.setSelectedId (juce::jlimit (1, 16, row.track.channel), juce::dontSendNotification);
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
    else if (format == "casioxw-pcm-tracks")
    {
        ok = applyPcmTracksText (text);
    }
    else if (format == "casioxw-sequence-set-ref" || format == "casioxw-sequence-set")
    {
        ok = true;

        const auto soloRef = obj->getProperty ("soloFile").toString();
        const auto drumsRef = obj->getProperty ("drumsFile").toString();
        const auto pcmRef = obj->getProperty ("pcmFile").toString();   // absent in older set files
        if (soloRef.isNotEmpty() && drumsRef.isNotEmpty())
        {
            const auto dir = sourceFile.getParentDirectory();
            const auto soloFile = dir.getChildFile (soloRef);
            const auto drumsFile = dir.getChildFile (drumsRef);
            if (soloFile.existsAsFile() && drumsFile.existsAsFile())
            {
                ok = applySoloSequenceText (soloFile.loadFileAsString()) && ok;
                ok = applyDrumSequenceText (drumsFile.loadFileAsString()) && ok;
                if (pcmRef.isNotEmpty())
                {
                    const auto pcmFile = dir.getChildFile (pcmRef);
                    if (pcmFile.existsAsFile())
                        applyPcmTracksText (pcmFile.loadFileAsString());   // optional -- absent doesn't fail the set
                }
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
            const auto pcm = obj->getProperty ("pcm");   // optional -- absent in older set files
            if (pcm.getDynamicObject() != nullptr)
                applyPcmTracksText (juce::JSON::toString (pcm));
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

void SequencerPanel::setMelodicFocus (int trackIndex)
{
    focusedTrackIndex = trackIndex;
    activeMelodicTrack = (trackIndex < 0) ? &sequence : &pcmTrackControls[(size_t) trackIndex]->track;

    const bool isSynth = (trackIndex < 0);
    static const char* const kNames[] = { "SOLO SYNTH", "BASS", "SOLO 1", "SOLO 2", "CHORDS" };
    synthLabel.setText (kNames[(size_t) (trackIndex + 1)], juce::dontSendNotification);

    // P-locks and the Base/Sync/Rnd/Shift tools only ever operate on `sequence` today (a PCM
    // track's `lockable` is empty) — disable them while a PCM track has focus so they can't fire
    // against the wrong data. Plain per-step enable/note/gate/velocity editing (STEP mode) already
    // works for whichever track is focused.
    editButton.setEnabled (isSynth);
    baseButton.setEnabled (isSynth);
    syncBaseButton.setEnabled (isSynth);
    randomizeButton.setEnabled (isSynth);
    rndOptionsButton.setEnabled (isSynth);
    shiftLeftButton.setEnabled (isSynth);
    shiftRightButton.setEnabled (isSynth);
    if (! isSynth)
    {
        stepModeButton.setToggleState (true, juce::dontSendNotification);   // clears editButton (radio group)
        setPLockMode (false);
    }

    syncStepWidgetsFromSequence();
    refreshStepButtons();
    updateStatusLabel();
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

void SequencerPanel::setPLockMode (bool pLockMode)
{
    if (! pLockMode)
    {
        selectStep (-1);       // also refreshes controls/steps/status
        clearDrumSelections();
    }
    refreshParamControls();
    refreshStepButtons();
    updateStatusLabel();
    updateClearLocksEnabled();
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

    for (size_t i = 0; i < sequence.lockable.size(); ++i)
    {
        const auto& lp = sequence.lockable[i];

        const int value = baseMode
            ? lp.baseValue
            : casioxw::effectiveParamValue (sequence, selectedStep, lp.paramId, lp.instance)
                  .value_or (lp.baseValue);

        // Inverted (amber) cell == this parameter holds a lock on the selected step.
        const bool locked = synthStepMode
            && casioxw::findStepLock (sequence.steps[(size_t) selectedStep], lp.paramId, lp.instance) != nullptr;

        paramDisplay->setCellState ((int) i, value, locked);
    }
}

void SequencerPanel::refreshStepButtons()
{
    const bool pLockMode = editButton.getToggleState();

    for (int i = 0; i < 16; ++i)
    {
        auto& btn = stepControls[(size_t) i]->select;
        const auto& step = activeMelodicTrack->steps[(size_t) i];
        btn.setToggleState (step.enabled, juce::dontSendNotification);
        // PCM-focused steps never carry locks yet (each track's `lockable` is empty), so the LED
        // and P-LOCK highlight naturally stay dark there — editButton is disabled while focused.
        btn.setLockState (! step.locks.empty(), pLockMode && i == selectedStep);
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
            row.velocityMarker.setText (locked ? "LOCKED" : "inherit", juce::dontSendNotification);
        else
            row.velocityMarker.setText ("base", juce::dontSendNotification);
        row.velocityMarker.setColour (juce::Label::textColourId, locked ? kSelectedColour : EditorColours::textMuted);

        for (int i = 0; i < 16; ++i)
        {
            auto& btn = row.steps[(size_t) i];
            btn.setLockState (row.velocityLocks[(size_t) i].has_value(),
                              pLockMode && i == row.selectedStep);
        }
    }
}

void SequencerPanel::updateStatusLabel()
{
    // The edit-target readout lives in the parameter display's header (the "screen"), like the
    // mode/pattern line on a hardware unit. The footer statusLabel is file messages only.
    juce::String text;
    if (! editButton.getToggleState())
        text = "GRID  BASE SOUND";
    else if (selectedStep < 0)
    {
        bool hasDrumTarget = false;
        for (size_t i = 0; i < drumTrackControls.size(); ++i)
        {
            const auto& row = drumTrackControls[i];
            if (row != nullptr && row->selectedStep >= 0)
            {
                text = "P-LOCK  DRUM " + juce::String ((int) i + 1)
                     + "  STEP " + juce::String (row->selectedStep + 1).paddedLeft ('0', 2) + " VEL";
                hasDrumTarget = true;
                break;
            }
        }
        if (! hasDrumTarget)
            text = "P-LOCK  BASE SOUND";
    }
    else
        text = "P-LOCK  STEP " + juce::String (selectedStep + 1).paddedLeft ('0', 2);

    paramDisplay->setStatus (text);
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
    outstandingBaseSync.clear();    // an in-flight base sync yields the timer to the feeder
    midiIO.startPlaybackThread();   // JUCE's high-res output thread dispatches the timestamps

    transportStartMs = (double) juce::Time::getMillisecondCounter() + kStartLeadMs;
    nextStepStartMs  = 0.0;
    nextStepIndex    = 0;
    prevStepIndex    = -1;          // first fed step establishes every parameter's value fresh

    playStopButton.setButtonText ("Stop");
    playStopButton.setColour (juce::TextButton::buttonColourId, EditorColours::green);
    playStopButton.setColour (juce::TextButton::textColourOffId, EditorColours::base03);
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
    playStopButton.removeColour (juce::TextButton::buttonColourId);
    playStopButton.removeColour (juce::TextButton::textColourOffId);

    // Discard everything still queued for future dispatch (incl. not-yet-fired note-offs), then
    // release + reset explicitly since those dropped note-offs won't arrive on their own.
    midiIO.stopPlaybackThread();
    midiIO.sendAllNotesOff (sequence.channel);
    for (const auto& row : drumTrackControls)
        if (row != nullptr)
            midiIO.sendAllNotesOff (juce::jlimit (1, 16, row->channel.getSelectedId()));
    for (const auto& row : pcmTrackControls)
        if (row != nullptr)
            midiIO.sendAllNotesOff (juce::jlimit (1, 16, row->track.channel));

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

        // PCM tracks (Bass/Solo 1/Solo 2/Chords) are melodic like the Solo Synth track, so they go
        // through the exact same scheduleStep()->ScheduledEvent path — no bespoke event-building.
        // tempoBpm/stepsPerBeat are mirrored from the main sequence every tick rather than edited
        // independently, so all melodic tracks + drums share one clock even across live tempo/rate
        // changes. Each track's own `lockable` is empty in this pass, so scheduleStep only ever
        // emits noteOn/noteOff for it (no paramChange p-locks yet - a follow-up chunk).
        for (const auto& row : pcmTrackControls)
        {
            if (row == nullptr || row->mute.getToggleState())
                continue;

            row->track.tempoBpm     = sequence.tempoBpm;
            row->track.stepsPerBeat = sequence.stepsPerBeat;

            for (const auto& e : casioxw::scheduleStep (row->track, nextStepIndex, prevStepIndex, nextStepStartMs))
            {
                const int samplePos = (int) std::llround (e.timeMs);
                switch (e.type)
                {
                    case casioxw::ScheduledEvent::Type::noteOn:
                        buffer.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.velocity), samplePos);
                        break;
                    case casioxw::ScheduledEvent::Type::noteOff:
                        buffer.addEvent (juce::MidiMessage::noteOff (e.channel, e.note), samplePos);
                        break;
                    case casioxw::ScheduledEvent::Type::paramChange:
                        break;   // no lockable params on a PCM track yet
                }
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
    if (playing)
    {
        feedLookahead();
        updatePlayheadStep();
        return;
    }

    // Stopped + timer running == a base-value sync is in flight; poll the receive queue.
    for (auto& frame : midiIO.drainReceived())
    {
        const auto d = codec.decode (frame);
        if (! d.ok || d.ambiguous)
            continue;

        const auto it = outstandingBaseSync.find (d.paramId + "#" + juce::String (d.instance));
        if (it == outstandingBaseSync.end())
            continue;

        const auto& lp = sequence.lockable[(size_t) it->second];
        casioxw::setBaseValue (sequence, lp.paramId, lp.instance, d.value);
        outstandingBaseSync.erase (it);
    }

    constexpr juce::uint32 kBaseSyncTimeoutMs = 2000;
    if (outstandingBaseSync.empty())
    {
        stopTimer();
        refreshParamControls();
        statusLabel.setText ("Base sound synced from synth", juce::dontSendNotification);
    }
    else if (juce::Time::getMillisecondCounter() - baseSyncStartedMs > kBaseSyncTimeoutMs)
    {
        stopTimer();
        refreshParamControls();   // adopt whatever did arrive
        statusLabel.setText (juce::String ((int) outstandingBaseSync.size())
                                 + " base param(s) did not reply (timeout)",
                             juce::dontSendNotification);
        outstandingBaseSync.clear();
    }
}

void SequencerPanel::syncBaseValuesFromSynth()
{
    if (playing)
    {
        statusLabel.setText ("Stop playback before syncing base values", juce::dontSendNotification);
        return;
    }
    if (! midiIO.isOutputOpen() || ! midiIO.isInputOpen())
    {
        statusLabel.setText ("Not connected - open MIDI devices on the Solo Synth tab first",
                             juce::dontSendNotification);
        return;
    }

    outstandingBaseSync.clear();
    for (size_t i = 0; i < sequence.lockable.size(); ++i)
    {
        const auto& lp = sequence.lockable[i];
        midiIO.sendFrame (casioxw::MidiIO::syncRequest (codec, lp.paramId, lp.instance));
        outstandingBaseSync[lp.paramId + "#" + juce::String (lp.instance)] = (int) i;
    }

    if (outstandingBaseSync.empty())
        return;

    statusLabel.setText ("Syncing " + juce::String ((int) outstandingBaseSync.size())
                             + " base value(s) from synth...",
                         juce::dontSendNotification);
    baseSyncStartedMs = juce::Time::getMillisecondCounter();
    startTimerHz (20);
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

    // ---- footer: file save/load messages, pinned to the bottom -----------------------------
    statusLabel.setBounds (bounds.removeFromBottom (kFooterHeight));
    bounds.removeFromBottom (4);

    // ---- transport toolbar: global controls only, in a wrapping flow -----------------------
    {
        struct Item { juce::Component* c; int w; int gapAfter; };
        const Item items[] = {
            { &playStopButton, 72, 12 },
            { &tempoLabel, 34, 2 }, { &tempoSlider, 150, 12 },
            { &rateLabel, 38, 2 }, { &rateCombo, 74, 12 },
            { &channelLabel, 26, 2 }, { &channelSlider, 118, 20 },
            { &stepModeButton, 60, 2 }, { &editButton, 74, 12 },
            { &clearLocksButton, 96, 20 },
            { &saveButton, 58, 4 }, { &loadButton, 58, 4 }, { &sequenceDirButton, 70, 0 },
        };
        int x = bounds.getX();
        int y = bounds.getY();
        for (const auto& item : items)
        {
            if (x + item.w > bounds.getRight())
            {
                x = bounds.getX();
                y += kToolbarRowHeight;
            }
            item.c->setBounds (x, y, item.w, kToolbarRowHeight - 4);
            x += item.w + item.gapAfter;
        }
        bounds.removeFromTop ((y + kToolbarRowHeight) - bounds.getY() + 6);
    }

    const bool showDrumControls = drumControlsButton.getToggleState();
    const bool showSynthControls = synthControlsButton.getToggleState();
    const int cardX = bounds.getX() + kStepGridWidth + kSectionGap + kLaneLabelWidth + kSectionGap;

    // ---- drum section ----------------------------------------------------------------------
    auto drumHeader = bounds.removeFromTop (20);
    drumTracksLabel.setBounds (drumHeader.removeFromLeft (140));
    drumControlsButton.setBounds (drumHeader.removeFromLeft (22));
    bounds.removeFromTop (4);

    const int playheadTop = bounds.getY();
    const int drumTop = bounds.getY();
    for (const auto& row : drumTrackControls)
    {
        if (row == nullptr)
            continue;
        auto r = bounds.removeFromTop (kDrumTrackRowHeight);
        auto stepCells = r.removeFromLeft (kStepGridWidth);
        r.removeFromLeft (kSectionGap);
        row->trackLabel.setBounds (r.removeFromLeft (kLaneLabelWidth));
        r.removeFromLeft (kSectionGap);

        if (showDrumControls)
        {
            auto controls = r.removeFromLeft (kCardWidth).reduced (8, 2);
            row->mute.setBounds (controls.removeFromLeft (46).withSizeKeepingCentre (46, 24));
            controls.removeFromLeft (6);
            row->channel.setBounds (controls.removeFromLeft (58).withSizeKeepingCentre (58, 24));
            controls.removeFromLeft (8);
            auto noteVel = controls;
            row->note.setBounds (noteVel.removeFromTop (23));
            noteVel.removeFromTop (2);
            auto velRow = noteVel.removeFromTop (23);
            row->velocityMarker.setBounds (velRow.removeFromRight (58));
            row->velocity.setBounds (velRow);
        }
        else
        {
            row->mute.setBounds (0, 0, 0, 0);
            row->channel.setBounds (0, 0, 0, 0);
            row->note.setBounds (0, 0, 0, 0);
            row->velocity.setBounds (0, 0, 0, 0);
            row->velocityMarker.setBounds (0, 0, 0, 0);
        }

        for (auto& b : row->steps)
        {
            auto cell = stepCells.removeFromLeft (kStepWidth);
            b.setBounds (cell.withSizeKeepingCentre (kStepWidth - 6, kDrumKeyHeight));
        }

        bounds.removeFromTop (2);
    }
    drumCardBounds = showDrumControls
        ? juce::Rectangle<int> (cardX, drumTop - 2, kCardWidth, bounds.getY() - drumTop + 2)
        : juce::Rectangle<int>();

    bounds.removeFromTop (10);

    // ---- PCM-track section (compact: label + mute + channel; step editing is a follow-up) ----
    const bool showPcmControls = pcmControlsButton.getToggleState();
    auto pcmHeader = bounds.removeFromTop (20);
    pcmTracksLabel.setBounds (pcmHeader.removeFromLeft (140));
    pcmControlsButton.setBounds (pcmHeader.removeFromLeft (22));
    bounds.removeFromTop (4);

    const int pcmTop = bounds.getY();
    for (const auto& row : pcmTrackControls)
    {
        if (row == nullptr)
            continue;
        auto r = bounds.removeFromTop (kPcmTrackRowHeight);
        r.removeFromLeft (kStepGridWidth);
        r.removeFromLeft (kSectionGap);
        row->trackLabel.setBounds (r.removeFromLeft (kLaneLabelWidth));
        r.removeFromLeft (kSectionGap);

        if (showPcmControls)
        {
            auto controls = r.removeFromLeft (kCardWidth).reduced (8, 2);
            row->mute.setBounds (controls.removeFromLeft (46).withSizeKeepingCentre (46, 24));
            controls.removeFromLeft (6);
            row->channel.setBounds (controls.removeFromLeft (58).withSizeKeepingCentre (58, 24));
            controls.removeFromLeft (6);
            row->focusButton.setBounds (controls.removeFromLeft (56).withSizeKeepingCentre (56, 24));
        }
        else
        {
            row->mute.setBounds (0, 0, 0, 0);
            row->channel.setBounds (0, 0, 0, 0);
            row->focusButton.setBounds (0, 0, 0, 0);
        }

        bounds.removeFromTop (2);
    }
    pcmCardBounds = showPcmControls
        ? juce::Rectangle<int> (cardX, pcmTop - 2, kCardWidth, bounds.getY() - pcmTop + 2)
        : juce::Rectangle<int>();

    bounds.removeFromTop (10);

    // ---- synth section ---------------------------------------------------------------------
    auto synthSection = bounds.removeFromTop (juce::jmax (kStepColumnHeight, kSynthSectionHeight));
    const int playheadBottom = synthSection.getY() + kStepColumnHeight;

    auto stepCols = synthSection.removeFromLeft (kStepGridWidth);
    const int gridX = stepCols.getX();
    synthSection.removeFromLeft (kSectionGap);

    // Lane label gutter: row labels aligned to the Pitch / Gate / Velocity knob rows, same
    // column as the drum track names.
    auto labelCol = synthSection.removeFromLeft (kLaneLabelWidth);
    labelCol.removeFromTop (kSelectKeyHeight + 2);
    pitchRowLabel.setBounds (labelCol.removeFromTop (kKnobCell));
    gateRowLabel.setBounds (labelCol.removeFromTop (kKnobCell));
    velocityRowLabel.setBounds (labelCol.removeFromTop (kKnobCell));
    synthSection.removeFromLeft (kSectionGap);

    auto card = synthSection.removeFromLeft (kCardWidth);
    auto cardInner = card.reduced (8);
    auto headerRow = cardInner.removeFromTop (24);
    synthControlsButton.setBounds (headerRow.removeFromLeft (22));
    headerRow.removeFromLeft (2);
    synthFocusButton.setBounds (headerRow.removeFromLeft (34));
    headerRow.removeFromLeft (4);
    synthLabel.setBounds (headerRow.removeFromLeft (92));

    if (showSynthControls)
    {
        shiftRightButton.setBounds (headerRow.removeFromRight (28));
        headerRow.removeFromRight (2);
        shiftLeftButton.setBounds (headerRow.removeFromRight (28));
        headerRow.removeFromRight (6);
        rndOptionsButton.setBounds (headerRow.removeFromRight (22));   // reads as one "Rnd ▾" pair
        randomizeButton.setBounds (headerRow.removeFromRight (42));
        headerRow.removeFromRight (6);
        muteSynthButton.setBounds (headerRow.removeFromRight (50));
        headerRow.removeFromRight (6);
        syncBaseButton.setBounds (headerRow.removeFromRight (50));
        headerRow.removeFromRight (6);
        baseButton.setBounds (headerRow.removeFromRight (50));

        cardInner.removeFromTop (6);
        paramDisplay->setBounds (cardInner);
        paramDisplay->setVisible (true);
        synthCardBounds = card;
    }
    else
    {
        for (auto* b : { &baseButton, &syncBaseButton, &muteSynthButton, &randomizeButton,
                         &rndOptionsButton, &shiftLeftButton, &shiftRightButton })
            b->setBounds (0, 0, 0, 0);
        paramDisplay->setVisible (false);
        synthCardBounds = card.withHeight (24 + 16);
    }

    for (int i = 0; i < 16; ++i)
    {
        auto col = stepCols.removeFromLeft (kStepWidth).reduced (3, 0);
        stepControls[(size_t) i]->select.setBounds (col.removeFromTop (kSelectKeyHeight));
        col.removeFromTop (2);
        stepControls[(size_t) i]->note.setBounds (col.removeFromTop (kKnobCell));
        stepControls[(size_t) i]->gate.setBounds (col.removeFromTop (kKnobCell));
        stepControls[(size_t) i]->velocity.setBounds (col.removeFromTop (kKnobCell));
    }

    playheadLaneBounds = { gridX, playheadTop, kStepGridWidth, playheadBottom - playheadTop };
}
