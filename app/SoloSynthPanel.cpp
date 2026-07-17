#include "SoloSynthPanel.h"

namespace
{
    constexpr int kRowHeight = 28;
    constexpr int kRowGap = 6;
    constexpr int kMargin = 8;
    constexpr int kControlRowHeight = 26;
    constexpr int kGroupHeaderHeight = 24;
    constexpr int kGroupHeaderGapAbove = 8;    // extra breathing room before a new group (not the first)
    constexpr int kEnvelopeDisplayHeight = 94;
    constexpr int kEnvelopeDisplayGap = 4;
    constexpr juce::uint32 kSyncTimeoutMs = 3000;   // stop polling if replies stop arriving

    juce::String syncKey (const juce::String& paramId, int instance)
    {
        return paramId + "#" + juce::String (instance);
    }

    // Representative ParamInfo for a block: every param sharing a block has the same
    // instances.count/labels (verified against params/xwp1.json — see the task brief), so the
    // first param found for a block stands in for the whole block's navigation metadata.
    const casioxw::ParamInfo* firstParamInBlock (const casioxw::ParamModel& model, const juce::String& block)
    {
        for (const auto& p : model.all())
            if (p.section == "soloSynth" && p.block == block)
                return &p;
        return nullptr;
    }

    // Chunk 7c item 4: bold label + separator between param groups. Deliberately tiny/local —
    // no state beyond the label text, so it lives here rather than as its own file pair.
    class GroupHeader : public juce::Component
    {
    public:
        explicit GroupHeader (juce::String text) : label (std::move (text)) {}

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds();
            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
            g.drawText (label, bounds.removeFromTop (getHeight() - 4), juce::Justification::centredLeft);
            g.setColour (juce::Colours::grey);
            g.fillRect (0, getHeight() - 2, getWidth(), 1);
        }

    private:
        juce::String label;
    };

    // Chunk 7c item 5: is `group` one of the 9-stage envelope groups (as opposed to a plain
    // param group like "Pitch" or "LFO")? Purely a naming convention set by gen_xwp1.py's
    // group_for() — every "*Envelope" group's members include the 9 ENV-suffixed stage ids.
    bool isEnvelopeGroup (const juce::String& group)
    {
        return group.endsWith ("Envelope");
    }
}

//==============================================================================
SoloSynthPanel::SoloSynthPanel (casioxw::SysExCodec& codecIn, casioxw::MidiIO& midiIOIn)
    : codec (codecIn), midiIO (midiIOIn)
{
    setSize (860, 620);

    // ---- Device panel -----------------------------------------------------------------------
    addAndMakeVisible (deviceLabel);
    addAndMakeVisible (inputCombo);
    addAndMakeVisible (outputCombo);
    addAndMakeVisible (connectButton);
    addAndMakeVisible (statusLabel);
    statusLabel.setJustificationType (juce::Justification::centredLeft);

    connectButton.onClick = [this] { connectButtonClicked(); };
    refreshDeviceLists();

    // ---- Block / instance navigation ---------------------------------------------------------
    addAndMakeVisible (blockLabel);
    addAndMakeVisible (blockCombo);
    addAndMakeVisible (instanceLabel);
    addAndMakeVisible (instanceCombo);
    addAndMakeVisible (syncButton);

    blockCombo.onChange = [this] { blockSelectionChanged(); };
    instanceCombo.onChange = [this] { instanceSelectionChanged(); };
    syncButton.onClick = [this] { syncButtonClicked(); };

    // ---- Param list ----------------------------------------------------------------------------
    addAndMakeVisible (paramViewport);
    paramViewport.setViewedComponent (&paramContainer, false);
    paramViewport.setScrollBarsShown (true, false);

    buildBlockList();

    // setSize() above ran at the top of this constructor, before instanceCombo (and everything
    // else) had been added as a child or had its real visibility set by buildBlockList() ->
    // blockSelectionChanged(). That premature resized() pass laid things out with stale state
    // (e.g. instanceCombo positioned/sized before its true isVisible() was known), and nothing
    // else re-triggers a layout until the window itself is resized. Force one final, correct
    // pass now that every child is fully configured.
    resized();
}

SoloSynthPanel::~SoloSynthPanel()
{
    stopTimer();
}

//==============================================================================
void SoloSynthPanel::refreshDeviceLists()
{
    inputDevices = casioxw::MidiDevices::availableInputs();
    outputDevices = casioxw::MidiDevices::availableOutputs();

    inputCombo.clear();
    outputCombo.clear();

    int defaultInputId = 0, defaultOutputId = 0;
    for (int i = 0; i < (int) inputDevices.size(); ++i)
    {
        inputCombo.addItem (inputDevices[(size_t) i].name, i + 1);
        if (defaultInputId == 0 && inputDevices[(size_t) i].name.containsIgnoreCase ("CASIO"))
            defaultInputId = i + 1;
    }
    for (int i = 0; i < (int) outputDevices.size(); ++i)
    {
        outputCombo.addItem (outputDevices[(size_t) i].name, i + 1);
        if (defaultOutputId == 0 && outputDevices[(size_t) i].name.containsIgnoreCase ("CASIO"))
            defaultOutputId = i + 1;
    }

    if (defaultInputId > 0)
        inputCombo.setSelectedId (defaultInputId, juce::dontSendNotification);
    if (defaultOutputId > 0)
        outputCombo.setSelectedId (defaultOutputId, juce::dontSendNotification);

    inputCombo.setEnabled (! inputDevices.empty());
    outputCombo.setEnabled (! outputDevices.empty());
    connectButton.setEnabled (! inputDevices.empty() && ! outputDevices.empty());

    statusLabel.setText (inputDevices.empty() && outputDevices.empty()
                              ? "No MIDI devices found"
                              : "Not connected",
                          juce::dontSendNotification);
}

void SoloSynthPanel::connectButtonClicked()
{
    if (midiIO.isInputOpen() || midiIO.isOutputOpen())
    {
        midiIO.closeInput();
        midiIO.closeOutput();
        connectButton.setButtonText ("Connect");
        statusLabel.setText ("Disconnected", juce::dontSendNotification);
        return;
    }

    const int inId = inputCombo.getSelectedId();
    const int outId = outputCombo.getSelectedId();
    if (inId <= 0 || outId <= 0 || inId > (int) inputDevices.size() || outId > (int) outputDevices.size())
    {
        statusLabel.setText ("Select an input and output device first", juce::dontSendNotification);
        return;
    }

    const bool inOk = midiIO.openInput (inputDevices[(size_t) (inId - 1)].identifier);
    const bool outOk = midiIO.openOutput (outputDevices[(size_t) (outId - 1)].identifier);

    if (inOk && outOk)
    {
        connectButton.setButtonText ("Disconnect");
        statusLabel.setText ("Connected", juce::dontSendNotification);
        autoSyncIfConnected();   // Chunk 7c item 1: sync immediately on a successful connect
    }
    else
    {
        midiIO.closeInput();
        midiIO.closeOutput();
        statusLabel.setText ("Failed to open device(s)", juce::dontSendNotification);
    }
}

void SoloSynthPanel::autoSyncIfConnected()
{
    if (midiIO.isInputOpen() && midiIO.isOutputOpen())
        syncButtonClicked();
}

//==============================================================================
void SoloSynthPanel::buildBlockList()
{
    blockOrder.clear();
    for (const auto& p : codec.model().all())
        if (p.section == "soloSynth" && ! blockOrder.contains (p.block))
            blockOrder.add (p.block);

    blockCombo.clear();
    for (int i = 0; i < blockOrder.size(); ++i)
        blockCombo.addItem (blockOrder[i], i + 1);

    if (! blockOrder.isEmpty())
        blockCombo.setSelectedId (1, juce::dontSendNotification);

    blockSelectionChanged();
}

void SoloSynthPanel::blockSelectionChanged()
{
    const int id = blockCombo.getSelectedId();
    if (id <= 0 || id > blockOrder.size())
        return;
    currentBlock = blockOrder[id - 1];

    const auto* rep = firstParamInBlock (codec.model(), currentBlock);
    instanceCombo.clear();

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
    autoSyncIfConnected();   // Chunk 7c item 1
}

void SoloSynthPanel::instanceSelectionChanged()
{
    const int id = instanceCombo.getSelectedId();
    if (id <= 0)
        return;
    currentInstance = id;
    rebuildParamControls();
    autoSyncIfConnected();   // Chunk 7c item 1
}

//==============================================================================
void SoloSynthPanel::rebuildParamControls()
{
    stopTimer();
    outstandingSync.clear();
    envelopeSinks.clear();     // sinks capture raw EnvelopeDisplay* — must die before groupRows does
    groupRows.clear();
    controls.clear();          // destroying each ParamControl removes it from paramContainer automatically

    const auto& model = codec.model();
    const int width = paramContainer.getWidth() > 0 ? paramContainer.getWidth() : 400;
    int y = 0;
    bool firstGroup = true;

    // Chunk 7c item 4: group order comes from the JSON (model.groupOrder()), not a hardcoded
    // list here — see casioxw::orderedGroupsForBlock().
    for (const auto& group : casioxw::orderedGroupsForBlock (model, "soloSynth", currentBlock))
    {
        // Params in this group, preserving their original JSON order (item 4's requirement).
        std::vector<const casioxw::ParamInfo*> bucket;
        for (const auto& p : model.all())
            if (p.section == "soloSynth" && p.block == currentBlock && p.group == group)
                bucket.push_back (&p);
        if (bucket.empty())
            continue;

        if (! firstGroup)
            y += kGroupHeaderGapAbove;
        firstGroup = false;

        auto header = std::make_unique<GroupHeader> (group);
        header->setBounds (0, y, width, kGroupHeaderHeight);
        paramContainer.addAndMakeVisible (*header);
        groupRows.push_back (std::move (header));
        y += kGroupHeaderHeight;

        // Chunk 7c item 5: one read-only EnvelopeDisplay above each envelope group's controls,
        // seeded from the current instance's stage defaults so it isn't blank before a sync.
        if (isEnvelopeGroup (group))
        {
            const casioxw::ParamInfo* anyEnvParam = nullptr;
            for (const auto* p : bucket)
                if (p->id.contains ("ENV"))
                {
                    anyEnvParam = p;
                    break;
                }

            const auto stageIds = anyEnvParam != nullptr
                ? casioxw::envelopeStageIds (anyEnvParam->id) : casioxw::EnvelopeStageIds {};

            if (stageIds.isValid())
            {
                const auto* levelParam = model.find (stageIds.initLevel);   // defines the level axis range
                const int levelMin = levelParam != nullptr ? levelParam->range.min : -64;
                const int levelMax = levelParam != nullptr ? levelParam->range.max : 63;

                auto display = std::make_unique<EnvelopeDisplay> (levelMin, levelMax);
                display->setBounds (0, y, width, kEnvelopeDisplayHeight);

                const juce::String stageIdsInOrder[EnvelopeDisplay::kNumStages] = {
                    stageIds.initLevel,     stageIds.attackTime,    stageIds.attackLevel,
                    stageIds.decayTime,     stageIds.sustainLevel,  stageIds.release1Time,
                    stageIds.release1Level, stageIds.release2Time,  stageIds.release2Level
                };

                auto* displayPtr = display.get();
                for (int i = 0; i < EnvelopeDisplay::kNumStages; ++i)
                {
                    const auto* stageInfo = model.find (stageIdsInOrder[i]);
                    const int seed = stageInfo != nullptr && stageInfo->defaultValue.has_value()
                        ? *stageInfo->defaultValue : 0;
                    displayPtr->setStage (static_cast<EnvelopeDisplay::Stage> (i), seed);

                    const auto stage = static_cast<EnvelopeDisplay::Stage> (i);
                    envelopeSinks[syncKey (stageIdsInOrder[i], currentInstance)] =
                        [displayPtr, stage] (int value) { displayPtr->setStage (stage, value); };
                }

                paramContainer.addAndMakeVisible (*display);
                groupRows.push_back (std::move (display));
                y += kEnvelopeDisplayHeight + kEnvelopeDisplayGap;
            }
        }

        for (const auto* p : bucket)
        {
            auto ctrl = std::make_unique<ParamControl> (model, *p, currentInstance);
            ctrl->setBounds (0, y, width, kControlRowHeight);
            y += kControlRowHeight + 2;

            const juce::String paramId = ctrl->paramId();
            const int instance = ctrl->instanceNumber();

            // If this control is one of the current envelope group's 9 stages, its edits should
            // also update the EnvelopeDisplay above it (in addition to the always-present
            // send-SysEx behaviour) — look up its sink once now rather than on every edit.
            const auto sinkIt = envelopeSinks.find (syncKey (paramId, instance));
            const std::function<void (int)> envSink =
                sinkIt != envelopeSinks.end() ? sinkIt->second : std::function<void (int)> {};

            ctrl->onValueChanged = [this, paramId, instance, envSink] (int value)
            {
                const auto frame = codec.encode (paramId, instance, value);
                midiIO.sendFrame (frame);
                if (envSink)
                    envSink (value);
            };

            paramContainer.addAndMakeVisible (*ctrl);
            controls.push_back (std::move (ctrl));
        }
    }

    paramContainer.setSize (juce::jmax (400, paramViewport.getWidth()), y);
}

void SoloSynthPanel::layoutParamContainerWidth()
{
    const int width = juce::jmax (400, paramViewport.getWidth());
    paramContainer.setSize (width, paramContainer.getHeight());

    // Every child keeps its Y/height from rebuildParamControls() — only the width tracks the
    // viewport. setSize() preserves the existing top-left, so this is a pure width relayout.
    for (auto& c : controls)
        c->setSize (width, c->getHeight());
    for (auto& c : groupRows)
        c->setSize (width, c->getHeight());
}

//==============================================================================
void SoloSynthPanel::syncButtonClicked()
{
    if (! midiIO.isOutputOpen() || ! midiIO.isInputOpen())
    {
        statusLabel.setText ("Connect device(s) before syncing", juce::dontSendNotification);
        return;
    }

    outstandingSync.clear();
    for (auto& c : controls)
    {
        // Disabled controls (e.g. tssOSCwf on OSC5/6) have nothing meaningful to sync — skip.
        if (c->controlKind() == casioxw::ControlKind::Disabled)
            continue;

        const auto req = casioxw::MidiIO::syncRequest (codec, c->paramId(), c->instanceNumber());
        midiIO.sendFrame (req);
        outstandingSync[syncKey (c->paramId(), c->instanceNumber())] = c.get();
    }

    if (outstandingSync.empty())
        return;

    statusLabel.setText ("Syncing " + juce::String ((int) outstandingSync.size()) + " param(s)...",
                         juce::dontSendNotification);
    syncStartedMs = juce::Time::getMillisecondCounter();
    startTimerHz (20);   // poll the receive queue — never a busy loop
}

void SoloSynthPanel::timerCallback()
{
    for (auto& frame : midiIO.drainReceived())
    {
        const auto d = codec.decode (frame);

        // Address collisions (the flagged lfo1D/2D Lua typos) are not resolved to a specific
        // control — the codec already tells us it can't tell which param this is; guessing
        // would silently show the wrong value, so just skip it.
        if (! d.ok || d.ambiguous)
            continue;

        const auto it = outstandingSync.find (syncKey (d.paramId, d.instance));
        if (it != outstandingSync.end())
        {
            it->second->setValueFromSync (d.value);
            outstandingSync.erase (it);
        }

        // Chunk 7c item 5: envelope displays repaint from sync replies too, not just live edits
        // (ParamControl::setValueFromSync() deliberately never fires onValueChanged, so this is
        // the only path a sync reply reaches an EnvelopeDisplay).
        const auto sinkIt = envelopeSinks.find (syncKey (d.paramId, d.instance));
        if (sinkIt != envelopeSinks.end())
            sinkIt->second (d.value);
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
void SoloSynthPanel::resized()
{
    auto bounds = getLocalBounds().reduced (kMargin);

    auto deviceRow = bounds.removeFromTop (kRowHeight);
    deviceLabel.setBounds (deviceRow.removeFromLeft (90));
    inputCombo.setBounds (deviceRow.removeFromLeft (220));
    deviceRow.removeFromLeft (kRowGap);
    outputCombo.setBounds (deviceRow.removeFromLeft (220));
    deviceRow.removeFromLeft (kRowGap);
    connectButton.setBounds (deviceRow.removeFromLeft (100));
    deviceRow.removeFromLeft (kRowGap);
    statusLabel.setBounds (deviceRow);

    bounds.removeFromTop (kRowGap);

    auto navRow = bounds.removeFromTop (kRowHeight);
    blockLabel.setBounds (navRow.removeFromLeft (60));
    blockCombo.setBounds (navRow.removeFromLeft (160));
    navRow.removeFromLeft (kRowGap);
    if (instanceCombo.isVisible())
    {
        instanceLabel.setBounds (navRow.removeFromLeft (70));
        instanceCombo.setBounds (navRow.removeFromLeft (160));
        navRow.removeFromLeft (kRowGap);
    }
    syncButton.setBounds (navRow.removeFromLeft (100));

    bounds.removeFromTop (kRowGap);

    paramViewport.setBounds (bounds);
    layoutParamContainerWidth();
}
