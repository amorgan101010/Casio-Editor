#include "HexLayerPanel.h"
#include "EditorLookAndFeel.h"

#include <map>

namespace
{
    constexpr int kMargin = 8;
    constexpr int kRowGap = 6;
    constexpr int kTopRowHeight = 28;
    // Layer-grid sync now fans out to ~90-103 params at once (previously just one layer's worth)
    // -- widened from the original 3s so a full batch has room to reply before the panel gives up
    // and reports a timeout. Still a guess pending an owner hardware pass (hexLayer's Sync path is
    // itself unverified -- see the class doc's provenance note).
    constexpr juce::uint32 kSyncTimeoutMs = 6000;

    constexpr const char* kSection = "hexLayer";
    constexpr const char* kLayerBlock = "Layer";
    constexpr const char* kGlobalBlock = "Global";
    constexpr int kHexLayerInstanceCount = 6;

    // Shared by LayerCard's knob grid and GlobalSection's knob row -- see MiniKnob's own doc
    // comment for why this is much smaller than ParamControl::RenderMode::Knob's 100x110, and why
    // it's not smaller still (matches the sequencer's own validated per-step knob floor).
    constexpr int kMiniKnobWidth = 70;
    constexpr int kMiniKnobHeight = 80;

    constexpr int kInterSectionGap = 12;   // between the layer-card grid and GlobalSection

    juce::String syncKey (const juce::String& paramId, int instance)
    {
        return paramId + "#" + juce::String (instance);
    }

    // 3-4 char hardware-style captions for the numeric Layer/Global params -- same convention
    // ParamPageDisplay::CellSpec::shortName already established for a compact knob cell.
    juce::String shortNameFor (const juce::String& paramId)
    {
        static const std::map<juce::String, juce::String> kNames = {
            { "hexPanOffset", "PAN" }, { "hexPitchKey", "KEY" },
            { "hexAmpAttackOfs", "ATK" }, { "hexAmpDecayOfs", "DCY" },
            { "hexAmpSustainOfs", "SUS" }, { "hexAmpReleaseOfs", "REL" },
            { "hexVolumeOfs", "VOL" }, { "hexCutoffOfs", "CUT" },
            { "hexTouchSenseOfs", "TS" }, { "hexReverbSendOfs", "REV" },
            { "hexChorusSendOfs", "CHO" }, { "hexKeyRangeLow", "KLO" },
            { "hexKeyRangeHigh", "KHI" }, { "hexVelRangeLow", "VLO" },
            { "hexVelRangeHigh", "VHI" },
            { "hexDetuneNumber", "DET" },
            { "hexPitchLfoRate", "P.RATE" }, { "hexPitchAutoDelay", "P.DLY" },
            { "hexPitchAutoRise", "P.RISE" }, { "hexPitchAutoDepth", "P.ADEP" },
            { "hexPitchModDepth", "P.MDEP" }, { "hexPitchAfterDepth", "P.AFT" },
            { "hexAmpLfoRate", "A.RATE" }, { "hexAmpAutoDelay", "A.DLY" },
            { "hexAmpAutoRise", "A.RISE" }, { "hexAmpAutoDepth", "A.ADEP" },
            { "hexAmpModDepth", "A.MDEP" }, { "hexAmpAfterDepth", "A.AFT" },
        };
        const auto it = kNames.find (paramId);
        return it != kNames.end() ? it->second : paramId;
    }
}

//==============================================================================
/** A compact rotary-knob cell for one numeric Layer/Global param: a short 3-4 char caps label
    painted above, a bare juce::Slider (RotaryHorizontalVerticalDrag, NoTextBox -- the value is
    painted below instead of a live JUCE text box) in the middle.

    Deliberately NOT ParamControl::RenderMode::Knob: that widget has ~50px of fixed internal
    overhead (a 34px name-label strip + a 16px live text box, baked into ParamControl.cpp's own
    resized(), which doesn't shrink with the cell) -- giving it a cell shorter than ~100px tall
    starves the actual dial down to an unreadable size. Fitting 90 Layer knobs + 13 Global knobs
    on one page inside the app's existing ~1490x962 window needs a much smaller cell, so MiniKnob
    keeps only ~28px of painted label/value overhead (14px each) instead, leaving room for a dial
    close to the SEQUENCER's own validated per-step knob size (SequencerPanel's note/gate/velocity
    knobs render a ~52px dial in a 74px-tall cell -- see .wolf/cerebrum.md addendum 8, where that
    size was deliberately ENLARGED from something smaller after an owner "tiny dots" complaint).
    MiniKnob's dial matches that floor rather than inventing a smaller one.

    Modeled structurally on ParamPageDisplay::Cell (the sequencer's own compact-knob-cell
    precedent: painted label above, NoTextBox rotary, painted value below) but WITHOUT that
    class's phosphor/glass colour overrides -- MiniKnob leaves the rotary's colours unset, so it
    inherits the same standard orange-on-panelBg look EditorLookAndFeel::drawRotarySlider draws
    for every other knob in the app. That's a deliberate, owner-confirmed choice (offered directly
    against the sequencer's screen-cell look, picked "matches the rest of the editor's normal
    control chrome") -- only Cell's SHAPE is reused here, not its palette.

    Dumb like ParamControl: knows nothing about SysEx/MidiIO. The owning panel wires
    onValueChanged to build+send an edit and calls setValueFromSync() to push a synced value in
    without firing it. */
class HexLayerPanel::MiniKnob : public juce::Component
{
public:
    MiniKnob (juce::String paramIdIn, int instanceIn, int rangeMin, int rangeMax, juce::String shortNameIn)
        : paramId (std::move (paramIdIn)), instance (instanceIn), shortName (std::move (shortNameIn))
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setRange ((double) rangeMin, (double) rangeMax, 1.0);
        knob.onValueChange = [this]
        {
            repaint();
            if (onValueChanged != nullptr)
                onValueChanged ((int) knob.getValue());
        };
        addAndMakeVisible (knob);
    }

    const juce::String& getParamId() const noexcept { return paramId; }
    int getInstance() const noexcept { return instance; }

    /** Update the displayed value from a sync read-back WITHOUT firing onValueChanged -- same
        guard ParamControl::setValueFromSync() provides. */
    void setValueFromSync (int value)
    {
        knob.setValue ((double) value, juce::dontSendNotification);
        repaint();
    }

    std::function<void (int)> onValueChanged;

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        g.setFont (EditorFonts::mono (10.0f, true));
        g.setColour (EditorColours::textMuted);
        g.drawFittedText (shortName, b.removeFromTop (14), juce::Justification::centred, 1);

        g.setFont (EditorFonts::mono (11.5f));
        g.setColour (EditorColours::textPrimary);
        g.drawFittedText (juce::String ((int) knob.getValue()), b.removeFromBottom (14), juce::Justification::centred, 1);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        b.removeFromTop (14);
        b.removeFromBottom (14);
        knob.setBounds (b.reduced (1));
    }

private:
    juce::String paramId;
    int instance;
    juce::String shortName;
    juce::Slider knob;
};

//==============================================================================
/** One hex layer's card in the 2-wide x 3-tall grid: a bold "LAYER N" title, then On/Off + Pitch
    Lock sharing ONE row (left/right half each -- Pitch Lock is blank on odd layers, but the row
    is always there, so every card's header is the same height and the knob grids below line up
    across all 6 cards), then a full-width Wave row, then every other Layer param as a MiniKnob
    tiled in a grid. Purely a layout shell -- it neither owns nor constructs its child controls
    (HexLayerPanel::controls/miniKnobs do), just parents + positions the ones handed to it via
    setRows(). */
class HexLayerPanel::LayerCard : public juce::Component
{
public:
    // kKnobCols=10 fits the 15 numeric Layer params in 2 rows (10+5) at a card width that pairs
    // 2-per-row inside the app's existing ~1490px window -- see HexLayerPanel::layoutContent().
    static constexpr int kKnobCols = 10;
    static constexpr int kPad = 10;
    static constexpr int kTitleHeight = 20;
    static constexpr int kRowGapV = 2;

    explicit LayerCard (juce::String titleIn) : title (std::move (titleIn)) {}

    /** onoff/wave are always present; lock is nullptr on odd layers (Pitch Lock only exists on
        Layers 2/4/6 -- see the class doc's provenance note). Takes ownership of nothing -- every
        pointer here is owned by HexLayerPanel and must outlive this card. */
    void setRows (ParamControl* onoffIn, ParamControl* waveIn, ParamControl* lockIn,
                  std::vector<MiniKnob*> knobsIn)
    {
        onoff = onoffIn;
        wave = waveIn;
        lock = lockIn;
        knobs = std::move (knobsIn);

        addAndMakeVisible (onoff);
        addAndMakeVisible (wave);
        if (lock != nullptr)
            addAndMakeVisible (lock);
        for (auto* k : knobs)
            addAndMakeVisible (k);
    }

    /** Total height this card needs at its fixed content width -- used by HexLayerPanel to pick
        one uniform grid-cell height. Identical for every card (the On/Off + Pitch Lock row is
        always present, just half-empty on odd layers), which is the whole point of combining them
        onto one row -- knob grids line up across all 6 cards. */
    int contentHeight() const
    {
        const int knobRows = knobs.empty() ? 0 : (((int) knobs.size() + kKnobCols - 1) / kKnobCols);
        int h = kPad + kTitleHeight;
        h += onoff->getHeight() + kRowGapV;             // On/Off + Pitch Lock, shared row
        if (wave != nullptr) h += wave->getHeight() + kRowGapV;
        h += knobRows * kMiniKnobHeight;
        h += kPad;
        return h;
    }

    static int contentWidth() { return kPad * 2 + kKnobCols * kMiniKnobWidth; }

    void paint (juce::Graphics& g) override
    {
        auto card = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (EditorColours::panelBg);
        g.fillRoundedRectangle (card, 5.0f);
        g.setColour (EditorColours::border.withAlpha (0.6f));
        g.drawRoundedRectangle (card, 5.0f, 1.0f);

        juce::Font font (juce::FontOptions (14.0f, juce::Font::bold));
        font.setExtraKerningFactor (0.05f);
        g.setFont (font);
        g.setColour (EditorColours::textHeader);
        g.drawText (title.toUpperCase(), getLocalBounds().reduced (kPad, 0).removeFromTop (kPad + kTitleHeight).withTrimmedTop (kPad),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (kPad);
        b.removeFromTop (kTitleHeight);

        auto headerRow = b.removeFromTop (onoff->getHeight());
        onoff->setBounds (headerRow.removeFromLeft (headerRow.getWidth() / 2));
        if (lock != nullptr)
            lock->setBounds (headerRow);
        b.removeFromTop (kRowGapV);

        if (wave != nullptr) { wave->setBounds (b.removeFromTop (wave->getHeight())); b.removeFromTop (kRowGapV); }

        int col = 0;
        int rowY = b.getY();
        for (auto* k : knobs)
        {
            k->setBounds (b.getX() + col * kMiniKnobWidth, rowY, kMiniKnobWidth, kMiniKnobHeight);
            if (++col >= kKnobCols)
            {
                col = 0;
                rowY += kMiniKnobHeight;
            }
        }
    }

private:
    juce::String title;
    ParamControl* onoff = nullptr;
    ParamControl* wave = nullptr;
    ParamControl* lock = nullptr;
    std::vector<MiniKnob*> knobs;
};

//==============================================================================
/** The shared Pitch/Amp LFO pair + Detune (Global block, one instance, never had the per-layer
    paging pain the Layer block did) -- now a single full-width section below the LayerCard grid
    instead of behind a block combo (owner feedback: "we don't really need the block selector any
    more, find somewhere on the same screen"). "GLOBAL" is folded into the same row as the two
    LFO Wave Type combos (rather than a separate title row above them, the way LayerCard does it)
    to save vertical space -- this section only has 2 non-knob rows to begin with, so folding the
    title costs nothing in readability the way it would on a busier card. */
class HexLayerPanel::GlobalSection : public juce::Component
{
public:
    static constexpr int kPad = 10;
    static constexpr int kTitleWidth = 90;
    static constexpr int kRowGapV = 2;
    static constexpr int kComboWidth = 340;

    /** Takes ownership of nothing -- every pointer here is owned by HexLayerPanel and must
        outlive this section. */
    void setRows (ParamControl* pitchWaveIn, ParamControl* ampWaveIn, std::vector<MiniKnob*> knobsIn)
    {
        pitchWave = pitchWaveIn;
        ampWave = ampWaveIn;
        knobs = std::move (knobsIn);

        addAndMakeVisible (pitchWave);
        addAndMakeVisible (ampWave);
        for (auto* k : knobs)
            addAndMakeVisible (k);
    }

    int contentHeight() const
    {
        const int headerH = juce::jmax (pitchWave->getHeight(), ampWave->getHeight());
        const int knobRowH = knobs.empty() ? 0 : kMiniKnobHeight;
        return kPad + headerH + kRowGapV + knobRowH + kPad;
    }

    void paint (juce::Graphics& g) override
    {
        auto card = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (EditorColours::panelBg);
        g.fillRoundedRectangle (card, 5.0f);
        g.setColour (EditorColours::border.withAlpha (0.6f));
        g.drawRoundedRectangle (card, 5.0f, 1.0f);

        const int headerH = juce::jmax (pitchWave->getHeight(), ampWave->getHeight());
        juce::Font font (juce::FontOptions (14.0f, juce::Font::bold));
        font.setExtraKerningFactor (0.05f);
        g.setFont (font);
        g.setColour (EditorColours::textHeader);
        auto titleZone = getLocalBounds().reduced (kPad, 0).removeFromTop (kPad + headerH).withTrimmedTop (kPad).removeFromLeft (kTitleWidth - kPad);
        g.drawText ("GLOBAL", titleZone, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (kPad);
        auto headerRow = b.removeFromTop (juce::jmax (pitchWave->getHeight(), ampWave->getHeight()));
        headerRow.removeFromLeft (kTitleWidth);
        pitchWave->setBounds (headerRow.removeFromLeft (juce::jmin (kComboWidth, headerRow.getWidth())));
        headerRow.removeFromLeft (kPad);
        ampWave->setBounds (headerRow.removeFromLeft (juce::jmin (kComboWidth, headerRow.getWidth())));
        b.removeFromTop (kRowGapV);

        int x = b.getX();
        for (auto* k : knobs)
        {
            k->setBounds (x, b.getY(), kMiniKnobWidth, kMiniKnobHeight);
            x += kMiniKnobWidth;
        }
    }

private:
    ParamControl* pitchWave = nullptr;
    ParamControl* ampWave = nullptr;
    std::vector<MiniKnob*> knobs;
};

//==============================================================================
HexLayerPanel::HexLayerPanel (casioxw::SysExCodec& codecIn, casioxw::MidiIO& midiIOIn)
    : codec (codecIn), midiIO (midiIOIn)
{
    setSize (560, 620);

    addAndMakeVisible (statusLabel);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setText (midiIO.isOutputOpen() && midiIO.isInputOpen()
                              ? "Connected"
                              : "Not connected - open MIDI devices on the Solo Synth tab first",
                          juce::dontSendNotification);

    addAndMakeVisible (syncButton);
    syncButton.onClick = [this] { syncButtonClicked(); };

    addAndMakeVisible (paramViewport);
    paramViewport.setViewedComponent (&paramContainer, false);
    paramViewport.setScrollBarsShown (true, false);

    buildParamControls();

    // Same bug-009-class fix as every other panel: setSize() above ran before the param rows
    // existed, so force one final, correct layout pass now that they do.
    resized();

    if (midiIO.isOutputOpen() && midiIO.isInputOpen())
        syncButtonClicked();
}

HexLayerPanel::~HexLayerPanel()
{
    stopTimer();
}

void HexLayerPanel::paint (juce::Graphics& g)
{
    g.fillAll (EditorColours::chassisBg);

    const int cardBottom = syncButton.getBottom();
    if (cardBottom > 0)
    {
        auto card = juce::Rectangle<int> (0, 0, getWidth(), cardBottom + kMargin).toFloat().reduced (2.0f);
        g.setColour (EditorColours::panelBg);
        g.fillRoundedRectangle (card, 4.0f);
        g.setColour (EditorColours::border.withAlpha (0.5f));
        g.drawRoundedRectangle (card, 4.0f, 1.0f);
    }
}

//==============================================================================
void HexLayerPanel::buildParamControls()
{
    stopTimer();
    outstandingSync.clear();
    layerCards.clear();
    globalSection.reset();
    miniKnobs.clear();
    controls.clear();   // destroying each ParamControl removes it from its parent automatically

    buildLayerGrid();
    buildGlobalSection();

    layoutParamContainerWidth();
}

void HexLayerPanel::buildLayerGrid()
{
    const auto& model = codec.model();

    // All 6 layers at once -- no per-group headers within a card: every card repeats the
    // identical param set/order, so the layout is learned once from Layer 1 rather than paying
    // for 5 sub-headers (General/Amp/Filter/Effects/Range) x 6 cards.
    for (int instance = 1; instance <= kHexLayerInstanceCount; ++instance)
    {
        auto card = std::make_unique<LayerCard> ("Layer " + juce::String (instance));

        ParamControl* onoffCtrl = nullptr;
        ParamControl* waveCtrl = nullptr;
        ParamControl* lockCtrl = nullptr;
        std::vector<MiniKnob*> knobCtrls;

        for (const auto& p : model.all())
        {
            if (p.section != kSection || p.block != kLayerBlock)
                continue;

            // Pitch Lock only exists on Layers 2/4/6 (manual: "Pitch Lock (Layers 2, 4, and 6
            // only)" -- turning it on for the even layer copies the odd layer's pitch onto it).
            // Layers 1/3/5 have nothing to lock TO, so skip the control entirely rather than show
            // a toggle with no effect -- see gen_xwp1.py's HEXLAYER_PITCH_LOCK_NOTE.
            if (p.id == "hexPitchLock" && (instance % 2) != 0)
                continue;

            if (p.id == "hexOnoff" || p.id == "hexWaveNumber" || p.id == "hexPitchLock")
            {
                // On/Off, Wave and (even layers') Pitch Lock need ParamControl's real widgets
                // (ToggleButton / WavePicker) -- not a knob.
                auto ctrl = std::make_unique<ParamControl> (model, p, instance);
                const juce::String paramId = ctrl->paramId();
                const int inst = ctrl->instanceNumber();
                ctrl->onValueChanged = [this, paramId, inst] (int value)
                {
                    midiIO.sendFrame (codec.encode (paramId, inst, value));
                };

                if (p.id == "hexOnoff")           onoffCtrl = ctrl.get();
                else if (p.id == "hexWaveNumber") waveCtrl = ctrl.get();
                else                              lockCtrl = ctrl.get();
                controls.push_back (std::move (ctrl));
            }
            else
            {
                auto knob = std::make_unique<MiniKnob> (p.id, instance, p.range.min, p.range.max, shortNameFor (p.id));
                const juce::String paramId = knob->getParamId();
                const int inst = knob->getInstance();
                knob->onValueChanged = [this, paramId, inst] (int value)
                {
                    midiIO.sendFrame (codec.encode (paramId, inst, value));
                };
                knobCtrls.push_back (knob.get());
                miniKnobs.push_back (std::move (knob));
            }
        }

        card->setRows (onoffCtrl, waveCtrl, lockCtrl, knobCtrls);
        paramContainer.addAndMakeVisible (*card);
        layerCards.push_back (std::move (card));
    }
}

void HexLayerPanel::buildGlobalSection()
{
    const auto& model = codec.model();
    auto section = std::make_unique<GlobalSection>();

    ParamControl* pitchWaveCtrl = nullptr;
    ParamControl* ampWaveCtrl = nullptr;
    std::vector<MiniKnob*> knobCtrls;

    for (const auto& p : model.all())
    {
        if (p.section != kSection || p.block != kGlobalBlock)
            continue;

        if (p.id == "hexPitchLfoWave" || p.id == "hexAmpLfoWave")
        {
            auto ctrl = std::make_unique<ParamControl> (model, p, 1);
            const juce::String paramId = ctrl->paramId();
            ctrl->onValueChanged = [this, paramId] (int value)
            {
                midiIO.sendFrame (codec.encode (paramId, 1, value));
            };

            if (p.id == "hexPitchLfoWave") pitchWaveCtrl = ctrl.get();
            else                           ampWaveCtrl = ctrl.get();
            controls.push_back (std::move (ctrl));
        }
        else
        {
            auto knob = std::make_unique<MiniKnob> (p.id, 1, p.range.min, p.range.max, shortNameFor (p.id));
            const juce::String paramId = knob->getParamId();
            knob->onValueChanged = [this, paramId] (int value)
            {
                midiIO.sendFrame (codec.encode (paramId, 1, value));
            };
            knobCtrls.push_back (knob.get());
            miniKnobs.push_back (std::move (knob));
        }
    }

    section->setRows (pitchWaveCtrl, ampWaveCtrl, knobCtrls);
    paramContainer.addAndMakeVisible (*section);
    globalSection = std::move (section);
}

//==============================================================================
int HexLayerPanel::layoutContent (int width)
{
    if (layerCards.empty())
    {
        paramContainer.setSize (juce::jmax (400, width), 0);
        return 0;
    }

    const int cellW = LayerCard::contentWidth();

    // One uniform cell height for the whole grid (every card is the same height now that On/Off +
    // Pitch Lock share one row -- see LayerCard::contentHeight()'s own comment).
    int cellH = 0;
    for (auto& card : layerCards)
        cellH = juce::jmax (cellH, card->contentHeight());

    // 2 wide x 3 tall, fixed (owner's explicit layout, not a responsive wrap): layerCards is built
    // in instance order 1..6, so 2 columns naturally pairs (1,2)/(3,4)/(5,6) -- the exact pairing
    // Pitch Lock needs (Layer 2 locks to 1, 4 to 3, 6 to 5). Still falls back to 1 column if the
    // window is too narrow for 2 (avoids a horizontal scrollbar).
    const int cols = juce::jmin (2, juce::jmax (1, width / cellW));
    int col = 0;
    int rowY = 0;
    for (auto& card : layerCards)
    {
        card->setBounds (col * cellW, rowY, cellW, cellH);
        if (++col >= cols)
        {
            col = 0;
            rowY += cellH;
        }
    }
    if (col != 0)
        rowY += cellH;   // a partially-filled last row still consumes a full row

    const int gridWidth = juce::jmax (cellW, cols * cellW);
    int totalHeight = rowY;

    if (globalSection != nullptr)
    {
        totalHeight += kInterSectionGap;
        const int globalHeight = globalSection->contentHeight();
        globalSection->setBounds (0, totalHeight, gridWidth, globalHeight);
        totalHeight += globalHeight;
    }

    paramContainer.setSize (juce::jmax (gridWidth, width), totalHeight);
    return totalHeight;
}

void HexLayerPanel::layoutParamContainerWidth()
{
    layoutContent (juce::jmax (400, paramViewport.getWidth()));
}

//==============================================================================
void HexLayerPanel::syncButtonClicked()
{
    if (! midiIO.isOutputOpen() || ! midiIO.isInputOpen())
    {
        statusLabel.setText ("Connect device(s) on the Solo Synth tab before syncing", juce::dontSendNotification);
        return;
    }

    outstandingSync.clear();
    for (auto& c : controls)
    {
        const auto req = casioxw::MidiIO::syncRequest (codec, c->paramId(), c->instanceNumber());
        midiIO.sendFrame (req);
        ParamControl* ptr = c.get();
        outstandingSync[syncKey (c->paramId(), c->instanceNumber())] = [ptr] (int v) { ptr->setValueFromSync (v); };
    }
    for (auto& k : miniKnobs)
    {
        const auto req = casioxw::MidiIO::syncRequest (codec, k->getParamId(), k->getInstance());
        midiIO.sendFrame (req);
        MiniKnob* ptr = k.get();
        outstandingSync[syncKey (k->getParamId(), k->getInstance())] = [ptr] (int v) { ptr->setValueFromSync (v); };
    }

    if (outstandingSync.empty())
        return;

    statusLabel.setText ("Syncing " + juce::String ((int) outstandingSync.size()) + " param(s)...",
                         juce::dontSendNotification);
    syncStartedMs = juce::Time::getMillisecondCounter();
    startTimerHz (20);   // poll the receive queue -- never a busy loop
}

void HexLayerPanel::timerCallback()
{
    for (auto& frame : midiIO.drainReceived())
    {
        const auto d = codec.decode (frame);
        if (! d.ok || d.ambiguous)
            continue;

        const auto it = outstandingSync.find (syncKey (d.paramId, d.instance));
        if (it != outstandingSync.end())
        {
            it->second (d.value);
            outstandingSync.erase (it);
        }
    }

    if (outstandingSync.empty())
    {
        statusLabel.setText ("Sync complete", juce::dontSendNotification);
        stopTimer();
    }
    else if (juce::Time::getMillisecondCounter() - syncStartedMs > kSyncTimeoutMs)
    {
        statusLabel.setText (juce::String ((int) outstandingSync.size()) + " param(s) did not reply (timeout)",
                             juce::dontSendNotification);
        stopTimer();
    }
}

//==============================================================================
void HexLayerPanel::resized()
{
    auto bounds = getLocalBounds().reduced (kMargin);

    auto topRow = bounds.removeFromTop (kTopRowHeight);
    syncButton.setBounds (topRow.removeFromLeft (100));
    topRow.removeFromLeft (kRowGap);
    statusLabel.setBounds (topRow);

    bounds.removeFromTop (kRowGap);

    paramViewport.setBounds (bounds);
    layoutParamContainerWidth();
}
