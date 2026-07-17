#include "SoloSynthPanel.h"

namespace
{
    constexpr int kRowHeight = 28;
    constexpr int kRowGap = 6;
    constexpr int kMargin = 8;
    constexpr int kControlRowHeight = 26;
    constexpr int kEnvelopeDisplayHeight = 94;
    constexpr int kEnvelopeDisplayGap = 4;
    constexpr juce::uint32 kSyncTimeoutMs = 3000;   // stop polling if replies stop arriving

    // Chunk 7f: knob and fader cells now share one common width (must match ParamControl.cpp's
    // kCompactCellWidth) — owner feedback that the two rows didn't line up at 88 vs 56. Heights
    // stay different (a knob doesn't need vertical throw, a fader does) but must match
    // ParamControl.cpp's kKnobHeight/kFaderHeight exactly (ParamControl owns its own size for a
    // given RenderMode; the grid here just tiles that fixed cell).
    constexpr int kKnobCellWidth = 100;
    constexpr int kKnobCellHeight = 110;
    constexpr int kGridGapAbove = 6;   // gap before a grid/fader-row when something precedes it

    constexpr int kFaderCellWidth = 100;
    constexpr int kFaderCellHeight = 164;

    // Chunk 7e item 2: only show the Group selector when a block has enough params that
    // group-at-a-time navigation actually helps. Current per-block totals: OSC=56, PWM=3, Etc=11,
    // TotalFilter=20, LFO=8 — any threshold between roughly 21 and 55 produces the same "OSC
    // only" result the owner asked for; picked with headroom on both sides rather than tuned to
    // the exact boundary.
    constexpr int kGroupSelectorParamThreshold = 25;

    // Extra breathing room between groups when several render together (Group selector hidden).
    constexpr int kInterGroupGap = 14;

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

    // Chunk 7d item 1: with one group shown at a time (when the selector is visible — see
    // groupSelectorNeeded), the old GroupHeader (bold label + separator, one per stacked group)
    // became redundant with groupCombo already naming the group. Chunk 7e item 1: whether a param
    // is one of the 9 envelope-shape stages is now a PER-PARAM check
    // (casioxw::envelopeStageIds(id).isValid()) rather than a whole-group one — see
    // rebuildParamControls().

    // Chunk 7f: "All Parameters" — owner feedback wanting the pre-knob/fader-redesign giant flat
    // list back as an OPTION (not the default), reachable from the Group combo wherever it's
    // shown. Sentinel value for SoloSynthPanel::currentGroup (never a real group name, which
    // always comes from gen_xwp1.py's GROUP_ORDER). Selecting it renders every group for the
    // block together, with every param as a plain full-width list row (no knob/fader split at
    // all) — GroupHeader (below) comes back specifically for this case, since multiple groups
    // are visible together again and need distinguishing.
    const juce::String kAllParamsLabel = "All Parameters";

    // Bold label + separator between groups — only used in "All Parameters" mode (see above);
    // a single selected group still doesn't need one, groupCombo already names it.
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
    constexpr int kGroupHeaderHeight = 24;
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
    addAndMakeVisible (groupLabel);
    addAndMakeVisible (groupCombo);
    addAndMakeVisible (syncButton);

    blockCombo.onChange = [this] { blockSelectionChanged(); };
    instanceCombo.onChange = [this] { instanceSelectionChanged(); };
    groupCombo.onChange = [this] { groupSelectionChanged(); };
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

    // dontSendNotification: clear() defaults to an ASYNC notification, which would otherwise
    // queue a redundant blockSelectionChanged() re-fire after the explicit call below already
    // runs synchronously (same class of fix as groupCombo.clear() below -- see its comment).
    blockCombo.clear (juce::dontSendNotification);
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
    // dontSendNotification: on a block switch instanceCombo already has a real prior selection,
    // so a bare clear() (async notification by default) queues a redundant instanceSelectionChanged()
    // that fires after this function has already set the new selection -- doubling every
    // rebuildParamControls()/sync burst on every block switch. Same fix as groupCombo.clear().
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
    buildGroupList();        // Chunk 7d item 1: group list depends on block, not instance
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
void SoloSynthPanel::buildGroupList()
{
    currentGroupList = casioxw::orderedGroupsForBlock (codec.model(), "soloSynth", currentBlock);

    const auto& model = codec.model();
    int totalParams = 0;
    for (const auto& p : model.all())
        if (p.section == "soloSynth" && p.block == currentBlock)
            ++totalParams;

    // Chunk 7e item 2: hide the Group selector entirely for blocks small enough that
    // group-at-a-time navigation is more friction than it's worth — see groupsToRender().
    groupSelectorNeeded = totalParams > kGroupSelectorParamThreshold;
    groupLabel.setVisible (groupSelectorNeeded);
    groupCombo.setVisible (groupSelectorNeeded);

    // dontSendNotification: ComboBox::clear() defaults to an ASYNC notification. On a second+
    // block switch, groupCombo already has a selection (id=1), so an unguarded clear() would
    // fire groupSelectionChanged() after this function (and blockSelectionChanged()) already
    // returned -- a redundant rebuildParamControls()/autoSyncIfConnected() racing the one this
    // call is already about to trigger deliberately. The explicit setSelectedId() below is the
    // one call that's meant to (re-)drive selection; onChange only needs to fire from an actual
    // user pick (see groupCombo.onChange in the constructor).
    groupCombo.clear (juce::dontSendNotification);

    // Chunk 7f: "All Parameters" is always item id=1 when the selector is shown at all -- the
    // owner's requested escape hatch back to the pre-split flat list. Real groups follow, id
    // offset by 1 (see groupSelectionChanged()'s matching id math).
    if (groupSelectorNeeded)
        groupCombo.addItem (kAllParamsLabel, 1);
    for (int i = 0; i < (int) currentGroupList.size(); ++i)
        groupCombo.addItem (currentGroupList[(size_t) i], i + (groupSelectorNeeded ? 2 : 1));

    if (! currentGroupList.empty())
    {
        // Default selection stays the first REAL group, not "All Parameters" -- this is an
        // extra option the owner can opt into, not a new default view.
        groupCombo.setSelectedId (groupSelectorNeeded ? 2 : 1, juce::dontSendNotification);
        currentGroup = currentGroupList.front();
    }
    else
    {
        currentGroup.clear();   // no groups at all for this block — rebuildParamControls() no-ops
    }
}

void SoloSynthPanel::groupSelectionChanged()
{
    const int id = groupCombo.getSelectedId();
    const int maxId = (int) currentGroupList.size() + (groupSelectorNeeded ? 1 : 0);
    if (id <= 0 || id > maxId)
        return;
    currentGroup = (groupSelectorNeeded && id == 1) ? kAllParamsLabel
                                                     : currentGroupList[(size_t) (id - (groupSelectorNeeded ? 2 : 1))];
    rebuildParamControls();
    autoSyncIfConnected();   // Chunk 7c item 1
}

std::vector<juce::String> SoloSynthPanel::groupsToRender() const
{
    if (! groupSelectorNeeded)
        return currentGroupList;   // small block: show every one of its groups together
    if (currentGroup == kAllParamsLabel)
        return currentGroupList;   // owner's explicit "All Parameters" choice
    return currentGroup.isEmpty() ? std::vector<juce::String> {}
                                   : std::vector<juce::String> { currentGroup };
}

//==============================================================================
void SoloSynthPanel::rebuildParamControls()
{
    stopTimer();
    outstandingSync.clear();
    envelopeSinks.clear();     // sinks capture raw EnvelopeDisplay* — must die before groupRows does
    groupRows.clear();
    controls.clear();          // destroying each ParamControl removes it from paramContainer automatically
    layoutItems.clear();

    const auto& model = codec.model();
    const int width = paramContainer.getWidth() > 0 ? paramContainer.getWidth() : 400;

    // Shared onValueChanged wiring for every control regardless of render mode (identical
    // send-SysEx + envelope-sink behaviour either way — only the visual layout differs).
    auto wireControl = [this] (ParamControl& ctrl)
    {
        const juce::String paramId = ctrl.paramId();
        const int instance = ctrl.instanceNumber();

        const auto sinkIt = envelopeSinks.find (syncKey (paramId, instance));
        const std::function<void (int)> envSink =
            sinkIt != envelopeSinks.end() ? sinkIt->second : std::function<void (int)> {};

        ctrl.onValueChanged = [this, paramId, instance, envSink] (int value)
        {
            const auto frame = codec.encode (paramId, instance, value);
            midiIO.sendFrame (frame);
            if (envSink)
                envSink (value);
        };
    };

    // Chunk 7f: "All Parameters" bypasses the knob/fader split entirely and reproduces the
    // pre-7d/7e giant flat list (every param as a plain full-width row) -- see kAllParamsLabel's
    // doc comment above. Only reachable when the Group selector is shown at all.
    const bool flatMode = groupSelectorNeeded && currentGroup == kAllParamsLabel;

    const auto groups = groupsToRender();
    for (size_t groupIdx = 0; groupIdx < groups.size(); ++groupIdx)
    {
        const auto& group = groups[groupIdx];

        // Params in this group, preserving their original JSON order (Chunk 7c item 4).
        std::vector<const casioxw::ParamInfo*> bucket;
        for (const auto& p : model.all())
            if (p.section == "soloSynth" && p.block == currentBlock && p.group == group)
                bucket.push_back (&p);
        if (bucket.empty())
            continue;

        // Flat mode gets its GroupHeader back (bold label + separator) -- with several groups
        // visible together again, they need distinguishing; a single selected group still
        // doesn't (groupCombo already names it).
        if (flatMode)
        {
            auto header = std::make_unique<GroupHeader> (group);
            paramContainer.addAndMakeVisible (*header);
            layoutItems.push_back ({ header.get(), kGroupHeaderHeight, {}, 0, 0, 0 });
            groupRows.push_back (std::move (header));
        }

        // Chunk 7e item 1: envelope-stage-ness is now a PER-PARAM check (casioxw::envelopeStageIds
        // ().isValid()), not a whole-group one — after merging "X Envelope" groups into their
        // parent "X" group, a single group can contain both the 9 envelope-shape points (which
        // still need the graphic + fader treatment) and plain modulation params (which don't).
        // In flatMode every param goes to listStyle regardless (the whole point of "All
        // Parameters" is bypassing the split) — but anyEnvParam is still tracked so the
        // EnvelopeDisplay graphic keeps showing, matching "the giant list view we had before".
        std::vector<const casioxw::ParamInfo*> listStyle, knobStyle, faderStyle;
        const casioxw::ParamInfo* anyEnvParam = nullptr;
        for (const auto* p : bucket)
        {
            const bool isEnvStage = casioxw::envelopeStageIds (p->id).isValid();
            if (isEnvStage && anyEnvParam == nullptr)
                anyEnvParam = p;

            if (flatMode)
                listStyle.push_back (p);
            else if (isEnvStage)
                faderStyle.push_back (p);
            else if (casioxw::decideControlKind (*p, currentInstance) == casioxw::ControlKind::Slider)
                knobStyle.push_back (p);
            else
                listStyle.push_back (p);
        }

        // Chunk 7c item 5: one read-only EnvelopeDisplay above this group's envelope-stage
        // controls (if any), seeded from the current instance's stage defaults so it isn't blank
        // before a sync.
        if (anyEnvParam != nullptr)
        {
            const auto stageIds = casioxw::envelopeStageIds (anyEnvParam->id);
            const auto* levelParam = model.find (stageIds.initLevel);   // defines the level axis range
            const int levelMin = levelParam != nullptr ? levelParam->range.min : -64;
            const int levelMax = levelParam != nullptr ? levelParam->range.max : 63;

            auto display = std::make_unique<EnvelopeDisplay> (levelMin, levelMax);

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
            layoutItems.push_back ({ displayPtr, kEnvelopeDisplayHeight, {}, 0, 0, kEnvelopeDisplayGap });
            groupRows.push_back (std::move (display));
        }

        // List-style controls first (toggles/combos read as qualitative selectors, not "dial in a
        // value" — always above the fader row / knob grid, deliberately and consistently).
        for (const auto* p : listStyle)
        {
            auto ctrl = std::make_unique<ParamControl> (model, *p, currentInstance);
            wireControl (*ctrl);
            paramContainer.addAndMakeVisible (*ctrl);
            layoutItems.push_back ({ ctrl.get(), kControlRowHeight + 2, {}, 0, 0, 0 });
            controls.push_back (std::move (ctrl));
        }

        // Vertical-fader row: the envelope-shape controls, side by side.
        if (! faderStyle.empty())
        {
            std::vector<ParamControl*> faderPtrs;
            for (const auto* p : faderStyle)
            {
                auto ctrl = std::make_unique<ParamControl> (model, *p, currentInstance,
                                                             ParamControl::RenderMode::VerticalFader);
                wireControl (*ctrl);
                paramContainer.addAndMakeVisible (*ctrl);
                faderPtrs.push_back (ctrl.get());
                controls.push_back (std::move (ctrl));
            }
            layoutItems.push_back ({ nullptr, 0, std::move (faderPtrs), kFaderCellWidth, kFaderCellHeight, 0 });
        }

        // Knob grid: everything else (plain numeric params, not envelope-shape points).
        if (! knobStyle.empty())
        {
            std::vector<ParamControl*> knobPtrs;
            for (const auto* p : knobStyle)
            {
                auto ctrl = std::make_unique<ParamControl> (model, *p, currentInstance,
                                                             ParamControl::RenderMode::Knob);
                wireControl (*ctrl);
                paramContainer.addAndMakeVisible (*ctrl);
                knobPtrs.push_back (ctrl.get());
                controls.push_back (std::move (ctrl));
            }
            layoutItems.push_back ({ nullptr, 0, std::move (knobPtrs), kKnobCellWidth, kKnobCellHeight, 0 });
        }

        // Extra breathing room before the next group, only when several render together.
        if (groupIdx + 1 < groups.size() && ! layoutItems.empty())
            layoutItems.back().gapAfter += kInterGroupGap;
    }

    layoutSequential (width);
}

int SoloSynthPanel::layoutSequential (int width)
{
    int y = 0;
    for (auto& item : layoutItems)
    {
        if (item.rowComponent != nullptr)
        {
            item.rowComponent->setBounds (0, y, width, item.rowHeight);
            y += item.rowHeight;
        }
        else if (! item.gridControls.empty())
        {
            const int cols = juce::jmax (1, width / item.cellWidth);
            int col = 0, rowY = y;
            for (auto* c : item.gridControls)
            {
                c->setBounds (col * item.cellWidth, rowY, item.cellWidth, item.cellHeight);
                if (++col >= cols)
                {
                    col = 0;
                    rowY += item.cellHeight;
                }
            }
            if (col != 0)
                rowY += item.cellHeight;   // a partially-filled last row still consumes a full row
            y = rowY;
        }
        y += item.gapAfter;
    }

    paramContainer.setSize (juce::jmax (400, width), y);
    return y;
}

void SoloSynthPanel::layoutParamContainerWidth()
{
    layoutSequential (juce::jmax (400, paramViewport.getWidth()));
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
    if (groupCombo.isVisible())
    {
        groupLabel.setBounds (navRow.removeFromLeft (55));
        groupCombo.setBounds (navRow.removeFromLeft (180));
        navRow.removeFromLeft (kRowGap);
    }
    syncButton.setBounds (navRow.removeFromLeft (100));

    bounds.removeFromTop (kRowGap);

    paramViewport.setBounds (bounds);
    layoutParamContainerWidth();
}
