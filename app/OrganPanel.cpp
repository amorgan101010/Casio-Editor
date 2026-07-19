#include "OrganPanel.h"
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

    // Must match ParamControl.cpp's kFaderHeight cell (kCompactLabelHeight + 130) — "the grid
    // just tiles the control's own size" convention SoloSynthPanel already established.
    constexpr int kFaderCellWidth = 100;
    constexpr int kFaderCellHeight = 164;

    constexpr const char* kSection = "drawbarOrgan";
    constexpr const char* kBlock = "DrawbarOrgan";
    constexpr const char* kDrawbarsGroup = "Drawbars";

    juce::String syncKey (const juce::String& paramId, int instance)
    {
        return paramId + "#" + juce::String (instance);
    }

    // Owner-confirmed on hardware 2026-07-18: writing organPosition through the SysEx cat=0x07
    // edit-buffer (act=0x01 IPS, what this panel used before) does NOT reach the live voice --
    // moving the app's fader visibly updates the synth's own edit-menu display and (per the
    // owner) DOES persist if the tone is saved and reloaded, but produces no audible change while
    // editing. The synth's own 9 physical drawbar knobs are wired to a completely different,
    // channel-voice NRPN path (022_XWOrgan.lua's g_orgModMidi["orgTW"] table, sent via sendNRNP,
    // 014_globalFunctions.lua:345) -- so Position writes now go out that way instead, matching
    // the owner's own suggestion ("point at those [hardware fader] parameters instead"). Reads
    // (Sync) still use the SysEx path below (unchanged) -- NRPN/CC are one-way performance
    // messages on this synth, there is no "read back the current NRPN value" mechanism, so the
    // edit-buffer register remains the only way to populate the fader from the current patch.
    constexpr int kOrganMidiChannel = 1;    // "Organ is always zone 1" -- 022_XWOrgan.lua:132
    constexpr int kDrawbarNrpnMsb = 0x40;   // g_orgModMidi["orgTW"].orgMSBid -- 011_initTables.lua:523

    // Ladder-tested on real hardware 2026-07-18 (.wolf/cerebrum.md addendum 31): the SysEx
    // "Select Bar" index (what organPosition's ParamModel instance 1-9 / params/xwp1.json's
    // ORGAN_DRAWBAR_LABELS now correctly reflects) groups drawbars by type -- 5 octave bars
    // (16',8',4',2',1') at SysEx instances 1-5, then 4 non-octave "mutation" bars (5 1/3',2 2/3',
    // 1 3/5',1 1/3') at instances 6-9. The NRPN performance path numbers the SAME 9 physical
    // drawbars in harmonic order instead (011_initTables.lua's orgTWdbar* table: LSB 0=16',
    // 1=5 1/3',2=8',3=4',4=2 2/3',5=2',6=1 3/5',7=1 1/3',8=1'). This table translates a SysEx
    // instance (1-9, i.e. array index 0-8) to the NRPN LSB for the SAME physical bar, so a write
    // triggered from the correctly-labelled fader lands on the correct drawbar despite the two
    // transports numbering them differently.
    constexpr int kSysExInstanceToNrpnLsb[9] = { 0, 2, 3, 5, 8, 1, 4, 6, 7 };

    // Owner feedback 2026-07-18: the type-grouped SysEx order above is correct DATA (which
    // register a given fader must read/write), but the owner wants the fader COLUMNS laid out
    // in the original harmonic order on screen (16',5 1/3',8',4',2 2/3',2',1 3/5',1 1/3',1'),
    // not grouped by octave/mutation. This is purely a display-order concern -- each entry here
    // is still the correct SysEx instance for that screen position, so labels/addressing/NRPN
    // translation are all untouched; only the ORDER the 9 ParamControls are built/laid out in
    // changes (left-to-right, wrapping into the same 5-then-4 grid as before).
    constexpr int kDrawbarDisplayOrder[9] = { 1, 6, 2, 3, 7, 4, 8, 9, 5 };

    void sendDrawbarNrpn (casioxw::MidiIO& midiIO, int sysExInstance1to9, int uiValue0to8)
    {
        // Scaled by 15 to spread the UI's 0-8 range across more of the CC's 0-127 resolution
        // (011_initTables.lua:71, g_xwModCalc["nrpn"].db is "(8-v)*15" -- NOT the same formula as
        // the unused V2SX 'db' entry at line 44, a separate table for a separate transport).
        //
        // HARDWARE-VERIFIED 2026-07-18 the (8-v) inversion in that Lua formula must NOT be
        // copied here: owner reported moving the app's fader to 0 made the synth show 8 (and
        // vice versa), and separately that setting a fader to 6 then re-syncing read back 2 --
        // exactly (8-6). That inversion in franky's Lua exists to translate FROM his own CTRLR
        // slider widget's convention (which ran the opposite direction) TO the wire's true
        // sense; our UI's 0=quiet/8=loud already matches the wire directly (same sense the
        // SysEx read side uses, hardware-confirmed in addendum 31), so inverting a second time
        // here just un-does it. Plain v*15, no inversion.
        const int nrpnLsb = kSysExInstanceToNrpnLsb[sysExInstance1to9 - 1];
        const int vmsb = juce::jlimit (0, 127, uiValue0to8 * 15);
        midiIO.sendMessageNow (juce::MidiMessage::controllerEvent (kOrganMidiChannel, 0x63, kDrawbarNrpnMsb));
        midiIO.sendMessageNow (juce::MidiMessage::controllerEvent (kOrganMidiChannel, 0x62, nrpnLsb));
        midiIO.sendMessageNow (juce::MidiMessage::controllerEvent (kOrganMidiChannel, 0x06, vmsb));
    }

    // Bold label + separator between the block's groups — same visual language as
    // SoloSynthPanel/PCMEnginePanel's own GroupHeader, kept as a small local duplicate rather
    // than a shared header (see PCMEnginePanel.cpp for the same reasoning: not worth coupling
    // this deliberately-independent panel to another panel's translation unit for ~15 lines).
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
OrganPanel::OrganPanel (casioxw::SysExCodec& codecIn, casioxw::MidiIO& midiIOIn)
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
}

OrganPanel::~OrganPanel()
{
    stopTimer();
}

void OrganPanel::paint (juce::Graphics& g)
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
void OrganPanel::buildParamControls()
{
    stopTimer();
    outstandingSync.clear();
    groupHeaders.clear();
    controls.clear();   // destroying each ParamControl removes it from paramContainer automatically
    layoutItems.clear();

    const auto& model = codec.model();
    const auto groups = casioxw::orderedGroupsForBlock (model, kSection, kBlock);

    auto wireControl = [this] (ParamControl& ctrl)
    {
        const juce::String paramId = ctrl.paramId();
        const int instance = ctrl.instanceNumber();
        ctrl.onValueChanged = [this, paramId, instance] (int value)
        {
            midiIO.sendFrame (codec.encode (paramId, instance, value));
        };
    };

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
        layoutItems.push_back ({ header.get(), kGroupHeaderHeight, {}, 0, 0, 0 });
        groupHeaders.push_back (std::move (header));

        if (group == kDrawbarsGroup)
        {
            // The drawbars: every instance of "Position" shown SIMULTANEOUS as a compact
            // vertical fader (the physical drawbar-organ metaphor), captioned by foot length
            // rather than the repeated param name — see ParamControl's labelOverride.
            for (const auto* p : bucket)
            {
                std::vector<ParamControl*> faderPtrs;
                for (int instance : kDrawbarDisplayOrder)
                {
                    const juce::String caption = (instance - 1) < p->instanceLabels.size()
                        ? p->instanceLabels[instance - 1] : juce::String (instance);
                    // invertVerticalFader=true: owner feedback (2026-07-18) -- physical drawbars
                    // are pulled DOWN/out for more volume, the opposite of a mixing-console
                    // fader's usual up-is-more convention.
                    auto ctrl = std::make_unique<ParamControl> (model, *p, instance,
                                                                 ParamControl::RenderMode::VerticalFader,
                                                                 caption, true);
                    // NOT wireControl() -- drawbar writes go out via the live NRPN performance
                    // path (sendDrawbarNrpn), not the SysEx edit-buffer write every other control
                    // uses. `instance` here is the SysEx instance (1-9); sendDrawbarNrpn
                    // translates it to the correct NRPN LSB internally.
                    ctrl->onValueChanged = [this, instance] (int value)
                    {
                        sendDrawbarNrpn (midiIO, instance, value);
                    };
                    paramContainer.addAndMakeVisible (*ctrl);
                    faderPtrs.push_back (ctrl.get());
                    controls.push_back (std::move (ctrl));
                }
                layoutItems.push_back ({ nullptr, 0, std::move (faderPtrs), kFaderCellWidth, kFaderCellHeight, 0 });
            }
        }
        else
        {
            for (const auto* p : bucket)
            {
                auto ctrl = std::make_unique<ParamControl> (model, *p, 1);
                wireControl (*ctrl);
                paramContainer.addAndMakeVisible (*ctrl);
                layoutItems.push_back ({ ctrl.get(), kControlRowHeight, {}, 0, 0, 0 });
                controls.push_back (std::move (ctrl));
            }
        }

        if (groupIdx + 1 < groups.size() && ! layoutItems.empty())
            layoutItems.back().gapAfter += kInterGroupGap;
    }

    layoutSequential (paramContainer.getWidth() > 0 ? paramContainer.getWidth() : 400);
}

int OrganPanel::layoutSequential (int width)
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

void OrganPanel::layoutParamContainerWidth()
{
    layoutSequential (juce::jmax (400, paramViewport.getWidth()));
}

//==============================================================================
void OrganPanel::syncButtonClicked()
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

void OrganPanel::timerCallback()
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
void OrganPanel::resized()
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
