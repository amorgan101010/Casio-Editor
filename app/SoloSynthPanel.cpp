#include "SoloSynthPanel.h"

namespace
{
    constexpr int kRowHeight = 28;
    constexpr int kRowGap = 6;
    constexpr int kMargin = 8;
    constexpr int kControlRowHeight = 26;
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
    }
    else
    {
        midiIO.closeInput();
        midiIO.closeOutput();
        statusLabel.setText ("Failed to open device(s)", juce::dontSendNotification);
    }
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
}

void SoloSynthPanel::instanceSelectionChanged()
{
    const int id = instanceCombo.getSelectedId();
    if (id <= 0)
        return;
    currentInstance = id;
    rebuildParamControls();
}

//==============================================================================
void SoloSynthPanel::rebuildParamControls()
{
    stopTimer();
    outstandingSync.clear();

    controls.clear();   // destroying each ParamControl removes it from paramContainer automatically

    const auto& model = codec.model();
    int y = 0;
    for (const auto& p : model.all())
    {
        if (p.section != "soloSynth" || p.block != currentBlock)
            continue;

        auto ctrl = std::make_unique<ParamControl> (model, p, currentInstance);
        ctrl->setBounds (0, y, paramContainer.getWidth() > 0 ? paramContainer.getWidth() : 400,
                         kControlRowHeight);
        y += kControlRowHeight + 2;

        const juce::String paramId = ctrl->paramId();
        const int instance = ctrl->instanceNumber();
        ctrl->onValueChanged = [this, paramId, instance] (int value)
        {
            const auto frame = codec.encode (paramId, instance, value);
            midiIO.sendFrame (frame);
        };

        paramContainer.addAndMakeVisible (*ctrl);
        controls.push_back (std::move (ctrl));
    }

    paramContainer.setSize (juce::jmax (400, paramViewport.getWidth()), y);
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
    paramContainer.setSize (paramViewport.getWidth(), paramContainer.getHeight());
    for (auto& c : controls)
        c->setSize (paramContainer.getWidth(), kControlRowHeight);
}
