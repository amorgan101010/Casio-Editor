#include "ArrangerPanel.h"

#include "casioxw/Scheduler.h"

#include <algorithm>
#include <cmath>

namespace
{
    // ---- shared row/header column geometry --------------------------------------------------
    // Both RowWidgets::resized() and ArrangerPanel's column-header row lay out against these same
    // x-offsets/widths so the header labels land directly above their column, matching how
    // SequencerPanel aligns its step-grid header to the step columns below it.
    constexpr int kRowHeight       = 44;
    constexpr int kRowGap          = 4;
    constexpr int kHeaderHeight    = 20;
    constexpr int kIndexWidth      = 28;
    constexpr int kLabelWidth      = 120;
    constexpr int kFileComboWidth  = 108;
    constexpr int kRepeatWidth     = 100;
    constexpr int kMuteChipWidth   = 30;
    constexpr int kMuteGroupGap    = 10;    // extra gap between the synth/drum/pcm mute clusters
    constexpr int kRemoveWidth     = 24;
    constexpr int kColGap          = 8;

    // ---- fixed lane roster, mirroring casioxw::kSongLaneCount/SequencerPanel's kDrumTracks/
    // kPcmTracks (SequencerPanel.cpp) -- short captions painted on the chip, full names as tooltip,
    // the same abbreviation+tooltip convention MiniKnob established for HexLayerPanel. ------------
    struct LaneDef { const char* caption; const char* tooltip; };
    constexpr LaneDef kLanes[casioxw::kSongLaneCount] = {
        { "SY", "Solo Synth" },
        { "D1", "Drum 1" }, { "D2", "Drum 2" }, { "D3", "Drum 3" }, { "D4", "Drum 4" }, { "D5", "Drum 5" },
        { "B",  "Bass" }, { "S1", "Solo 1" }, { "S2", "Solo 2" }, { "CH", "Chords" },
    };

    constexpr double kSchedulerTickMs    = 12.0;
    constexpr double kLookaheadMs        = 60.0;
    constexpr double kStartLeadMs        = 50.0;
    constexpr double kStartPrimeFloorMs  = 1500.0;
    constexpr double kScheduleSampleRate = 1000.0;

    juce::String defaultDrumsDataFormat()   { return "casioxw-drum-sequence"; }
    juce::String defaultPcmDataFormat()     { return "casioxw-pcm-tracks"; }
    juce::String defaultSetDataFormat()     { return "casioxw-sequence-set-ref"; }
}

//==============================================================================
void ArrangerPanel::RowWidgets::resized()
{
    auto b = getLocalBounds();
    int x = 0;
    const int midY = b.getHeight() / 2;

    indexLabel.setBounds (x, 0, kIndexWidth, b.getHeight()); x += kIndexWidth + kColGap;
    labelEditor.setBounds (x, midY - 11, kLabelWidth, 22); x += kLabelWidth + kColGap;

    setCombo.setBounds (x, midY - 11, kFileComboWidth, 22);
    const int contentX = x;
    x += kFileComboWidth + kColGap;

    soloCombo.setBounds (x, midY - 11, kFileComboWidth, 22); x += kFileComboWidth + kColGap;
    drumsCombo.setBounds (x, midY - 11, kFileComboWidth, 22); x += kFileComboWidth + kColGap;
    pcmCombo.setBounds (x, midY - 11, kFileComboWidth, 22); x += kFileComboWidth + kColGap;
    fromSetLabel.setBounds (contentX + kFileComboWidth + kColGap, midY - 11,
                            kFileComboWidth * 3 + kColGap * 2, 22);

    repeatSlider.setBounds (x, midY - 11, kRepeatWidth, 22); x += kRepeatWidth + kColGap;

    for (int i = 0; i < casioxw::kSongLaneCount; ++i)
    {
        muteChips[(size_t) i].setBounds (x, midY - 13, kMuteChipWidth, 26);
        x += kMuteChipWidth + 3;
        if (i == 0 || i == 5)   // group boundary after the synth lane, and after the 5 drum lanes
            x += kMuteGroupGap;
    }

    removeButton.setBounds (b.getWidth() - kRemoveWidth, midY - 11, kRemoveWidth, 22);
}

//==============================================================================
ArrangerPanel::ArrangerPanel (casioxw::SysExCodec& codecIn, casioxw::MidiIO& midiIOIn)
    : codec (codecIn), midiIO (midiIOIn)
{
    titleLabel.setFont (EditorFonts::header (14.0f));
    titleLabel.setColour (juce::Label::textColourId, EditorColours::textHeader);
    addAndMakeVisible (titleLabel);

    for (auto* header : { &colLabelHeader, &colContentHeader, &colRepeatHeader, &colMuteHeader })
    {
        header->setFont (EditorFonts::header (10.0f));
        header->setColour (juce::Label::textColourId, EditorColours::textMuted);
        addAndMakeVisible (*header);
    }

    playStopButton.onClick = [this] { playing ? stop() : play(); };
    addAndMakeVisible (playStopButton);

    addRowButton.onClick = [this] { addRow(); };
    addAndMakeVisible (addRowButton);

    refreshButton.onClick = [this] { refreshFileCombos(); };
    addAndMakeVisible (refreshButton);

    saveButton.onClick = [this] { saveSongToFile(); };
    addAndMakeVisible (saveButton);

    loadButton.onClick = [this] { loadSongFromFile(); };
    addAndMakeVisible (loadButton);

    tempoSlider.setRange (30.0, 300.0, 1.0);
    tempoSlider.setValue ((double) song.tempoBpm, juce::dontSendNotification);
    tempoSlider.onValueChange = [this] { song.tempoBpm = (int) tempoSlider.getValue(); };
    addAndMakeVisible (tempoSlider);
    addAndMakeVisible (tempoLabel);

    statusLabel.setFont (EditorFonts::mono (11.0f));
    statusLabel.setColour (juce::Label::textColourId, EditorColours::textMuted);
    addAndMakeVisible (statusLabel);

    viewport.setViewedComponent (&rowContainer, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    // Start with one empty row -- an arrangement with zero rows has nothing to play, so a fresh
    // panel gives the owner something to fill in rather than an empty table.
    song.rows.push_back ({});
    rebuildRowWidgets();

    setSize (1490, 500);
}

ArrangerPanel::~ArrangerPanel()
{
    stopTimer();
}

//==============================================================================
void ArrangerPanel::paint (juce::Graphics& g)
{
    g.fillAll (EditorColours::chassisBg);

    // Currently-playing row: a translucent playhead wash across the row's full width, same
    // technique/colour SequencerPanel uses for its step-grid playhead (EditorColours::playhead),
    // so "something is actively playing here" reads consistently across both transports.
    if (playing && currentPosition.row >= 0 && currentPosition.row < (int) rowWidgets.size())
    {
        auto* row = rowWidgets[(size_t) currentPosition.row].get();
        auto rowBoundsInViewport = row->getBounds() + rowContainer.getPosition() + viewport.getPosition()
                                 - juce::Point<int> (viewport.getViewPositionX(), viewport.getViewPositionY());
        g.setColour (EditorColours::playhead.withAlpha (0.22f));
        g.fillRect (rowBoundsInViewport.withX (viewport.getX()).withWidth (viewport.getWidth()));
    }
}

void ArrangerPanel::resized()
{
    auto b = getLocalBounds().reduced (8);

    auto headerRow = b.removeFromTop (24);
    titleLabel.setBounds (headerRow.removeFromLeft (100));
    playStopButton.setBounds (headerRow.removeFromLeft (70));
    headerRow.removeFromLeft (12);
    tempoLabel.setBounds (headerRow.removeFromLeft (30));
    tempoSlider.setBounds (headerRow.removeFromLeft (140));
    headerRow.removeFromLeft (12);
    addRowButton.setBounds (headerRow.removeFromLeft (60));
    headerRow.removeFromLeft (6);
    refreshButton.setBounds (headerRow.removeFromLeft (70));
    headerRow.removeFromLeft (12);
    saveButton.setBounds (headerRow.removeFromLeft (80));
    headerRow.removeFromLeft (6);
    loadButton.setBounds (headerRow.removeFromLeft (80));
    statusLabel.setBounds (headerRow);

    b.removeFromTop (6);

    auto colHeaderRow = b.removeFromTop (kHeaderHeight);
    int x = colHeaderRow.getX() + kIndexWidth + kColGap;
    colLabelHeader.setBounds (x, colHeaderRow.getY(), kLabelWidth, kHeaderHeight); x += kLabelWidth + kColGap;
    colContentHeader.setBounds (x, colHeaderRow.getY(), kFileComboWidth * 4 + kColGap * 3, kHeaderHeight);
    x += kFileComboWidth * 4 + kColGap * 3 + kColGap;
    colRepeatHeader.setBounds (x, colHeaderRow.getY(), kRepeatWidth, kHeaderHeight); x += kRepeatWidth + kColGap;
    colMuteHeader.setBounds (x, colHeaderRow.getY(), colHeaderRow.getRight() - x, kHeaderHeight);

    b.removeFromTop (4);
    viewport.setBounds (b);
    layoutRowContainer();
}

void ArrangerPanel::layoutRowContainer()
{
    const int width = viewport.getWidth() - viewport.getScrollBarThickness();
    const int height = (int) rowWidgets.size() * (kRowHeight + kRowGap);
    rowContainer.setSize (juce::jmax (width, 400), juce::jmax (height, 1));

    int y = 0;
    for (auto& w : rowWidgets)
    {
        w->setBounds (0, y, rowContainer.getWidth(), kRowHeight);
        y += kRowHeight + kRowGap;
    }
}

//==============================================================================
void ArrangerPanel::addRow()
{
    song.rows.push_back ({});
    rebuildRowWidgets();
    refreshFileCombos();
}

void ArrangerPanel::removeRow (RowWidgets* widgets)
{
    const int idx = indexOfWidgets (widgets);
    if (idx < 0)
        return;
    song.rows.erase (song.rows.begin() + idx);
    if (song.rows.empty())
        song.rows.push_back ({});   // never let the table go fully empty -- nothing to click "+ Row" on
    rebuildRowWidgets();
}

int ArrangerPanel::indexOfWidgets (const RowWidgets* w) const
{
    for (int i = 0; i < (int) rowWidgets.size(); ++i)
        if (rowWidgets[(size_t) i].get() == w)
            return i;
    return -1;
}

void ArrangerPanel::rebuildRowWidgets()
{
    rowContainer.removeAllChildren();
    rowWidgets.clear();

    for (int i = 0; i < (int) song.rows.size(); ++i)
    {
        auto w = std::make_unique<RowWidgets>();
        configureRowWidgets (*w);
        rowContainer.addAndMakeVisible (*w);
        rowWidgets.push_back (std::move (w));
        syncRowWidgetsFromSong (i);
    }

    refreshFileCombos();
    layoutRowContainer();
}

void ArrangerPanel::configureRowWidgets (RowWidgets& w)
{
    w.indexLabel.setFont (EditorFonts::mono (13.0f, true));
    w.indexLabel.setColour (juce::Label::textColourId, EditorColours::textHeader);
    w.indexLabel.setJustificationType (juce::Justification::centred);
    w.addAndMakeVisible (w.indexLabel);

    w.labelEditor.setFont (EditorFonts::mono (12.0f));
    w.labelEditor.setTextToShowWhenEmpty (juce::CharPointer_UTF8 ("\xe2\x80\x94"), EditorColours::textMuted);
    w.labelEditor.onTextChange = [this, &w] { onRowFieldChanged (w); };
    w.addAndMakeVisible (w.labelEditor);

    w.setCombo.onChange = [this, &w]
    {
        const bool setActive = w.setCombo.getSelectedId() > 1;
        w.soloCombo.setVisible (! setActive);
        w.drumsCombo.setVisible (! setActive);
        w.pcmCombo.setVisible (! setActive);
        w.fromSetLabel.setVisible (setActive);
        onRowFieldChanged (w);
    };
    w.addAndMakeVisible (w.setCombo);

    for (auto* combo : { &w.soloCombo, &w.drumsCombo, &w.pcmCombo })
    {
        combo->onChange = [this, &w] { onRowFieldChanged (w); };
        w.addAndMakeVisible (*combo);
    }

    w.fromSetLabel.setFont (EditorFonts::mono (11.0f, true));
    w.fromSetLabel.setColour (juce::Label::textColourId, EditorColours::textMuted);
    w.fromSetLabel.setJustificationType (juce::Justification::centred);
    w.fromSetLabel.setVisible (false);
    w.addAndMakeVisible (w.fromSetLabel);

    w.repeatSlider.setRange (1.0, 99.0, 1.0);
    w.repeatSlider.setValue (1.0, juce::dontSendNotification);
    w.repeatSlider.setTextValueSuffix ("x");
    w.repeatSlider.onValueChange = [this, &w] { onRowFieldChanged (w); };
    w.addAndMakeVisible (w.repeatSlider);

    for (int i = 0; i < casioxw::kSongLaneCount; ++i)
    {
        auto& chip = w.muteChips[(size_t) i];
        chip.setButtonText (kLanes[i].caption);
        chip.setTooltip (kLanes[i].tooltip);
        chip.setClickingTogglesState (true);
        // Inverted from the usual "toggled-on == bright" convention: the chip's DATA meaning is
        // "muted" (matching casioxw::SongRow::laneMuted / SequencerPanel's own Mute buttons), so
        // toggled-ON (muted) gets the dim idle colour and toggled-OFF (audible) gets the bright one.
        chip.setColour (juce::TextButton::buttonColourId, EditorColours::filledStep);
        chip.setColour (juce::TextButton::buttonOnColourId, EditorColours::idleStep);
        chip.setColour (juce::TextButton::textColourOffId, EditorColours::base03);
        chip.setColour (juce::TextButton::textColourOnId, EditorColours::textMuted);
        chip.onClick = [this, &w] { onRowFieldChanged (w); };
        w.addAndMakeVisible (chip);
    }

    w.removeButton.setColour (juce::TextButton::textColourOffId, EditorColours::base01);
    w.removeButton.onClick = [this, &w] { removeRow (&w); };
    w.addAndMakeVisible (w.removeButton);
}

void ArrangerPanel::syncRowWidgetsFromSong (int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= (int) rowWidgets.size())
        return;
    const auto& row = song.rows[(size_t) rowIndex];
    auto& w = *rowWidgets[(size_t) rowIndex];

    w.indexLabel.setText (juce::String (rowIndex + 1), juce::dontSendNotification);
    w.labelEditor.setText (row.label, juce::dontSendNotification);
    w.repeatSlider.setValue ((double) row.repeatCount, juce::dontSendNotification);
    for (int i = 0; i < casioxw::kSongLaneCount; ++i)
        w.muteChips[(size_t) i].setToggleState (row.laneMuted[(size_t) i], juce::dontSendNotification);

    const bool setActive = row.setFile.isNotEmpty();
    w.soloCombo.setVisible (! setActive);
    w.drumsCombo.setVisible (! setActive);
    w.pcmCombo.setVisible (! setActive);
    w.fromSetLabel.setVisible (setActive);
}

void ArrangerPanel::onRowFieldChanged (RowWidgets& w)
{
    const int idx = indexOfWidgets (&w);
    if (idx < 0)
        return;
    auto& row = song.rows[(size_t) idx];

    row.label = w.labelEditor.getText();
    row.repeatCount = juce::jlimit (1, 99, (int) w.repeatSlider.getValue());
    for (int i = 0; i < casioxw::kSongLaneCount; ++i)
        row.laneMuted[(size_t) i] = w.muteChips[(size_t) i].getToggleState();

    row.setFile   = w.setCombo.getSelectedId() > 1   ? w.setCombo.getText()   : juce::String();
    row.soloFile  = w.soloCombo.getSelectedId() > 1  ? w.soloCombo.getText()  : juce::String();
    row.drumsFile = w.drumsCombo.getSelectedId() > 1 ? w.drumsCombo.getText() : juce::String();
    row.pcmFile   = w.pcmCombo.getSelectedId() > 1   ? w.pcmCombo.getText()   : juce::String();
}

void ArrangerPanel::populateFileCombo (juce::ComboBox& combo, const juce::String& wildcard,
                                       const juce::String& currentValue) const
{
    const auto previouslySelected = combo.getText();
    const auto keep = currentValue.isNotEmpty() ? currentValue : previouslySelected;

    combo.clear (juce::dontSendNotification);
    combo.addItem ("(none)", 1);

    auto dir = sequenceDirectory;
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    juce::StringArray names;
    for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, wildcard))
        names.add (f.getFileName());
    names.sort (true);

    bool keepFound = keep.isEmpty();
    int id = 2;
    for (const auto& name : names)
    {
        combo.addItem (name, id);
        if (name == keep)
            keepFound = true;
        ++id;
    }

    if (keep.isNotEmpty() && ! keepFound)
        combo.addItem (keep, id);   // referenced file is missing from the directory scan -- keep it
                                    // visible/selected rather than silently dropping the row's data

    if (keep.isEmpty())
        combo.setSelectedId (1, juce::dontSendNotification);
    else
        combo.setText (keep, juce::dontSendNotification);
}

void ArrangerPanel::refreshFileCombos()
{
    for (int i = 0; i < (int) rowWidgets.size(); ++i)
    {
        const auto& row = song.rows[(size_t) i];
        auto& w = *rowWidgets[(size_t) i];
        populateFileCombo (w.setCombo, "*.xwset", row.setFile);
        populateFileCombo (w.soloCombo, "*.xwseq", row.soloFile);
        populateFileCombo (w.drumsCombo, "*.xwdrm", row.drumsFile);
        populateFileCombo (w.pcmCombo, "*.xwpcm", row.pcmFile);
    }
}

void ArrangerPanel::setSequenceDirectory (const juce::File& dir)
{
    sequenceDirectory = dir;
    refreshFileCombos();
}

juce::File ArrangerPanel::resolveFile (const juce::String& relativeName) const
{
    auto dir = sequenceDirectory;
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    return dir.getChildFile (relativeName);
}

//==============================================================================
namespace
{
    // Parses a .xwdrm file's "tracks" array (SequencerPanel::serializeDrumsToJson's shape) into
    // plain DrumTrackData, independent of any live SequencerPanel widgets.
    bool parseDrumTracks (const juce::String& text, std::array<ArrangerPanel::DrumTrackData, 5>& out)
    {
        const auto parsed = juce::JSON::parse (text);
        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr || obj->getProperty ("format").toString() != defaultDrumsDataFormat())
            return false;

        const auto* tracks = obj->getProperty ("tracks").getArray();
        if (tracks == nullptr)
            return false;

        const int n = juce::jmin ((int) out.size(), tracks->size());
        for (int i = 0; i < n; ++i)
        {
            auto* t = (*tracks)[i].getDynamicObject();
            if (t == nullptr)
                continue;

            auto& track = out[(size_t) i];
            track.channel = juce::jlimit (1, 16, (int) t->getProperty ("channel"));
            track.note = juce::jlimit (0, 127, (int) t->getProperty ("note"));
            track.baseVelocity = juce::jlimit (1, 127, (int) t->getProperty ("baseVelocity"));

            if (const auto* steps = t->getProperty ("steps").getArray())
            {
                const int stepCount = juce::jmin (16, steps->size());
                for (int s = 0; s < stepCount; ++s)
                    track.steps[(size_t) s] = (bool) steps->getReference (s);
            }
            if (const auto* locks = t->getProperty ("velocityLocks").getArray())
            {
                const int lockCount = juce::jmin (16, locks->size());
                for (int s = 0; s < lockCount; ++s)
                {
                    const auto& v = locks->getReference (s);
                    track.velocityLocks[(size_t) s] = v.isVoid() ? std::optional<int>() : std::optional<int> ((int) v);
                }
            }
        }
        return true;
    }

    // Parses a .xwpcm file's "tracks" array -- each entry is a full sequenceToJson() object
    // (SequencerPanel::serializePcmTracksToJson's shape) -- into plain casioxw::Sequence copies.
    bool parsePcmTracks (const juce::String& text, std::array<std::optional<casioxw::Sequence>, 4>& out)
    {
        const auto parsed = juce::JSON::parse (text);
        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr || obj->getProperty ("format").toString() != defaultPcmDataFormat())
            return false;

        const auto* tracks = obj->getProperty ("tracks").getArray();
        if (tracks == nullptr)
            return false;

        const int n = juce::jmin ((int) out.size(), tracks->size());
        for (int i = 0; i < n; ++i)
        {
            const auto& trackVar = (*tracks)[i];
            if (trackVar.getDynamicObject() == nullptr)
                continue;   // this track slot wasn't saved -- leave it unloaded
            out[(size_t) i] = casioxw::sequenceFromJson (juce::JSON::toString (trackVar));
        }
        return true;
    }
}

ArrangerPanel::RowRuntime ArrangerPanel::loadRowRuntime (const casioxw::SongRow& row) const
{
    RowRuntime runtime;

    if (row.setFile.isNotEmpty())
    {
        const auto file = resolveFile (row.setFile);
        const auto parsed = juce::JSON::parse (file.loadFileAsString());
        auto* obj = parsed.getDynamicObject();
        if (obj != nullptr && obj->getProperty ("format").toString() == defaultSetDataFormat())
        {
            runtime.solo = casioxw::sequenceFromJson (juce::JSON::toString (obj->getProperty ("solo")));
            runtime.hasDrums = parseDrumTracks (juce::JSON::toString (obj->getProperty ("drums")), runtime.drums);
            runtime.hasPcm = parsePcmTracks (juce::JSON::toString (obj->getProperty ("pcm")), runtime.pcm);
        }
        return runtime;
    }

    if (row.soloFile.isNotEmpty())
        runtime.solo = casioxw::sequenceFromJson (resolveFile (row.soloFile).loadFileAsString());
    if (row.drumsFile.isNotEmpty())
        runtime.hasDrums = parseDrumTracks (resolveFile (row.drumsFile).loadFileAsString(), runtime.drums);
    if (row.pcmFile.isNotEmpty())
        runtime.hasPcm = parsePcmTracks (resolveFile (row.pcmFile).loadFileAsString(), runtime.pcm);

    return runtime;
}

//==============================================================================
std::vector<juce::MidiMessage> ArrangerPanel::paramMessages (const juce::String& paramId, int instance,
                                                             int value) const
{
    // Same seam as SequencerPanel::paramMessages: encode via the codec's SysEx path.
    const auto frame = codec.encode (paramId, instance, value);
    if (frame.size() < 3)
        return {};
    return { juce::MidiMessage::createSysExMessage (frame.data() + 1, (int) frame.size() - 2) };
}

void ArrangerPanel::sendParamNow (const juce::String& paramId, int instance, int value)
{
    for (const auto& m : paramMessages (paramId, instance, value))
        midiIO.sendMessageNow (m);
}

void ArrangerPanel::resetCurrentRuntimeToBase()
{
    if (currentRuntime.solo.has_value())
        for (const auto& lp : currentRuntime.solo->lockable)
            sendParamNow (lp.paramId, lp.instance, lp.baseValue);
}

void ArrangerPanel::play()
{
    if (playing)
        return;
    if (! midiIO.isOutputOpen())
    {
        statusLabel.setText ("Not connected - open a MIDI output on the Solo Synth tab first",
                             juce::dontSendNotification);
        return;
    }

    if (beforePlay)
        beforePlay();

    playing = true;
    songEnded = false;
    currentPosition = { 0, 0 };
    runtimeLoadedForRow = -1;
    midiIO.startPlaybackThread();

    transportStartMs = (double) juce::Time::getMillisecondCounter() + kStartLeadMs;
    nextStepStartMs = 0.0;
    nextStepIndex = 0;
    prevStepIndex = casioxw::kPrevStepFresh;   // arranger doesn't sync base values from hardware first,
                                               // so the device's actual state is unknown at play-start
    playStopButton.setButtonText ("Stop");
    playStopButton.setColour (juce::TextButton::buttonColourId, EditorColours::green);
    playStopButton.setColour (juce::TextButton::textColourOffId, EditorColours::base03);
    statusLabel.setText ("Playing", juce::dontSendNotification);

    feedLookahead (juce::jmax (kStartPrimeFloorMs, 200.0));
    startTimer ((int) kSchedulerTickMs);
    repaint();
}

void ArrangerPanel::stop()
{
    if (! playing)
        return;

    stopTimer();
    playing = false;
    songEnded = false;
    playStopButton.setButtonText ("Play");
    playStopButton.removeColour (juce::TextButton::buttonColourId);
    playStopButton.removeColour (juce::TextButton::textColourOffId);

    midiIO.stopPlaybackThread();
    if (currentRuntime.solo.has_value())
        midiIO.sendAllNotesOff (currentRuntime.solo->channel);
    if (currentRuntime.hasDrums)
        for (const auto& t : currentRuntime.drums)
            midiIO.sendAllNotesOff (t.channel);
    if (currentRuntime.hasPcm)
        for (const auto& t : currentRuntime.pcm)
            if (t.has_value())
                midiIO.sendAllNotesOff (t->channel);

    resetCurrentRuntimeToBase();
    statusLabel.setText ("Stopped", juce::dontSendNotification);
    repaint();
}

void ArrangerPanel::timerCallback()
{
    if (! playing)
        return;

    feedLookahead (kLookaheadMs);
    repaint();

    if (songEnded && (double) juce::Time::getMillisecondCounter() >= transportStartMs + nextStepStartMs)
    {
        statusLabel.setText ("Song finished", juce::dontSendNotification);
        stop();
    }
}

void ArrangerPanel::feedLookahead (double lookaheadMs)
{
    const double now = (double) juce::Time::getMillisecondCounter();
    const double horizon = now + lookaheadMs;

    while (! songEnded && transportStartMs + nextStepStartMs < horizon)
    {
        if (currentPosition.row != runtimeLoadedForRow)
        {
            // Row boundary: load the new row's files fresh (independent copy, never touching
            // SequencerPanel's own live sequence/tracks) and reset the OLD row's params to base
            // first, so a p-lock from the previous row can never bleed into this one.
            resetCurrentRuntimeToBase();
            currentRuntime = loadRowRuntime (song.rows[(size_t) currentPosition.row]);
            runtimeLoadedForRow = currentPosition.row;
            prevStepIndex = casioxw::kPrevStepFresh;   // unknown device state relative to the new lane set
        }

        const auto& row = song.rows[(size_t) currentPosition.row];

        // The song's tempoBpm is the single clock; only the RATE (stepsPerBeat) is taken from
        // whichever loaded lane defines it, same convention SequencerPanel uses to keep drum/pcm
        // lanes locked to the main sequence's rate.
        const int stepsPerBeat = currentRuntime.solo.has_value() ? currentRuntime.solo->stepsPerBeat
                                : currentRuntime.hasDrums ? 4
                                                          : 4;
        if (currentRuntime.solo.has_value())
        {
            currentRuntime.solo->tempoBpm = song.tempoBpm;
            currentRuntime.solo->stepsPerBeat = stepsPerBeat;
        }
        for (auto& t : currentRuntime.pcm)
            if (t.has_value())
            {
                t->tempoBpm = song.tempoBpm;
                t->stepsPerBeat = stepsPerBeat;
            }

        casioxw::Sequence clockRef;
        clockRef.tempoBpm = song.tempoBpm;
        clockRef.stepsPerBeat = stepsPerBeat;
        const double stepMs = casioxw::stepIntervalMs (clockRef);
        const double drumGateMs = juce::jmax (1.0, stepMs * 0.5);

        juce::MidiBuffer buffer;

        if (currentRuntime.solo.has_value() && ! row.laneMuted[(size_t) casioxw::kSongSynthLane])
        {
            for (const auto& e : casioxw::scheduleStep (*currentRuntime.solo, nextStepIndex, prevStepIndex, nextStepStartMs))
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
                        for (const auto& m : paramMessages (e.paramId, e.instance, e.value))
                            buffer.addEvent (m, samplePos);
                        break;
                }
            }
        }

        if (currentRuntime.hasDrums)
        {
            for (int i = 0; i < (int) currentRuntime.drums.size(); ++i)
            {
                if (row.laneMuted[(size_t) (casioxw::kSongDrumLaneStart + i)])
                    continue;
                const auto& t = currentRuntime.drums[(size_t) i];
                if (! t.steps[(size_t) nextStepIndex])
                    continue;

                int velocity = t.baseVelocity;
                if (const auto locked = t.velocityLocks[(size_t) nextStepIndex])
                    velocity = *locked;
                velocity = juce::jlimit (1, 127, velocity);

                const int onPos = (int) std::llround (nextStepStartMs);
                const int offPos = (int) std::llround (nextStepStartMs + drumGateMs);
                buffer.addEvent (juce::MidiMessage::noteOn (t.channel, t.note, (juce::uint8) velocity), onPos);
                buffer.addEvent (juce::MidiMessage::noteOff (t.channel, t.note), offPos);
            }
        }

        if (currentRuntime.hasPcm)
        {
            for (int i = 0; i < (int) currentRuntime.pcm.size(); ++i)
            {
                if (row.laneMuted[(size_t) (casioxw::kSongPcmLaneStart + i)] || ! currentRuntime.pcm[(size_t) i].has_value())
                    continue;

                for (const auto& e : casioxw::scheduleStep (*currentRuntime.pcm[(size_t) i], nextStepIndex,
                                                            prevStepIndex, nextStepStartMs))
                {
                    const int samplePos = (int) std::llround (e.timeMs);
                    if (e.type == casioxw::ScheduledEvent::Type::noteOn)
                        buffer.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.velocity), samplePos);
                    else if (e.type == casioxw::ScheduledEvent::Type::noteOff)
                        buffer.addEvent (juce::MidiMessage::noteOff (e.channel, e.note), samplePos);
                }
            }
        }

        if (! buffer.isEmpty())
            midiIO.scheduleBlock (buffer, transportStartMs, kScheduleSampleRate);

        prevStepIndex = nextStepIndex;
        nextStepIndex = (nextStepIndex + 1) % 16;
        nextStepStartMs += stepMs;

        if (nextStepIndex == 0)   // this row's 16 steps just finished one full loop
        {
            const auto next = casioxw::advanceSongPosition (song, currentPosition);
            if (! next.has_value())
                songEnded = true;   // let already-scheduled note-offs finish, then auto-stop (see timerCallback)
            else
                currentPosition = *next;
        }
    }
}

//==============================================================================
void ArrangerPanel::saveSongToFile()
{
    auto dir = sequenceDirectory;
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    fileChooser = std::make_unique<juce::FileChooser> ("Save song", dir.getChildFile ("song.xwsong"), "*.xwsong", false);
    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File())
                return;
            if (! file.hasFileExtension ("xwsong"))
                file = file.withFileExtension ("xwsong");

            const bool ok = file.replaceWithText (casioxw::songToJson (song));
            statusLabel.setText (ok ? ("Saved " + file.getFileName()) : "Save failed", juce::dontSendNotification);
        });
}

void ArrangerPanel::loadSongFromFile()
{
    auto dir = sequenceDirectory;
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    fileChooser = std::make_unique<juce::FileChooser> ("Load song", dir, "*.xwsong", false);
    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (! file.existsAsFile())
                return;
            const auto loaded = casioxw::songFromJson (file.loadFileAsString());
            if (! loaded.has_value())
            {
                statusLabel.setText ("Load failed: " + file.getFileName() + " is not a valid song", juce::dontSendNotification);
                return;
            }
            applyLoadedSong (*loaded);
            statusLabel.setText ("Loaded " + file.getFileName(), juce::dontSendNotification);
        });
}

void ArrangerPanel::applyLoadedSong (const casioxw::Song& loaded)
{
    if (playing)
        stop();
    song = loaded;
    if (song.rows.empty())
        song.rows.push_back ({});
    tempoSlider.setValue ((double) song.tempoBpm, juce::dontSendNotification);
    rebuildRowWidgets();
}

//==============================================================================
void ArrangerPanel::applyPreviewDemoState()
{
    song.rows.clear();

    casioxw::SongRow intro;
    intro.label = "Intro";
    intro.drumsFile = "demo.drums.xwdrm";
    intro.repeatCount = 4;
    intro.laneMuted[(size_t) casioxw::kSongDrumLaneStart + 1] = true;   // kick only: mute Drum 2
    intro.laneMuted[(size_t) casioxw::kSongDrumLaneStart + 2] = true;
    intro.laneMuted[(size_t) casioxw::kSongDrumLaneStart + 3] = true;
    intro.laneMuted[(size_t) casioxw::kSongDrumLaneStart + 4] = true;
    song.rows.push_back (intro);

    casioxw::SongRow verse;
    verse.label = "Verse";
    verse.soloFile = "demo.solo.xwseq";
    verse.drumsFile = "demo.drums.xwdrm";
    verse.pcmFile = "demo.pcm.xwpcm";
    verse.repeatCount = 8;
    song.rows.push_back (verse);

    casioxw::SongRow chorus;
    chorus.label = "Chorus";
    chorus.setFile = "demo.xwset";
    chorus.repeatCount = 4;
    song.rows.push_back (chorus);

    tempoSlider.setValue ((double) song.tempoBpm, juce::dontSendNotification);
    rebuildRowWidgets();

    // Show a representative "currently playing" wash on row 2 without actually starting the
    // real-time transport (no MIDI device in a headless snapshot).
    currentPosition = { 1, 0 };
    playing = true;
    repaint();
}
