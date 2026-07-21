#include "ArrangerPanel.h"

#include "casioxw/Scheduler.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    // ---- shared row/header column geometry --------------------------------------------------
    // Both RowWidgets::resized() and ArrangerPanel's column-header row lay out against these same
    // x-offsets/widths so the header labels land directly above their column, matching how
    // SequencerPanel aligns its step-grid header to the step columns below it.
    constexpr int kRowHeight       = 44;
    constexpr int kRowGap          = 4;
    constexpr int kHeaderHeight    = 20;
    constexpr int kGlobalMuteRowHeight = 30;
    constexpr int kIndexWidth      = 28;
    constexpr int kLabelWidth      = 120;
    // Widened from 108 -- owner reported saved sequence filenames getting cut off in these combos.
    // 1465px of row width is available (viewport width) against ~1315px actually needed at the old
    // width, so there was slack to spend here without pushing the mute/remove columns off-screen.
    constexpr int kFileComboWidth  = 136;
    constexpr int kRepeatWidth     = 100;
    constexpr int kLoopBackWidth   = 84;
    constexpr int kLoopCountWidth  = 84;
    constexpr int kLoopInfWidth    = 26;
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

    // Lays out kSongLaneCount mute-chip buttons starting at `startX`, with the same per-lane width/
    // gap/group-boundary spacing used by BOTH the per-row mute chips (RowWidgets::resized()) and
    // the arrangement-wide global mute row (ArrangerPanel::resized()) -- shared here so the two rows
    // of chips always land in the same columns, one factored-out geometry instead of two copies that
    // could drift apart.
    void layoutLaneChips (juce::TextButton* chips, int startX, int y, int height)
    {
        int x = startX;
        for (int i = 0; i < casioxw::kSongLaneCount; ++i)
        {
            chips[i].setBounds (x, y, kMuteChipWidth, height);
            x += kMuteChipWidth + 3;
            if (i == 0 || i == 5)   // group boundary after the synth lane, and after the 5 drum lanes
                x += kMuteGroupGap;
        }
    }

    constexpr double kSchedulerTickMs    = 12.0;
    // Steady-state look-ahead horizon. SequencerPanel keeps this small (60ms) ONLY to keep live
    // step-edit/p-lock EDITS responsive while playing -- arranger playback has no live editing, so
    // nothing forces this small here. A small horizon means the feeder MUST be re-entered (message
    // thread, every kSchedulerTickMs) well ahead of every step boundary; any stall on that thread
    // bigger than (horizon - time-already-consumed) -- a paint, a popup, an OS scheduling hiccup --
    // leaves the feeder behind schedule, which either clumps/rushes catch-up notes or, past
    // MidiOutput's ~200ms grace, drops them outright (see bug-152). Owner reported arranger timing
    // getting worse with more rows/tempo -- exactly what a starved feeder looks like. Widening this
    // buffers hundreds of ms of already-built MIDI ahead at all times, making the feeder robust
    // against that whole class of stall regardless of how late any individual tick runs. Trade-off
    // (accepted): a mid-song tempo drag takes up to this long to reach not-yet-scheduled steps, and
    // the playhead-leads-audio gap (already a known, flagged cosmetic issue) grows by the same
    // amount. Provisional/owner-tunable -- raise further if stalls are still reported.
    constexpr double kLookaheadMs        = 500.0;
    constexpr double kStartLeadMs        = 50.0;
    constexpr double kStartPrimeFloorMs  = 1500.0;
    constexpr double kScheduleSampleRate = 1000.0;

    // A paramChange burst -- a row-transition establish OR an ordinary per-step p-lock/base change
    // -- landing at the EXACT same instant as a note-on is the literal "SysEx burst simultaneous
    // with a note-on drops the note" failure mode SequencerPanel::play()'s own comment documents,
    // and which bit this feature twice already (song start, row transitions -- both previously
    // patched by giving the WHOLE burst a lead ahead of the note it precedes). That earlier fix
    // only separated burst-from-note; several DIFFERENT params changing on the SAME step (dense
    // p-locking, the "complicated sequences" case) are still all stamped at one instant relative to
    // EACH OTHER, which can overrun the synth's SysEx receiver even once the burst as a whole is
    // ahead of the note. scheduleParamBurst() below generalizes both fixes to every paramChange
    // batch (per-step AND row-transition): messages are spread across kInterMessageGapMs-spaced
    // slots ending kParamEstablishLeadMs before the batch's deadline, so the synth gets one message
    // at a time instead of a pile. All provisional/owner-tunable -- no hardware here to verify the
    // exact minimums against.
    constexpr double kParamEstablishLeadMs = 20.0;
    constexpr double kInterMessageGapMs    = 6.0;
    // How far back before the deadline a burst may reach, further capped per-call to a fraction of
    // the current step interval (see scheduleParamBurst()) so the earliest message in a burst can
    // never reach back into the PREVIOUS step's own note/param territory. A burst that doesn't fit
    // in the resulting window is TRUNCATED -- only the leading entries that fit are scheduled, the
    // rest are silently dropped. Owner's explicit call: timing/note delivery outranks p-lock
    // fidelity when a dense pattern makes both impossible to guarantee at once.
    constexpr double kMaxParamLeadWindowMs = 120.0;

    // Spreads a burst of param SysEx messages across kInterMessageGapMs-spaced slots ending
    // kParamEstablishLeadMs before `deadlineMs` (see the constants above for the full reasoning),
    // instead of stamping the whole burst at one instant. Returns how many of `perParamMessages`'
    // entries were actually scheduled (from the front) so the caller knows which of its own
    // parallel bookkeeping -- e.g. lastAppliedParams -- to update: a dropped param must NOT be
    // marked as applied, or a later diff would never retry it.
    int scheduleParamBurst (juce::MidiBuffer& buffer,
                            const std::vector<std::vector<juce::MidiMessage>>& perParamMessages,
                            double deadlineMs, double stepMs)
    {
        if (perParamMessages.empty())
            return 0;

        const double window = juce::jmin (kMaxParamLeadWindowMs, stepMs * 0.8);
        const int capacity = juce::jmax (1, 1 + (int) (window / kInterMessageGapMs));
        const int count = juce::jmin ((int) perParamMessages.size(), capacity);

        for (int i = 0; i < count; ++i)
        {
            const double t = deadlineMs - kParamEstablishLeadMs - (double) (count - 1 - i) * kInterMessageGapMs;
            const int samplePos = (int) std::llround (t);
            for (const auto& m : perParamMessages[(size_t) i])
                buffer.addEvent (m, samplePos);
        }
        return count;
    }

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
    x += kFileComboWidth + kColGap;

    soloCombo.setBounds (x, midY - 11, kFileComboWidth, 22); x += kFileComboWidth + kColGap;
    drumsCombo.setBounds (x, midY - 11, kFileComboWidth, 22); x += kFileComboWidth + kColGap;
    pcmCombo.setBounds (x, midY - 11, kFileComboWidth, 22); x += kFileComboWidth + kColGap;

    repeatSlider.setBounds (x, midY - 11, kRepeatWidth, 22); x += kRepeatWidth + kColGap;

    loopBackSlider.setBounds (x, midY - 11, kLoopBackWidth, 22); x += kLoopBackWidth + kColGap;
    loopCountSlider.setBounds (x, midY - 11, kLoopCountWidth, 22); x += kLoopCountWidth + 3;
    loopInfiniteButton.setBounds (x, midY - 11, kLoopInfWidth, 22); x += kLoopInfWidth + kColGap;

    layoutLaneChips (muteChips.data(), x, midY - 13, 26);

    removeButton.setBounds (b.getWidth() - kRemoveWidth, midY - 11, kRemoveWidth, 22);
}

//==============================================================================
ArrangerPanel::ArrangerPanel (casioxw::SysExCodec& codecIn, casioxw::MidiIO& midiIOIn)
    : codec (codecIn), midiIO (midiIOIn)
{
    titleLabel.setFont (EditorFonts::header (14.0f));
    titleLabel.setColour (juce::Label::textColourId, EditorColours::textHeader);
    addAndMakeVisible (titleLabel);

    for (auto* header : { &colLabelHeader, &colContentHeader, &colRepeatHeader, &colLoopHeader, &colMuteHeader })
    {
        header->setFont (EditorFonts::header (10.0f));
        header->setColour (juce::Label::textColourId, EditorColours::textMuted);
        addAndMakeVisible (*header);
    }

    playStopButton.onClick = [this] { playing ? stop() : play(); };
    addAndMakeVisible (playStopButton);

    loopArrangementButton.setClickingTogglesState (true);
    loopArrangementButton.setTooltip ("Loop the whole arrangement: restart at row 1 when it ends");
    loopArrangementButton.onClick = [this] { song.loopEnabled = loopArrangementButton.getToggleState(); };
    addAndMakeVisible (loopArrangementButton);

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

    for (int i = 0; i < casioxw::kSongLaneCount; ++i)
    {
        auto& chip = globalMuteChips[(size_t) i];
        chip.setButtonText (kLanes[i].caption);
        chip.setTooltip (juce::String ("Mute ") + kLanes[i].tooltip
                         + " across the WHOLE arrangement, regardless of any row's own mute state");
        chip.setClickingTogglesState (true);
        // Same "toggled-on == muted" data semantic as the per-row chips, but a distinct ON colour
        // (red, not idleStep) -- this row overrides every other row's own mute state, so an active
        // global mute should read as visually different/more emphatic than an ordinary row mute.
        chip.setColour (juce::TextButton::buttonColourId, EditorColours::filledStep);
        chip.setColour (juce::TextButton::buttonOnColourId, EditorColours::red);
        chip.setColour (juce::TextButton::textColourOffId, EditorColours::base03);
        chip.setColour (juce::TextButton::textColourOnId, EditorColours::base03);
        const int laneIndex = i;
        chip.onClick = [this, laneIndex]
        {
            song.globalLaneMuted[(size_t) laneIndex] = globalMuteChips[(size_t) laneIndex].getToggleState();
        };
        addAndMakeVisible (chip);
    }

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

    // A distinct panel-toned band behind the whole global-mute strip -- otherwise it read as just
    // another line of small text stacked directly under colLoopHeader/colMuteHeader, with nothing
    // to visually mark it as its own control (part of the "chaotic" feedback alongside the
    // alignment bug above). Painted here (behind every child component) so it never overlaps the
    // label/chip widgets, only frames them. Owner explicitly didn't want a divider line under it --
    // the band alone is enough separation.
    if (! globalMuteRowBounds.isEmpty())
    {
        g.setColour (EditorColours::panelBg);
        g.fillRect (globalMuteRowBounds);
    }

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

    // Drop-zone highlight while a row is being dragged (see beginRowDrag()/updateRowDrag()) --
    // computed from dragTargetIndex's GRID slot, not any rowWidgets[]->getBounds(), since the row
    // actually being dragged no longer sits at its grid position for the gesture's duration.
    if (dragRowIndex >= 0 && dragTargetIndex >= 0 && dragTargetIndex < (int) rowWidgets.size())
    {
        juce::Rectangle<int> slotBounds (0, dragTargetIndex * (kRowHeight + kRowGap),
                                         rowContainer.getWidth(), kRowHeight);
        auto onScreen = slotBounds + rowContainer.getPosition() + viewport.getPosition()
                      - juce::Point<int> (viewport.getViewPositionX(), viewport.getViewPositionY());
        g.setColour (EditorColours::selected.withAlpha (0.25f));
        g.fillRect (onScreen.withX (viewport.getX()).withWidth (viewport.getWidth()));
    }
}

void ArrangerPanel::resized()
{
    auto b = getLocalBounds().reduced (8);

    auto headerRow = b.removeFromTop (24);
    titleLabel.setBounds (headerRow.removeFromLeft (100));
    playStopButton.setBounds (headerRow.removeFromLeft (70));
    headerRow.removeFromLeft (6);
    loopArrangementButton.setBounds (headerRow.removeFromLeft (60));
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
    // Span from loopBackSlider's own left edge to loopInfiniteButton's own right edge -- i.e. the
    // exact same three widget widths AND the same two internal gaps RowWidgets::resized() actually
    // lays out between them (kColGap after loopBackSlider, then only 3px after loopCountSlider).
    // The previous formula (kLoopBackWidth+kLoopCountWidth+kLoopInfWidth+3, missing that first
    // kColGap) undercounted by 8px, which cascaded into colMuteHeader AND the global mute row
    // landing 8px short of where the per-row mute chips actually start -- caught by the owner
    // ("global mutes are misaligned with the line mutes"), not by gui-preview, since a solid-colour
    // chip row makes an 8px offset far more visually obvious than it ever was under plain header text.
    constexpr int kLoopClusterWidth = kLoopBackWidth + kColGap + kLoopCountWidth + 3 + kLoopInfWidth;
    colLoopHeader.setBounds (x, colHeaderRow.getY(), kLoopClusterWidth, kHeaderHeight); x += kLoopClusterWidth + kColGap;
    colMuteHeader.setBounds (x, colHeaderRow.getY(), colHeaderRow.getRight() - x, kHeaderHeight);
    const int muteColX = x;   // same column start the global mute row's chips align to below

    b.removeFromTop (6);

    // Arrangement-wide mute row, pinned here (above the scrolling viewport) rather than living
    // inside a row -- it applies to every row, not one. Aligned to the exact same column as the
    // per-row mute chips (muteColX, via the shared layoutLaneChips() helper) so it visually reads
    // as a "master" row sitting directly above them -- that alignment plus the painted background
    // band (globalMuteRowBounds, see paint()) is what identifies it; no caption label (owner
    // flagged a prior label attempt as misleading regardless of how it was sized/positioned).
    globalMuteRowBounds = b.removeFromTop (kGlobalMuteRowHeight);
    auto globalMuteRow = globalMuteRowBounds.reduced (0, 2);
    layoutLaneChips (globalMuteChips.data(), muteColX, globalMuteRow.getY(), globalMuteRow.getHeight());

    b.removeFromTop (6);
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

void ArrangerPanel::beginRowDrag (RowWidgets& w, const juce::MouseEvent& e)
{
    dragRowIndex = indexOfWidgets (&w);
    if (dragRowIndex < 0)
        return;
    dragTargetIndex = dragRowIndex;
    // Offset from the ROW's own top, not the handle's -- e originates on indexLabel, which sits at
    // a fixed offset within w, so re-target it to w's coordinate space once here rather than
    // re-deriving it on every drag update.
    dragGrabOffsetY = e.getEventRelativeTo (&w).getPosition().y;
    w.toFront (false);   // paint above sibling rows while being dragged
}

void ArrangerPanel::updateRowDrag (RowWidgets& w, const juce::MouseEvent& e)
{
    if (dragRowIndex < 0)
        return;

    // w is a direct child of rowContainer, so this is the same coordinate space layoutRowContainer()
    // positions every row in -- re-deriving the drop target from raw pixels (not a delta) so a drag
    // that overshoots the viewport and comes back still lands where the cursor actually is.
    const int newTop = e.getEventRelativeTo (&rowContainer).getPosition().y - dragGrabOffsetY;
    w.setTopLeftPosition (w.getX(), newTop);

    // Target flips at the midpoint between two row slots (the row's CENTRE, not its top), so the
    // drop zone changes at a predictable point instead of at each slot's leading edge.
    const int centreY = newTop + kRowHeight / 2;
    const int target = juce::jlimit (0, (int) rowWidgets.size() - 1, centreY / (kRowHeight + kRowGap));
    if (target != dragTargetIndex)
    {
        dragTargetIndex = target;
        repaint();   // drop-zone highlight, drawn in paint()
    }
}

void ArrangerPanel::endRowDrag (RowWidgets&, const juce::MouseEvent&)
{
    if (dragRowIndex < 0)
        return;

    const bool moved = dragTargetIndex != dragRowIndex;
    if (moved)
    {
        auto row = song.rows[(size_t) dragRowIndex];
        song.rows.erase (song.rows.begin() + dragRowIndex);
        song.rows.insert (song.rows.begin() + dragTargetIndex, row);
    }

    dragRowIndex = -1;
    dragTargetIndex = -1;

    if (moved)
        rebuildRowWidgets();   // new order -- every row's widgets need re-syncing anyway
    else
        layoutRowContainer();  // no reorder -- just snap the dragged row's widget back to its slot
    repaint();
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
    w.indexLabel.setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    w.indexLabel.setTooltip ("Drag to reorder this row");
    w.indexLabel.onHandleMouseDown = [this, &w] (const juce::MouseEvent& e) { beginRowDrag (w, e); };
    w.indexLabel.onHandleMouseDrag = [this, &w] (const juce::MouseEvent& e) { updateRowDrag (w, e); };
    w.indexLabel.onHandleMouseUp   = [this, &w] (const juce::MouseEvent& e) { endRowDrag (w, e); };
    w.addAndMakeVisible (w.indexLabel);

    w.labelEditor.setFont (EditorFonts::mono (12.0f));
    w.labelEditor.setTextToShowWhenEmpty (juce::CharPointer_UTF8 ("\xe2\x80\x94"), EditorColours::textMuted);
    w.labelEditor.onTextChange = [this, &w] { onRowFieldChanged (w); };
    w.addAndMakeVisible (w.labelEditor);

    w.setCombo.onChange = [this, &w]
    {
        // solo/drums/pcm stay visible/usable regardless of whether a set is also loaded -- a set is
        // a BASELINE, not exclusive; picking an individual file overrides just that one part (see
        // loadRowRuntime()). setCombo.getSelectedId() no longer changes any other widget's visibility.
        w.setCombo.setTooltip (w.setCombo.getText());   // full name on hover even when the column
                                                        // itself can't fit it (see kFileComboWidth)
        onRowFieldChanged (w);
    };
    w.addAndMakeVisible (w.setCombo);

    for (auto* combo : { &w.soloCombo, &w.drumsCombo, &w.pcmCombo })
    {
        combo->onChange = [this, &w, combo] { combo->setTooltip (combo->getText()); onRowFieldChanged (w); };
        w.addAndMakeVisible (*combo);
    }

    // Explicit setTextBoxStyle() on all three IncDecButtons sliders below -- JUCE's own default
    // textBoxWidth is 80px, which getSliderLayout() clamps to (columnWidth - 30), leaving as little
    // as ~13px per +/- button on these narrow (84-100px) columns. At that width, drawFittedText()
    // can't fit even a single "+" glyph at any legible scale and silently substitutes the WHOLE
    // button label with "..." (the "-" glyph is narrow enough to survive, which is why only the "+"
    // side looked broken). Narrowing the text box to just what the value actually needs leaves the
    // buttons a legible, comfortable width instead.
    w.repeatSlider.setRange (1.0, 99.0, 1.0);
    w.repeatSlider.setValue (1.0, juce::dontSendNotification);
    w.repeatSlider.setTextValueSuffix ("x");
    w.repeatSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 22);
    w.repeatSlider.onValueChange = [this, &w] { onRowFieldChanged (w); };
    w.addAndMakeVisible (w.repeatSlider);

    w.loopBackSlider.setRange (0.0, 99.0, 1.0);
    w.loopBackSlider.setValue (0.0, juce::dontSendNotification);
    w.loopBackSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 22);
    w.loopBackSlider.setTooltip ("Loop line: jump back this many rows once this row's own repeats "
                                 "finish (0 = no loop line on this row)");
    w.loopBackSlider.onValueChange = [this, &w] { onRowFieldChanged (w); updateLoopWidgetVisibility (w); };
    w.addAndMakeVisible (w.loopBackSlider);

    w.loopCountSlider.setRange (1.0, 99.0, 1.0);
    w.loopCountSlider.setValue (1.0, juce::dontSendNotification);
    w.loopCountSlider.setTextValueSuffix ("x");
    w.loopCountSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 36, 22);
    w.loopCountSlider.setTooltip ("How many times to take the loop line before falling through");
    w.loopCountSlider.onValueChange = [this, &w] { onRowFieldChanged (w); };
    w.addAndMakeVisible (w.loopCountSlider);

    w.loopInfiniteButton.setClickingTogglesState (true);
    w.loopInfiniteButton.setTooltip ("Loop forever instead of a set number of times");
    w.loopInfiniteButton.onClick = [this, &w] { onRowFieldChanged (w); updateLoopWidgetVisibility (w); };
    w.addAndMakeVisible (w.loopInfiniteButton);

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

void ArrangerPanel::updateLoopWidgetVisibility (RowWidgets& w)
{
    const bool hasLoopLine = w.loopBackSlider.getValue() > 0.0;
    w.loopCountSlider.setVisible (hasLoopLine);
    w.loopInfiniteButton.setVisible (hasLoopLine);
    w.loopCountSlider.setEnabled (! w.loopInfiniteButton.getToggleState());
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
    w.loopBackSlider.setValue ((double) row.loopBackRows, juce::dontSendNotification);
    const bool infinite = row.loopCount == casioxw::kInfiniteLoopCount;
    w.loopInfiniteButton.setToggleState (infinite, juce::dontSendNotification);
    w.loopCountSlider.setValue (infinite ? 1.0 : (double) row.loopCount, juce::dontSendNotification);
    updateLoopWidgetVisibility (w);
    for (int i = 0; i < casioxw::kSongLaneCount; ++i)
        w.muteChips[(size_t) i].setToggleState (row.laneMuted[(size_t) i], juce::dontSendNotification);

}

void ArrangerPanel::onRowFieldChanged (RowWidgets& w)
{
    const int idx = indexOfWidgets (&w);
    if (idx < 0)
        return;
    auto& row = song.rows[(size_t) idx];

    row.label = w.labelEditor.getText();
    row.repeatCount = juce::jlimit (1, 99, (int) w.repeatSlider.getValue());
    row.loopBackRows = juce::jlimit (0, 99, (int) w.loopBackSlider.getValue());
    row.loopCount = w.loopInfiniteButton.getToggleState()
                        ? casioxw::kInfiniteLoopCount
                        : juce::jlimit (1, 99, (int) w.loopCountSlider.getValue());
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

    // Widening the column (see kFileComboWidth) helps most filenames fit, but a long one can still
    // get truncated with an ellipsis -- a tooltip is the general-case fix, since no fixed column
    // width can guarantee every possible saved-file name fits.
    combo.setTooltip (keep.isEmpty() ? juce::String ("(none)") : keep);
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

    // A loaded set is the BASELINE, not exclusive -- soloFile/drumsFile/pcmFile below (if any are
    // also set) OVERRIDE just their one part, so a row can load a whole .xwset and still swap out
    // e.g. only its drums for a different saved pattern, rather than the set being all-or-nothing.
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

void ArrangerPanel::queueDiffEstablish (juce::MidiBuffer& buffer, const casioxw::Sequence& seq,
                                       int stepIndex, double timeMs, double stepMs)
{
    // Collect every param that actually needs to change first, THEN hand the whole batch to
    // scheduleParamBurst() to pace/cap -- deliberately allowed to go negative relative to timeMs
    // (transportStartMs already carries its own kStartLeadMs buffer ahead of wall-clock "now", see
    // play()), so pulling a row's param establish ahead of ITS note-on is still comfortably in the
    // future in absolute terms even for the very first step of the whole song (timeMs == 0) -- and
    // if an unusually large burst pushes past that buffer, JUCE's own scheduling grace (bug-152)
    // fires it ASAP rather than dropping it, so pacing degrades gracefully instead of failing.
    std::vector<juce::String> keys;
    std::vector<int> values;
    std::vector<std::vector<juce::MidiMessage>> messages;

    for (const auto& lp : seq.lockable)
    {
        const auto value = casioxw::effectiveParamValue (seq, stepIndex, lp.paramId, lp.instance);
        if (! value.has_value())
            continue;

        const auto key = lp.paramId + "#" + juce::String (lp.instance);
        const auto it = lastAppliedParams.find (key);
        if (it != lastAppliedParams.end() && it->second == *value)
            continue;   // device already believed to hold this value -- nothing to send

        keys.push_back (key);
        values.push_back (*value);
        messages.push_back (paramMessages (lp.paramId, lp.instance, *value));
    }

    const int sent = scheduleParamBurst (buffer, messages, timeMs, stepMs);
    for (int i = 0; i < sent; ++i)
        lastAppliedParams[keys[(size_t) i]] = values[(size_t) i];
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
    currentPosition = { 0, 0, {} };
    runtimeLoadedForRow = -1;
    lastHighlightedRow = -1;
    lastAppliedParams.clear();   // device's actual state is unknown at song start, same reasoning
                                // as prevStepIndex's kPrevStepFresh below

    // Parse every row's file(s) ONCE, here, before the real-time clock starts -- disk I/O + JSON
    // parsing is fine as a one-time pause on Play, but doing it from inside the feeder at every row
    // boundary (the old approach) stalled the message thread long enough to cause audible lateness.
    preloadedRuntimes.clear();
    preloadedRuntimes.reserve (song.rows.size());
    for (const auto& row : song.rows)
        preloadedRuntimes.push_back (loadRowRuntime (row));

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

    // Repaint only when the highlighted row actually changes, not on every 12ms tick -- a full
    // repaint recomposites every row widget (comboboxes/sliders/buttons across potentially many
    // rows), which is real work on this SAME message thread the feeder runs on. Repainting
    // unconditionally here was stealing enough time from the feeder to starve its look-ahead
    // window, which read as audible lateness ("lurching") that got worse with more rows -- more
    // widgets to repaint each tick. Same fix SequencerPanel's own updatePlayheadStep() already uses
    // (repaint only on an actual playhead change).
    if (currentPosition.row != lastHighlightedRow)
    {
        lastHighlightedRow = currentPosition.row;
        repaint();
    }

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
        juce::MidiBuffer buffer;

        const bool rowJustChanged = currentPosition.row != runtimeLoadedForRow;
        if (rowJustChanged)
        {
            // Row boundary: swap in the new row's ALREADY-parsed runtime (preloadedRuntimes, built
            // once in play() -- never disk I/O/JSON parsing from inside this real-time feeder loop,
            // which used to stall the message thread long enough to cause audible lateness).
            currentRuntime = preloadedRuntimes[(size_t) currentPosition.row];
            runtimeLoadedForRow = currentPosition.row;
        }

        const auto& row = song.rows[(size_t) currentPosition.row];

        // The song's tempoBpm is the single clock; only the RATE (stepsPerBeat) is taken from
        // whichever loaded lane defines it, same convention SequencerPanel uses to keep drum/pcm
        // lanes locked to the main sequence's rate. Computed BEFORE the establish call below (moved
        // ahead of it deliberately) so queueDiffEstablish() gets the SONG's current stepMs, not a
        // stale value from whatever tempo the just-loaded row's file happened to be saved at.
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

        if (rowJustChanged)
        {
            // Establish the new row's lockable params via a DIFF against lastAppliedParams, not a
            // full kPrevStepFresh dump -- unconditionally re-sending every lockable param (dozens
            // for an engine like Hex Layer) at every row transition was exactly the "SysEx burst
            // fired alongside the first notes" lurch SequencerPanel's own play() already documents
            // avoiding, just recurring here instead of happening once. Same-engine/same-value rows
            // send ~nothing; an actual change sends only what's needed, PACED (queueDiffEstablish /
            // scheduleParamBurst) rather than dumped at one instant -- no p-lock carryover left
            // permanently stuck (the removed reset-to-base's gap), no silent wrong-tone risk from
            // assuming the device sits at base (kPrevStepBaseline would be wrong here: nothing
            // resets the device to base between rows any more).
            if (currentRuntime.solo.has_value())
                queueDiffEstablish (buffer, *currentRuntime.solo, nextStepIndex, nextStepStartMs, stepMs);
            prevStepIndex = nextStepIndex;   // suppress scheduleStep()'s OWN paramChange emission
                                             // below (diffing a sequence against itself is always
                                             // empty) -- queueDiffEstablish() already covered it
        }

        // A lane only sounds when BOTH this row's own mute AND the arrangement-wide global mute
        // (song.globalLaneMuted -- "GLOBAL MUTE" row in the UI) say it's audible, so the global
        // strip can silence a lane across the whole song without having to mute it on every row.
        const bool synthAudible = ! row.laneMuted[(size_t) casioxw::kSongSynthLane]
                                 && ! song.globalLaneMuted[(size_t) casioxw::kSongSynthLane];
        if (currentRuntime.solo.has_value() && synthAudible)
        {
            // paramChange events are collected here, not sent immediately -- scheduleStep() stamps
            // every paramChange at the SAME instant as this step's own note-on (its contract only
            // promises params land "before" the note, not "with any separation"). Several DIFFERENT
            // locked params changing on one step (dense p-locking) would otherwise all land in one
            // instant, the same burst-vs-note-and-burst-vs-itself problem queueDiffEstablish already
            // has to solve at row transitions -- paced the same way here via scheduleParamBurst().
            std::vector<juce::String> paramKeys;
            std::vector<int> paramValues;
            std::vector<std::vector<juce::MidiMessage>> paramMsgs;

            for (const auto& e : casioxw::scheduleStep (*currentRuntime.solo, nextStepIndex, prevStepIndex, nextStepStartMs))
            {
                switch (e.type)
                {
                    case casioxw::ScheduledEvent::Type::noteOn:
                        buffer.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.velocity),
                                         (int) std::llround (e.timeMs));
                        break;
                    case casioxw::ScheduledEvent::Type::noteOff:
                        buffer.addEvent (juce::MidiMessage::noteOff (e.channel, e.note),
                                         (int) std::llround (e.timeMs));
                        break;
                    case casioxw::ScheduledEvent::Type::paramChange:
                        paramKeys.push_back (e.paramId + "#" + juce::String (e.instance));
                        paramValues.push_back (e.value);
                        paramMsgs.push_back (paramMessages (e.paramId, e.instance, e.value));
                        break;
                }
            }

            const int sent = scheduleParamBurst (buffer, paramMsgs, nextStepStartMs, stepMs);
            // Keep lastAppliedParams current for whatever was ACTUALLY sent (not anything truncated
            // away -- see scheduleParamBurst()), so a later row transition's diff (queueDiffEstablish())
            // compares against the device's real current value, not one it never received.
            for (int i = 0; i < sent; ++i)
                lastAppliedParams[paramKeys[(size_t) i]] = paramValues[(size_t) i];
        }

        if (currentRuntime.hasDrums)
        {
            for (int i = 0; i < (int) currentRuntime.drums.size(); ++i)
            {
                if (row.laneMuted[(size_t) (casioxw::kSongDrumLaneStart + i)]
                    || song.globalLaneMuted[(size_t) (casioxw::kSongDrumLaneStart + i)])
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
                if (row.laneMuted[(size_t) (casioxw::kSongPcmLaneStart + i)]
                    || song.globalLaneMuted[(size_t) (casioxw::kSongPcmLaneStart + i)]
                    || ! currentRuntime.pcm[(size_t) i].has_value())
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
    loopArrangementButton.setToggleState (song.loopEnabled, juce::dontSendNotification);
    for (int i = 0; i < casioxw::kSongLaneCount; ++i)
        globalMuteChips[(size_t) i].setToggleState (song.globalLaneMuted[(size_t) i], juce::dontSendNotification);
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
    chorus.loopBackRows = 2;   // loop line: jump back to the Verse
    chorus.loopCount = casioxw::kInfiniteLoopCount;
    song.rows.push_back (chorus);

    tempoSlider.setValue ((double) song.tempoBpm, juce::dontSendNotification);
    loopArrangementButton.setToggleState (song.loopEnabled, juce::dontSendNotification);
    rebuildRowWidgets();

    // Show a representative "currently playing" wash on row 2 without actually starting the
    // real-time transport (no MIDI device in a headless snapshot).
    currentPosition = { 1, 0, {} };
    playing = true;
    repaint();
}
