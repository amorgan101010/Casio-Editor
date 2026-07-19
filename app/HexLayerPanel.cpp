#include "HexLayerPanel.h"
#include "EditorLookAndFeel.h"

namespace
{
    constexpr int kMargin = 8;
    constexpr int kRowGap = 6;
    constexpr int kTopRowHeight = 28;
    constexpr int kControlRowHeight = 28;
    constexpr int kGroupHeaderHeight = 24;
    constexpr int kInterGroupGap = 10;
    // Layer-grid sync now fans out to up to 6 layers' worth of params at once (previously just
    // the one currently-selected layer) -- widened from the original 3s so a full ~100-param batch
    // has room to reply before the panel gives up and reports a timeout. Still a guess pending an
    // owner hardware pass (hexLayer's Sync path is itself unverified -- see the class doc's
    // provenance note); revisit this number once that pass has real round-trip timing to go on.
    constexpr juce::uint32 kSyncTimeoutMs = 6000;

    constexpr const char* kSection = "hexLayer";
    constexpr const char* kLayerBlock = "Layer";
    constexpr int kHexLayerInstanceCount = 6;

    juce::String syncKey (const juce::String& paramId, int instance)
    {
        return paramId + "#" + juce::String (instance);
    }

    // Bold label + separator between a block's groups -- same visual language as
    // SoloSynthPanel/PCMEnginePanel's own GroupHeader, kept as a small local duplicate rather than
    // a shared header (same rationale PCMEnginePanel's copy already gave: not worth coupling this
    // deliberately-independent panel to another panel's translation unit for ~15 lines). Global
    // block only now -- LayerCard has its own (much smaller) title, not this.
    class GroupHeader : public juce::Component
    {
    public:
        explicit GroupHeader (juce::String text) : label (std::move (text)) {}

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds();
            g.setColour (EditorColours::textHeader);
            juce::Font font (juce::FontOptions (14.0f, juce::Font::bold));
            font.setExtraKerningFactor (0.04f);
            g.setFont (font);
            g.drawText (label.toUpperCase(), bounds.removeFromTop (getHeight() - 4), juce::Justification::centredLeft);
            g.setColour (EditorColours::border);
            g.fillRect (0, getHeight() - 2, getWidth(), 1);
        }

    private:
        juce::String label;
    };
}

//==============================================================================
/** One hex layer's card in the 2x3 grid: a bold "LAYER N" title, then On/Off + Wave (+ Pitch Lock
    on even layers) as full-width Default-mode ParamControl rows, then every other Layer param as
    a compact ParamControl::RenderMode::Knob tiled in a wrapping grid. Purely a layout shell -- it
    neither owns nor constructs the ParamControls (HexLayerPanel::controls does, same as every
    other panel's pattern), just parents + positions the ones handed to it via setRows(). */
class HexLayerPanel::LayerCard : public juce::Component
{
public:
    // Knob cells stay at ParamControl's own standard Knob-mode size (must match ParamControl.cpp's
    // kCompactCellWidth/kKnobHeight, the same "grid just tiles the control's own fixed size"
    // convention OrganPanel's drawbar grid and SoloSynthPanel's knob grid already use) -- see the
    // class doc's note on why these are NOT shrunk to pack more per card.
    static constexpr int kKnobCols = 4;
    static constexpr int kKnobCellWidth = 100;
    static constexpr int kKnobCellHeight = 110;
    static constexpr int kTitleHeight = 20;
    static constexpr int kPad = 10;
    static constexpr int kRowGapV = 2;

    explicit LayerCard (juce::String titleIn) : title (std::move (titleIn)) {}

    /** onoff/wave are always present; lock is nullptr on odd layers (Pitch Lock only exists on
        Layers 2/4/6 -- see the class doc's provenance note). Takes ownership of nothing -- every
        pointer here is owned by HexLayerPanel::controls and must outlive this card. */
    void setRows (ParamControl* onoffIn, ParamControl* waveIn, ParamControl* lockIn,
                  std::vector<ParamControl*> knobsIn)
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
        one uniform grid-cell height for the whole 2x3 layout (even-layer cards are taller, since
        they carry the extra Pitch Lock row; odd-layer cards just get a little unused space at the
        bottom, same simplification OrganPanel's fixed-size drawbar grid already accepts). */
    int contentHeight() const
    {
        const int knobRows = knobs.empty() ? 0 : (((int) knobs.size() + kKnobCols - 1) / kKnobCols);
        int h = kPad + kTitleHeight;
        if (onoff != nullptr) h += onoff->getHeight() + kRowGapV;
        if (wave  != nullptr) h += wave->getHeight()  + kRowGapV;
        if (lock  != nullptr) h += lock->getHeight()  + kRowGapV;
        h += knobRows * kKnobCellHeight;
        h += kPad;
        return h;
    }

    static int contentWidth() { return kPad * 2 + kKnobCols * kKnobCellWidth; }

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

        if (onoff != nullptr) { onoff->setBounds (b.removeFromTop (onoff->getHeight())); b.removeFromTop (kRowGapV); }
        if (wave  != nullptr) { wave->setBounds  (b.removeFromTop (wave->getHeight()));  b.removeFromTop (kRowGapV); }
        if (lock  != nullptr) { lock->setBounds  (b.removeFromTop (lock->getHeight()));  b.removeFromTop (kRowGapV); }

        int col = 0;
        int rowY = b.getY();
        for (auto* k : knobs)
        {
            k->setBounds (b.getX() + col * kKnobCellWidth, rowY, kKnobCellWidth, kKnobCellHeight);
            if (++col >= kKnobCols)
            {
                col = 0;
                rowY += kKnobCellHeight;
            }
        }
    }

private:
    juce::String title;
    ParamControl* onoff = nullptr;
    ParamControl* wave = nullptr;
    ParamControl* lock = nullptr;
    std::vector<ParamControl*> knobs;
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

    addAndMakeVisible (blockLabel);
    addAndMakeVisible (blockCombo);
    blockCombo.onChange = [this] { blockSelectionChanged(); };

    addAndMakeVisible (paramViewport);
    paramViewport.setViewedComponent (&paramContainer, false);
    paramViewport.setScrollBarsShown (true, false);

    buildBlockList();

    // Same bug-009-class fix as every other panel: setSize() above ran before the block/instance
    // combos and param rows existed, so force one final, correct layout pass now that they do.
    resized();
}

HexLayerPanel::~HexLayerPanel()
{
    stopTimer();
}

void HexLayerPanel::paint (juce::Graphics& g)
{
    g.fillAll (EditorColours::chassisBg);

    // Anchored to blockCombo -- there's no instance combo any more (Layer shows all 6 at once,
    // Global is always one instance), so blockCombo is now the only always-visible nav row, same
    // fix SoloSynthPanel's own nav-card background needed once a block could hide its instance
    // selector (see .wolf/cerebrum.md decision log, 2026-07-18).
    const int cardBottom = blockCombo.getBottom();
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
void HexLayerPanel::buildBlockList()
{
    blockOrder.clear();
    for (const auto& p : codec.model().all())
        if (p.section == kSection && ! blockOrder.contains (p.block))
            blockOrder.add (p.block);

    // dontSendNotification: clear() defaults to an async notification, which would otherwise
    // queue a redundant blockSelectionChanged() re-fire after the explicit call below already
    // runs synchronously -- same fix SoloSynthPanel::buildBlockList() applies.
    blockCombo.clear (juce::dontSendNotification);
    for (int i = 0; i < blockOrder.size(); ++i)
        blockCombo.addItem (blockOrder[i], i + 1);

    if (! blockOrder.isEmpty())
        blockCombo.setSelectedId (1, juce::dontSendNotification);

    blockSelectionChanged();
}

void HexLayerPanel::blockSelectionChanged()
{
    const int id = blockCombo.getSelectedId();
    if (id <= 0 || id > blockOrder.size())
        return;
    currentBlock = blockOrder[id - 1];

    rebuildParamControls();
    autoSyncIfConnected();
}

void HexLayerPanel::autoSyncIfConnected()
{
    if (midiIO.isOutputOpen() && midiIO.isInputOpen())
        syncButtonClicked();
}

//==============================================================================
void HexLayerPanel::rebuildParamControls()
{
    stopTimer();
    outstandingSync.clear();
    groupHeaders.clear();
    layerCards.clear();
    controls.clear();   // destroying each ParamControl removes it from its parent automatically
    rows.clear();

    if (currentBlock == kLayerBlock)
        buildLayerGrid();
    else
        buildGlobalList();

    layoutParamContainerWidth();
}

void HexLayerPanel::buildLayerGrid()
{
    const auto& model = codec.model();

    // All 6 layers at once now (the point of this redesign) -- no per-group headers within a
    // card: every card repeats the identical param set/order, so the layout is learned once from
    // Layer 1 rather than paying for 5 sub-headers (General/Amp/Filter/Effects/Range) x 6 cards.
    for (int instance = 1; instance <= kHexLayerInstanceCount; ++instance)
    {
        auto card = std::make_unique<LayerCard> ("Layer " + juce::String (instance));

        ParamControl* onoffCtrl = nullptr;
        ParamControl* waveCtrl = nullptr;
        ParamControl* lockCtrl = nullptr;
        std::vector<ParamControl*> knobCtrls;

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

            // On/Off, Wave and (even layers') Pitch Lock stay full-width Default-mode rows up top
            // of the card; every other param renders as a compact knob -- the owner's explicit
            // choice of "standard rotary knobs" over the sequencer's screen-cell p-lock look.
            const bool isKnob = p.id != "hexOnoff" && p.id != "hexWaveNumber" && p.id != "hexPitchLock";
            auto ctrl = std::make_unique<ParamControl> (model, p, instance,
                isKnob ? ParamControl::RenderMode::Knob : ParamControl::RenderMode::Default);

            const juce::String paramId = ctrl->paramId();
            const int inst = ctrl->instanceNumber();
            ctrl->onValueChanged = [this, paramId, inst] (int value)
            {
                midiIO.sendFrame (codec.encode (paramId, inst, value));
            };

            if (p.id == "hexOnoff")           onoffCtrl = ctrl.get();
            else if (p.id == "hexWaveNumber") waveCtrl = ctrl.get();
            else if (p.id == "hexPitchLock")  lockCtrl = ctrl.get();
            else                              knobCtrls.push_back (ctrl.get());

            controls.push_back (std::move (ctrl));
        }

        card->setRows (onoffCtrl, waveCtrl, lockCtrl, knobCtrls);
        paramContainer.addAndMakeVisible (*card);
        layerCards.push_back (std::move (card));
    }
}

void HexLayerPanel::buildGlobalList()
{
    const auto& model = codec.model();
    const auto groups = casioxw::orderedGroupsForBlock (model, kSection, currentBlock);

    for (size_t groupIdx = 0; groupIdx < groups.size(); ++groupIdx)
    {
        const auto& group = groups[groupIdx];

        std::vector<const casioxw::ParamInfo*> bucket;
        for (const auto& p : model.all())
            if (p.section == kSection && p.block == currentBlock && p.group == group)
                bucket.push_back (&p);
        if (bucket.empty())
            continue;

        auto header = std::make_unique<GroupHeader> (group);
        paramContainer.addAndMakeVisible (*header);
        rows.push_back ({ header.get(), kGroupHeaderHeight, 0 });
        groupHeaders.push_back (std::move (header));

        for (const auto* p : bucket)
        {
            auto ctrl = std::make_unique<ParamControl> (model, *p, 1);
            const juce::String paramId = ctrl->paramId();
            const int instance = ctrl->instanceNumber();
            ctrl->onValueChanged = [this, paramId, instance] (int value)
            {
                midiIO.sendFrame (codec.encode (paramId, instance, value));
            };
            paramContainer.addAndMakeVisible (*ctrl);
            rows.push_back ({ ctrl.get(), kControlRowHeight, 0 });
            controls.push_back (std::move (ctrl));
        }

        if (groupIdx + 1 < groups.size() && ! rows.empty())
            rows.back().gapAfter += kInterGroupGap;
    }
}

int HexLayerPanel::layoutRows (int width)
{
    int y = 0;
    for (auto& row : rows)
    {
        row.component->setBounds (0, y, width, row.height);
        y += row.height + row.gapAfter;
    }
    paramContainer.setSize (juce::jmax (400, width), y);
    return y;
}

int HexLayerPanel::layoutLayerGrid (int width)
{
    if (layerCards.empty())
    {
        paramContainer.setSize (juce::jmax (400, width), 0);
        return 0;
    }

    const int cellW = LayerCard::contentWidth();

    // One uniform cell height for the whole grid (the tallest card, always an even layer since
    // those carry the extra Pitch Lock row) -- same fixed-size-grid-cell simplification
    // OrganPanel's drawbar grid and SoloSynthPanel's knob grid already accept.
    int cellH = 0;
    for (auto& card : layerCards)
        cellH = juce::jmax (cellH, card->contentHeight());

    const int cols = juce::jmax (1, width / cellW);
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

    paramContainer.setSize (juce::jmax (cellW, cols * cellW), rowY);
    return rowY;
}

void HexLayerPanel::layoutParamContainerWidth()
{
    const int width = juce::jmax (400, paramViewport.getWidth());
    if (currentBlock == kLayerBlock)
        layoutLayerGrid (width);
    else
        layoutRows (width);
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
        outstandingSync[syncKey (c->paramId(), c->instanceNumber())] = c.get();
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
            it->second->setValueFromSync (d.value);
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

    auto navRow = bounds.removeFromTop (kTopRowHeight);
    blockLabel.setBounds (navRow.removeFromLeft (60));
    blockCombo.setBounds (navRow.removeFromLeft (160));

    bounds.removeFromTop (kRowGap);

    paramViewport.setBounds (bounds);
    layoutParamContainerWidth();
}
