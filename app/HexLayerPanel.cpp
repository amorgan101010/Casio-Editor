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
    constexpr juce::uint32 kSyncTimeoutMs = 3000;   // stop polling if replies stop arriving

    constexpr const char* kSection = "hexLayer";

    juce::String syncKey (const juce::String& paramId, int instance)
    {
        return paramId + "#" + juce::String (instance);
    }

    // Representative ParamInfo for a block: every param sharing a block shares the same
    // instances.count/labels (Layer=6 "Layer 1".."Layer 6", Global=1 "Hex Layer") -- same
    // "first param stands in for the block" convention SoloSynthPanel's firstParamInBlock() uses.
    const casioxw::ParamInfo* firstParamInBlock (const casioxw::ParamModel& model, const juce::String& block)
    {
        for (const auto& p : model.all())
            if (p.section == kSection && p.block == block)
                return &p;
        return nullptr;
    }

    // Bold label + separator between a block's groups -- same visual language as
    // SoloSynthPanel/PCMEnginePanel's own GroupHeader, kept as a small local duplicate rather than
    // a shared header (same rationale PCMEnginePanel's copy already gave: not worth coupling this
    // deliberately-independent panel to another panel's translation unit for ~15 lines).
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
    addAndMakeVisible (instanceLabel);
    addAndMakeVisible (instanceCombo);
    blockCombo.onChange = [this] { blockSelectionChanged(); };
    instanceCombo.onChange = [this] { instanceSelectionChanged(); };

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

    // Anchored to blockCombo (always visible) rather than instanceCombo (sometimes hidden for the
    // Global block) -- the same fix SoloSynthPanel's own nav-card background needed once a block
    // could hide its instance selector (see .wolf/cerebrum.md decision log, 2026-07-18).
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

    const auto* rep = firstParamInBlock (codec.model(), currentBlock);
    instanceCombo.clear (juce::dontSendNotification);

    const int instanceCount = rep != nullptr ? rep->instanceCount : 1;
    const bool multi = instanceCount > 1;
    instanceLabel.setVisible (multi);
    instanceCombo.setVisible (multi);

    if (multi && rep != nullptr)
    {
        for (int i = 0; i < instanceCount; ++i)
        {
            const juce::String label = i < rep->instanceLabels.size() ? rep->instanceLabels[i]
                                                                        : juce::String (i + 1);
            instanceCombo.addItem (label, i + 1);
        }
        instanceCombo.setSelectedId (1, juce::dontSendNotification);
    }

    currentInstance = 1;
    rebuildParamControls();
    autoSyncIfConnected();
}

void HexLayerPanel::instanceSelectionChanged()
{
    const int id = instanceCombo.getSelectedId();
    if (id <= 0)
        return;
    currentInstance = id;
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
    controls.clear();   // destroying each ParamControl removes it from paramContainer automatically
    rows.clear();

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
            auto ctrl = std::make_unique<ParamControl> (model, *p, currentInstance);
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

    layoutRows (paramContainer.getWidth() > 0 ? paramContainer.getWidth() : 400);
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

void HexLayerPanel::layoutParamContainerWidth()
{
    layoutRows (juce::jmax (400, paramViewport.getWidth()));
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
    navRow.removeFromLeft (kRowGap);
    if (instanceCombo.isVisible())
    {
        instanceLabel.setBounds (navRow.removeFromLeft (60));
        instanceCombo.setBounds (navRow.removeFromLeft (140));
    }

    bounds.removeFromTop (kRowGap);

    paramViewport.setBounds (bounds);
    layoutParamContainerWidth();
}
