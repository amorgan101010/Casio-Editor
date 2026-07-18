#include "PCMEnginePanel.h"
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

    constexpr const char* kSection = "pcmMelody";
    constexpr const char* kBlock = "Melody";

    juce::String syncKey (const juce::String& paramId, int instance)
    {
        return paramId + "#" + juce::String (instance);
    }

    // Bold label + separator between the block's groups -- same visual language as
    // SoloSynthPanel's own GroupHeader, kept as a small local duplicate rather than a shared
    // header: pulling it into a common file for one ~15-line class isn't worth coupling this
    // deliberately-independent panel to SoloSynthPanel's translation unit.
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
PCMEnginePanel::PCMEnginePanel (casioxw::SysExCodec& codecIn, casioxw::MidiIO& midiIOIn)
    : codec (codecIn), midiIO (midiIOIn)
{
    setSize (520, 560);

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

    // Same bug-009-class fix as SoloSynthPanel/SequencerPanel: setSize() above ran before the
    // param rows existed, so force one final, correct layout pass now that they do.
    resized();
}

PCMEnginePanel::~PCMEnginePanel()
{
    stopTimer();
}

void PCMEnginePanel::paint (juce::Graphics& g)
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
void PCMEnginePanel::buildParamControls()
{
    stopTimer();
    outstandingSync.clear();
    groupHeaders.clear();
    controls.clear();   // destroying each ParamControl removes it from paramContainer automatically
    rows.clear();

    const auto& model = codec.model();
    const auto groups = casioxw::orderedGroupsForBlock (model, kSection, kBlock);

    for (size_t groupIdx = 0; groupIdx < groups.size(); ++groupIdx)
    {
        const auto& group = groups[groupIdx];

        std::vector<const casioxw::ParamInfo*> bucket;
        for (const auto& p : model.all())
            if (p.section == kSection && p.block == kBlock && p.group == group)
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
            ctrl->onValueChanged = [this, paramId] (int value)
            {
                midiIO.sendFrame (codec.encode (paramId, 1, value));
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

int PCMEnginePanel::layoutRows (int width)
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

void PCMEnginePanel::layoutParamContainerWidth()
{
    layoutRows (juce::jmax (400, paramViewport.getWidth()));
}

//==============================================================================
void PCMEnginePanel::syncButtonClicked()
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

void PCMEnginePanel::timerCallback()
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
void PCMEnginePanel::resized()
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
