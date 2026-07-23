#pragma once

#include "ParamPageDisplay.h"

#include "casioxw/MidiIO.h"
#include "casioxw/Sequence.h"
#include "casioxw/SysExCodec.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class ArrangerPanel;   // owned by pointer below; full definition only needed in SequencerPanel.cpp

//==============================================================================
/** Which tone engine the sequencer's solo track currently p-locks. Only one of these three can
    ever be sounding on the XW-P1 at once (a Performance part's engine assignment is exclusive),
    so the sequencer mirrors that: one active engine at a time, selected in the synth card, with
    its own lockable-parameter table (SequencerPanel.cpp's kEngineSets). Switching engines swaps
    which table `sequence.lockable` is built from; per-step locks stay keyed by paramId in the
    step data regardless, so old locks for a not-currently-selected engine are simply inert
    (not deleted) until that engine is reselected -- no data loss on a round trip through another
    engine. */
enum class TrackEngine { soloSynth, hexLayer, drawbarOrgan };

//==============================================================================
/** One step key in the trig grid, painted like a hardware trig button rather than a stock
    text button: rounded cap, underlined monospace numeral, an amber LED dot when the step holds
    any p-lock, a cyan LED dot (opposite corner, distinct colour so the two are never confused)
    when a poly-capable track's step sounds more than one note there but its sub-track rows are
    collapsed, and a structurally thicker outline on the quarter-note steps (1/5/9/13) so bar
    orientation never depends on fill colour alone.

    State model: juce::Button's own toggle state IS the trig on/off (several owners persist it
    from getToggleState()); hasLock/selected/hasChord are display-only flags pushed via
    setLockState()/setChordState(). */
class StepKeyButton : public juce::Button
{
public:
    StepKeyButton() : juce::Button ({}) {}

    void setStepIndex (int index) { stepIndex = index; }
    void setLockState (bool hasLockIn, bool selectedIn);

    /** Whether this step currently sounds more than one note on a poly-capable track (any
        extra-voice step here is enabled) -- irrelevant/always false for non-poly-capable tracks
        (drums, mono PCM tracks, Solo Synth). */
    void setChordState (bool hasChordIn);

    /** True when this widget's windowed step index (currentPage*kStepsPerPage + column) is at or
        beyond the pattern's configured stepCount -- i.e. it's on the last page of a pattern whose
        length isn't a multiple of 16. A greyed step paints dim/inert and ignores clicks (the
        caller is expected to also skip wiring/leave its onClick a no-op while greyed, but
        paintButton() alone is enough to make the state visually unambiguous). */
    void setGreyed (bool greyedIn);

    void paintButton (juce::Graphics&, bool isMouseOver, bool isMouseDown) override;

private:
    int stepIndex = 0;
    bool hasLock = false;
    bool selected = false;
    bool hasChord = false;
    bool greyed = false;
};

//==============================================================================
/** Elektron-style page indicator: a row of up to 4 small LED dots, one per 16-step page the
    current pattern spans (pageCount() = ceil(stepCount/16)) -- lit for the current page, dim for
    the rest. Purely a display; SequencerPanel's page-nav buttons (< / >) drive currentPage, this
    just reflects it. Click support is deliberately omitted (the LEDs are read-only status on real
    Elektron hardware too) -- paging is via the buttons only. */
class PageIndicator : public juce::Component
{
public:
    void setPageState (int pageCountIn, int currentPageIn)
    {
        pageCountShown = pageCountIn;
        currentPageShown = currentPageIn;
        repaint();
    }

    void paint (juce::Graphics&) override;

private:
    int pageCountShown = 1;
    int currentPageShown = 0;
};

//==============================================================================
/** 16-step note sequencer with Elektron-style parameter locks.

    `sequence` is the single source of truth (note/vel/enable/tempo/channel + per-step p-locks +
    per-param base values); every widget writes into it on edit and is refreshed from it. Notes go
    out as channel-voice messages; p-locks go out as NRPN where mapped (SysEx fallback otherwise) —
    a p-lock IS a tone edit scheduled to a step.

    Interaction model (one rule: the edit target is `(selectedStep, editMode)`, surfaced in the
    parameter display's header):
      - `selectedStep < 0` (Base): the param cells edit each parameter's *base* value (the
        track's main sound, applied on every unlocked step). Always editable.
      - a step selected + P-LOCK mode: moving a param cell writes a p-lock to that step, and the
        cell renders inverted (amber) while locked — the Digitakt convention.

    The p-lockable set is far larger than fits on screen, so the cells live in ParamPageDisplay —
    a pageable LCD-style sub-window (2x4 cells per page, page keys underneath). Adding a lockable
    param = one row in kLockables (SequencerPanel.cpp); the pages, playback, and lock UI all
    derive from it.

    Playback uses the roadmap's look-ahead + timestamped-output design (SEQUENCER_HANDOFF.md S3/S4):
    a loose message-thread feeder timer fills a short horizon by calling the pure
    casioxw::scheduleStep() per step and handing the timestamped events to the MIDI output thread
    (casioxw::MidiIO::scheduleBlock), so note timing stays steady even when the feeder is jittery
    under background load. Param changes (p-locks + base) go through a single paramMessages() seam —
    SysEx via the codec today, NRPN/CC later — used by both the scheduled path and the immediate
    audition path. Stop flushes pending output + resets every parameter to base so a p-lock can't
    leave the filter stuck. */
class SequencerPanel : public juce::Component, private juce::Timer
{
public:
    SequencerPanel (casioxw::SysExCodec& codec, casioxw::MidiIO& midiIO);
    ~SequencerPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    /** tools/gui-preview only: seed a representative editing state (trigs, p-locks, a selected
        step in P-LOCK mode, a playhead position) so offscreen snapshots can verify the
        state-dependent rendering a fresh panel never shows. Never called by the app itself. */
    void applyPreviewDemoState();

    /** tools/gui-preview only: switch the synth lane to Hex Layer via the same guarded
        switchEngine() path the engine combo uses, so a snapshot can verify the combo/label swap
        and the AMP/FILT/PITCH/LFO pages actually populate (a fresh panel only ever shows Solo
        Synth). Never called by the app itself. */
    void applyHexLayerPreviewState();

    /** tools/gui-preview only: select a step on a PCM track (Bass) with P-LOCK mode on, so an
        offscreen snapshot can verify the screen actually swaps to the NOTE/GATE/VEL step editor
        (and back) rather than always showing the Solo Synth's pages. Never called by the app
        itself. */
    void applyPcmStepEditPreviewState();

    /** tools/gui-preview only: turns on poly mode + expands sub-tracks on the Chords row and the
        solo lane (switched to Hex Layer first, since Solo Synth can't go poly), seeds a chord on
        one step of each, and selects an extra voice's step so the screen shows its NOTE/GATE/VEL
        editor -- so a snapshot can verify the Poly toggle/arrow, sub-track rows, and the amber/
        cyan step-key dots all render (a fresh panel never shows any of them). Never called by the
        app itself. */
    void applyPolyPreviewState();

    /** tools/gui-preview only: focuses Drum 2 (no step selected) via the same setFocusedTrack()
        path a real label click uses, so a snapshot can verify the focused-row highlight wash and
        the LCD screen swapping to that lane's NOTE/VEL base page (a fresh panel starts focused on
        the solo lane, showing neither). Never called by the app itself. */
    void applyFocusPreviewState();

    /** tools/gui-preview only: headless correctness check for the PCM tracks save/load path (the
        one genuinely new bit of logic in that feature -- everything else reuses already-tested
        casioxw_core functions). Seeds two PCM tracks with distinct data, serializes, clobbers the
        live tracks, reloads, and compares. Returns true iff every seeded field round-trips.
        Never called by the app itself; no display/JUCE peer required. */
    bool verifyPcmRoundTripForPreview();
    bool verifySoloPolyRoundTripForPreview();

    /** tools/gui-preview only: switch into Arranger mode and seed the sub-panel's representative
        demo rows (see ArrangerPanel::applyPreviewDemoState), so an offscreen snapshot can verify
        the row table's real layout at the app's real window size. Never called by the app itself. */
    void applyArrangerPreviewState();

    /** tools/gui-preview only: sets a step count NOT divisible by 16 (40 -- 2 full pages + a
        partial one) and pages to the last one, with a couple of trigs on both the solo lane and a
        drum lane spanning the greyed boundary, so a snapshot can verify the page LED row (3 lit of
        3), the </> nav, and the last-page greyed/inert steps (40..47 of that page's 32..47) all
        render correctly. Never called by the app itself. */
    void applyPagingPreviewState();

private:
    enum class SaveKind
    {
        solo,
        drums,
        pcm,
        sequenceSet
    };

    /** Which lane the shift arrows + the LCD's "Base" display currently act on -- set by clicking
        a track's label or by selecting a step on any of its lanes (see setFocusedTrack()). One
        value across the WHOLE panel, not per-voice: a poly-capable track's sub-voices follow their
        parent's focus, they don't introduce a separate focus target. Defaults to soloSynth so the
        pre-existing shift-arrow behaviour (always the solo lane) is unchanged for a fresh panel. */
    enum class FocusedTrackKind { soloSynth, drum, pcm };

    // Note/gate/velocity for the solo lane's own primary voice are NOT edited here -- selecting a
    // step in P-LOCK mode swaps the screen to a NOTE page (then this engine's own p-lock pages),
    // the same select-then-edit mechanism PCM/poly tracks already use (see PcmTrackControl's doc
    // comment and refreshParamDisplayPages()). Previously this held always-visible per-step note/
    // gate/velocity knobs; owner asked for those to go in favour of one consistent mechanism.
    struct StepControl
    {
        StepKeyButton select;                 // shows step number; click selects/toggles it
    };

    struct DrumTrackControl
    {
        juce::Label trackLabel;
        juce::TextButton mute { "Mute" };
        juce::ComboBox channel;
        juce::Slider note { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::Slider velocity { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::Label velocityMarker;

        // 16 physical widgets are a WINDOWED VIEW onto up to kMaxSteps real steps (see
        // SequencerPanel::currentPage) -- which absolute step widget column `col` represents is
        // `currentPage*kStepsPerPage + col`, resolved at click time, not fixed at construction.
        std::array<StepKeyButton, 16> steps;

        // The real per-step data (kMaxSteps long): a drum lane has no casioxw::Sequence backing it
        // (a fixed note/velocity per lane, not a per-step melodic Sequence), so unlike solo/PCM/poly
        // tracks its trigger on/off state can't live in casioxw::Step::enabled -- it used to live
        // directly on the StepKeyButton's own JUCE toggle state instead, which only worked because
        // each of the (exactly 16) widgets always represented the same one step. Paging breaks that
        // assumption (a widget's represented step changes with the page), so triggerEnabled is the
        // real source of truth now; the widget's toggle state is just a repaint of whichever slice
        // is currently windowed into view.
        std::array<bool, casioxw::kMaxSteps> triggerEnabled {};
        int baseVelocity = 100;
        std::array<std::optional<int>, casioxw::kMaxSteps> velocityLocks;
        int selectedStep = -1; // per-track p-lock selection target, -1 means base

        // This row's full extent (label through step keys), captured by resized() before it's
        // sliced up -- paint() uses it to wash the row when it's the focused track.
        juce::Rectangle<int> rowBounds;
    };

    // Every lane keeps exactly 16 physical StepKeyButton widgets (unchanged pixel footprint --
    // "don't want to change the UI much"); they're a WINDOWED VIEW onto up to kMaxSteps real steps,
    // one page (16 consecutive steps) at a time. currentPage/pageCount() live below with the other
    // playback-adjacent state; kStepsPerPage is compile-time so both the widget arrays and the
    // paging arithmetic stay obviously in lockstep.
    static constexpr int kStepsPerPage = 16;

    // Poly mode's voice cap: a PRODUCT constant, not hardware-derived (the manual has no
    // per-track "chord width" figure -- its only bounded "simultaneous notes from one trigger"
    // concept is the Arpeggiator's own Polyphony(P) setting, a different subsystem, so borrowing
    // a number from it would be a guess dressed as a manual fact). 4 covers a standard triad/7th
    // without the sub-track UI getting unreasonably tall; owner can revisit if that's too tight.
    static constexpr int kMaxPolyVoices = 4;

    /** One additional simultaneous note-lane on a poly-capable track (PCM Chords, or the solo
        lane while its engine is Hex Layer/Drawbar Organ -- see SequencerPanel.h's TrackEngine doc
        and PcmTrackControl::polyMode). Reuses casioxw::Sequence per the project's own PCM-tracks
        precedent (`track` mirrors channel/tempo from its parent the same way a PcmTrackControl
        does from the main sequence) rather than a bespoke struct -- gets scheduleStep()/
        sequenceToJson()/FromJson() for free. `track.lockable` is intentionally left EMPTY: hardware
        has one filter/envelope per PART, not per simultaneous note, so p-locks stay on the
        parent's own lockable table (sequence's, for the solo lane) -- a voice is note/gate/
        velocity/enabled only, same as a PCM track's own step shape. */
    struct PolyVoice
    {
        casioxw::Sequence track;
        std::array<StepKeyButton, 16> steps;
    };

    /** One melodic PCM/Melody track (Bass, Solo 1, Solo 2, or Chords — the Step Sequencer note
        parts that aren't Drum 1-5 or the Solo Synth's own Zone Part 1, XWP1_1B_EN.pdf p.E-49). Its
        own lane, structured like DrumTrackControl (label/mute/channel/16 step keys), NOT a shared
        column with the Solo Synth. `track` reuses casioxw::Sequence as a self-contained per-track
        model (channel + 16 steps + an empty `lockable` for now) so every existing pure core
        function — scheduleStep, stepIntervalMs, sequenceToJson/FromJson — works on it unchanged;
        tempoBpm/stepsPerBeat are mirrored from the main `sequence` every playback tick. A step's
        note/gate/velocity are always-defined (no base-vs-lock inheritance the way FLTR/ENV params
        have) — selecting a step in P-LOCK mode swaps the screen to a 3-cell NOTE/GATE/VEL editor
        for it (see refreshParamDisplayPages()), not a per-row knob column.

        Only the Chords row (kPcmTracks index 3) is ever poly-capable in practice -- Bass/Solo 1/
        Solo 2 keep polyMode permanently false/unwired (per owner's explicit scope: PCM Chords +
        the solo lane while Hex Layer/Drawbar Organ, not the other PCM tracks, not Solo Synth) --
        but the fields live on every row uniformly rather than a separate struct, matching this
        codebase's general preference for one data shape over a conditional one. */
    struct PcmTrackControl
    {
        juce::Label trackLabel;
        juce::TextButton mute { "Mute" };
        juce::ComboBox channel;
        std::array<StepKeyButton, 16> steps;
        int selectedStep = -1;   // -1 == none; which step's note/gate/vel the screen currently edits
        int selectedVoice = 0;   // which voice selectedStep refers to: 0 == track, 1..N == extraVoices[v-1]
        casioxw::Sequence track;

        bool polyCapable = false;   // true only for the Chords row -- set once in the ctor
        bool polyMode = false;
        std::array<PolyVoice, kMaxPolyVoices - 1> extraVoices;
        bool subTracksExpanded = false;
        juce::TextButton polyToggle { "Poly" };
        juce::TextButton subTrackArrow;

        // This row's full extent (label through step keys), captured by resized() before it's
        // sliced up -- paint() uses it to wash the row when it's the focused track.
        juce::Rectangle<int> rowBounds;
    };

    void timerCallback() override;
    void play();
    void stop();

    /** Pull every lockable parameter's CURRENT value from the synth and adopt it as that
        parameter's base value — so the sequence's "base sound" is whatever the tone editor /
        hardware actually holds, not the offline defaults in kLockables. Uses the same
        request/drain flow as SoloSynthPanel's Sync (the shared receive queue is only ever
        drained by one panel at a time); only available while stopped, since the playback
        feeder owns this panel's timer while playing. */
    void syncBaseValuesFromSynth();

    void selectStep (int step);                // -1 == Base
    void setPLockMode (bool pLockMode);        // STEP / P-LOCK mode keys both land here

    /** Sets the panel-wide focused track (see FocusedTrackKind's doc comment) and refreshes the
        row highlight + LCD display to match. Called both from a track label's click (pure focus,
        no step selection change) and from every step button's click handler (so focus always
        follows whatever lane you're actively editing). */
    void setFocusedTrack (FocusedTrackKind kind, int index);

    /** Shift arrows' handler: rotates whichever lane setFocusedTrack() last pointed at (+ its poly
        sub-tracks, if any) by delta -- the arrows themselves are unchanged, only their target
        varies with focus. */
    void shiftFocusedTrack (int delta);

    /** Rotates one drum lane's pattern (trigger on/off + velocity locks, over the active
        sequence.stepCount window) by delta, wrapping -- the manual equivalent of
        casioxw::shiftSteps() for a lane that has no casioxw::Sequence backing it (a drum step's
        state lives in DrumTrackControl::triggerEnabled/velocityLocks, see that struct's doc
        comment). Same rotation direction/formula as casioxw::shiftSteps so drum shifting feels
        identical to every other lane's. */
    void shiftDrumTrack (DrumTrackControl& row, int delta);

    /** Number of 16-step pages the current pattern spans: ceil(sequence.stepCount / kStepsPerPage),
        1..4. The step count is global (one value shared by every Sequence-backed track + every
        drum lane), so this is a single panel-wide page count, not per-lane. */
    int pageCount() const;

    /** Sets the panel-wide global step count (1..kMaxSteps), mirrored into every Sequence-backed
        track (sequence, every PCM track, every poly voice -- drum lanes have no per-lane stepCount
        of their own, they just read sequence.stepCount) so every lane's active pattern length stays
        in lockstep, per the "global, not per-track" design. Clamps currentPage back into range if
        the new page count is smaller, then refreshes the step grid/page indicator. */
    void setStepCount (int newCount);

    /** Page nav (the small LED-row's < / > buttons): moves currentPage by delta, clamped to
        0..pageCount()-1, then refreshes every lane's windowed step buttons + the LED indicator. */
    void setCurrentPage (int newPage);

    void onParamEdited (int lockableIndex, int value);
    /** A p-lock cell's double-click reset: clears the selected step's lock on this param
        (reverting it to the base value) when the cell is currently locked; a no-op otherwise
        (base mode, or an already-unlocked step, has nothing step-specific to clear). */
    void onParamReset (int lockableIndex);
    void refreshParamControls();               // value + locked flags into the param display
    void refreshStepButtons();                 // selected highlight + has-locks LED

    /** Rebuild lockablePages from the active engine's lockable set (kEngineSets-driven) --
        includes the range-seeding/continuousLockables side effects, so unlike
        refreshParamDisplayPages() this should only run when the engine/table itself changed, not
        on every step-selection change. Ends by handing off to refreshParamDisplayPages() so
        paramDisplay's actual page set (lockablePages alone, or with a NOTE page prepended) stays
        correct for whatever's currently selected. Ctor calls it once for the default engine;
        applyEngine()/setHexLayer()/setSoloSynthBlock()/setSoloSynthInstance() call it on every
        engine/table switch. */
    void rebuildSynthParamPages();

    /** Populate sequence.lockable (+ sequence.engineTag) from kEngineSets[engine], discarding the
        previous engine's lockable list -- per-step locks in sequence.steps are untouched (they
        stay keyed by paramId in the step data, so they're simply inert until that engine is
        reselected). Pure state, no UI/guard -- both switchEngine() and the load path call this
        after deciding it's safe/appropriate to switch. */
    void seedLockableFromEngine (TrackEngine engine);

    /** Full engine switch: seedLockableFromEngine() + refresh the header label/combo + rebuild
        paramDisplay's pages + reset continuousLockables (sized for the OLD engine's lockable
        count, so it must not carry over). No playing/sync guard -- callers decide. */
    void applyEngine (TrackEngine newEngine);

    /** User-driven engine switch (the synth card's engine combo): guarded the same way
        syncBaseValuesFromSynth() is (refuses mid-playback/mid-sync, since both touch
        sequence.lockable while the shared timer may be using it) and reverts the combo's
        selection on refusal. */
    void switchEngine (TrackEngine newEngine);

    /** User-driven Hex Layer selector change (hexLayerCombo, only reachable while
        currentEngine == hexLayer). Same guard as switchEngine() (refuses mid-playback/mid-sync,
        reverts the combo on refusal) since it also rebuilds sequence.lockable. */
    void setHexLayer (int layer);

    /** Solo Synth block combo change (soloSynthBlockCombo, only reachable while
        currentEngine == soloSynth). Repopulates soloSynthInstanceCombo for the new block's
        instance count/labels and resets to instance 1, then behaves like setSoloSynthInstance(). */
    void setSoloSynthBlock (const juce::String& block);

    /** Solo Synth instance combo change. Same guard as setHexLayer()/switchEngine(). */
    void setSoloSynthInstance (int instance);

    /** Swap paramDisplay's page set to match whichever lane currently owns the edit target:
        lockablePages alone (Base mode, nothing selected anywhere), a NOTE page followed by
        lockablePages (the solo lane's own primary voice selected), or -- if a PCM/poly voice has
        a step selected -- a single-page NOTE/GATE/VEL editor for that step (raw cells, no
        ParamInfo/SysEx address; PCM/poly voices have no lockable params of their own). Cheap to
        call liberally; it only rebuilds paramDisplay's page SET when the target lane actually
        changed (cell values still refresh on every call). Called from the tail of
        refreshStepButtons() so every selection-changing action stays in sync automatically. */
    void refreshParamDisplayPages();
    /** Resolves displayedMelodicTarget's encoding to the actual casioxw::Step -- nullptr if
        nothing is selected there. Shared by refreshMelodicStepCellValues() (read) and
        onParamEdited()'s raw-cell branch (write). */
    casioxw::Step* melodicStepForTarget (int target);
    void refreshMelodicStepCellValues (int target);   // push the selected step's note/gate/vel into the screen
    /** Pushes a focused drum lane's base NOTE/VEL (row.note's slider value + row.baseVelocity)
        into the LCD's raw NOTE/VEL cells -- the drum-focus counterpart of
        refreshMelodicStepCellValues(), used when refreshParamDisplayPages() shows a drum lane's
        base page (see kDrumNoteCell/kDrumVelCell in SequencerPanel.cpp). */
    void refreshDrumBaseCellValues (int drumIndex);
    void updateStatusLabel();                  // edit-target readout in the display header
    void randomizeSequence();                  // Randomize button -> casioxw::randomize + resync widgets
    void showRandomizeOptions();               // call-out editing randomizeOptions in place
    void syncTransportWidgetsFromSequence();   // push channel/tempo/rate back into their widgets
    void saveSequenceToFile();
    void loadSequenceFromFile();
    void saveByKind (SaveKind kind);
    juce::String serializeDrumsToJson() const;
    juce::String serializePcmTracksToJson() const;
    /** casioxw::sequenceToJson(sequence) plus the app-level poly fields core doesn't know about
        (synthPolyMode/synthExtraVoices) -- see its .cpp doc comment. Use this instead of a bare
        sequenceToJson(sequence) call for every SAVE path (loading still goes through
        applySoloSequenceText(), which reads both the core fields and these). */
    juce::String serializeSoloSequenceToJson() const;
    juce::String serializeSequenceSetToJson (const juce::String& soloFile, const juce::String& drumsFile,
                                             const juce::String& pcmFile) const;
    bool applySoloSequenceText (const juce::String& text);
    bool applyDrumSequenceText (const juce::String& text);
    bool applyPcmTracksText (const juce::String& text);
    bool applyLoadedText (const juce::String& text, const juce::File& sourceFile);  // parse + adopt + resync
    void chooseSequenceDirectory();
    juce::File settingsFilePath() const;
    void loadSequenceSettings();
    void saveSequenceSettings() const;
    bool hasAnyDrumStepSelected() const;
    bool hasAnyPcmStepSelected() const;
    void clearDrumSelections();
    void clearPcmSelections();
    void clearSynthPolySelection();   // deselects the solo lane's poly sub-voice edit target
    void clearAllSteps();   // reset every lane's pattern (trigs + per-step notes/gate/vel + locks);
                            // keeps sound setup: channels, tempo/rate, mutes, and lockable base values
    void updateClearLocksEnabled();

    // scheduler tick: fill the look-ahead horizon with timestamped events. lookaheadMs defaults to
    // the small steady-state horizon; play() passes a deeper one-time value to prime the start.
    void feedLookahead (double lookaheadMs);
    void updatePlayheadStep(); // shared step-column playhead for synth + drum lanes

    // The p-lock transport seam. Builds the MIDI message(s) for one parameter change: NRPN where
    // mapped (to cut traffic on the live transport path), with SysEx fallback through the proven
    // codec for anything unmapped.
    std::vector<juce::MidiMessage> paramMessages (const juce::String& paramId, int instance, int value,
                                                  int channel) const;
    void sendParamNow (const juce::String& paramId, int instance, int value);   // immediate (audition/reset)

    casioxw::SysExCodec& codec;
    casioxw::MidiIO& midiIO;

    casioxw::Sequence sequence;                // source of truth (solo track: Solo Synth/Hex Layer/Organ)
    TrackEngine currentEngine = TrackEngine::soloSynth;
    juce::ComboBox engineCombo;                // selects which engine's lockable table `sequence.lockable` uses

    // Hex Layer's lockable table covers all 6 layers, but only ONE layer's per-layer params are
    // ever loaded into sequence.lockable at a time (same "one active selector" shape as
    // engineCombo itself) -- this combo picks which. Global/LFO params (shared across all 6
    // layers) are always present regardless of this selection. Zero-sized/inert while
    // currentEngine != hexLayer (see resized()).
    int currentHexLayer = 1;                   // 1-6
    juce::ComboBox hexLayerCombo;

    // Solo Synth's lockable table covers every param in xwp1.json's soloSynth section, but only
    // ONE (block, instance) pair's params are loaded into sequence.lockable at a time -- mirrors
    // SoloSynthPanel's own blockCombo/instanceCombo (OSC's 6 instances, PWM/LFO's 2, Etc/
    // TotalFilter's 1). Zero-sized/inert while currentEngine != soloSynth (see resized()).
    juce::String currentSoloSynthBlock;         // e.g. "OSC", "TotalFilter" -- populated from the model in the ctor
    int currentSoloSynthInstance = 1;
    juce::ComboBox soloSynthBlockCombo;
    juce::ComboBox soloSynthInstanceCombo;
    juce::StringArray soloSynthBlockOrder;      // blockCombo item index -> block name, built once

    // The solo lane's own poly mode -- only reachable while currentEngine is hexLayer or
    // drawbarOrgan (forced false/hidden for soloSynth, per owner's explicit "disabled for Solo
    // Synth" scope; see switchEngine()/applyEngine()). synthPolyStep/synthSelectedVoice are a
    // THIRD selection axis alongside selectedStep (see clearSynthPolySelection()'s doc comment).
    bool synthPolyMode = false;
    std::array<PolyVoice, kMaxPolyVoices - 1> synthExtraVoices;
    bool synthSubTracksExpanded = false;
    int synthPolyStep = -1;        // -1 == none; which step of synthSelectedVoice the screen edits
    int synthSelectedVoice = 0;    // 1..N == synthExtraVoices[v-1]; meaningful only while synthPolyStep >= 0
    juce::TextButton synthPolyToggle { "Poly" };
    juce::TextButton synthSubTrackArrow;

    std::array<std::unique_ptr<StepControl>, 16> stepControls;
    std::unique_ptr<ParamPageDisplay> paramDisplay;   // the pageable p-lock parameter sub-window
    std::array<std::unique_ptr<DrumTrackControl>, 5> drumTrackControls;
    std::array<std::unique_ptr<PcmTrackControl>, 4> pcmTrackControls;

    juce::TextButton playStopButton { "Play" };
    juce::TextButton randomizeButton { "Rnd" };
    juce::TextButton rndOptionsButton;                // "..." beside Rnd: opens the options call-out

    // Randomize tuning, edited live by the call-out. continuousLockables = the Slider-kind
    // lockable indices (combo/toggle params sit out unless randomizeComboParams is on).
    casioxw::RandomizeOptions randomizeOptions;
    bool randomizeComboParams = false;
    std::vector<int> continuousLockables;
    juce::Component::SafePointer<juce::CallOutBox> activeCallout;   // dismissed in the destructor
    juce::TextButton saveButton { "Save" };
    juce::TextButton loadButton { "Load" };
    juce::TextButton sequenceDirButton { "Seq Dir" };
    std::unique_ptr<juce::FileChooser> fileChooser;   // kept alive across the async dialog
    juce::Slider tempoSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider channelSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider stepCountSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ComboBox rateCombo;
    juce::Label tempoLabel { {}, "BPM" };
    juce::Label channelLabel { {}, "CH" };
    juce::Label rateLabel { {}, "RATE" };
    juce::Label stepCountLabel { {}, "STEPS" };

    // Paging (see kStepsPerPage/pageCount()'s doc comments): which 16-step slice of the pattern
    // every lane's windowed step buttons currently show. Manual navigation only for now (per the
    // owner's answer -- "make it configurable, start with whatever is easiest"); autoFollowPlayhead
    // is the seam a later "Follow" toggle would flip.
    int currentPage = 0;
    bool autoFollowPlayhead = false;
    juce::TextButton pageLeftButton  { "<" };
    juce::TextButton pageRightButton { ">" };
    juce::TextButton followPageToggle { "Follow" };   // ON == view auto-advances to the playhead's page
    PageIndicator pageIndicator;

    juce::Random rng;   // seeded from time; drives the Randomize button

    juce::TextButton baseButton { "Base" };
    juce::TextButton syncBaseButton { "Sync" };       // adopt the synth's live values as base
    juce::TextButton stepModeButton { "STEP" };       // grid mode: step keys toggle trigs
    juce::TextButton editButton { "P-LOCK" };         // toggle ON == p-lock edit mode
    juce::TextButton muteSynthButton { "Mute" };
    juce::TextButton clearLocksButton { "Clear Locks" };
    juce::TextButton clearAllButton { "Clear All" };  // wipe every lane's pattern (testing aid)
    juce::TextButton shiftLeftButton  { "<" };
    juce::TextButton shiftRightButton { ">" };
    juce::Label statusLabel;                          // footer: file/save/load messages only
    juce::Label drumTracksLabel { {}, "DRUM TRACKS" };
    juce::Label pcmTracksLabel { {}, "PCM TRACKS" };
    // Doubles as the solo lane's clickable focus label AND its current-engine readout ("Synth"/
    // "Hex Layer"/"Organ" -- a short form kept in sync by applyEngine() via shortEngineLabel(),
    // deliberately terser than EngineLockableSet::displayName so it fits the narrow lane-label-
    // gutter without juce::Label auto-shrinking it to a visibly smaller font than every other
    // track label; the initial text below matches the default engine so a fresh panel is correct
    // before any engine switch ever runs applyEngine()) -- positioned in resized() at the SAME
    // lane-label-gutter column a DrumTrackControl/PcmTrackControl row's trackLabel uses, not up in
    // the section header.
    juce::Label synthLabel { {}, "Synth" };

    // Card regions computed by resized(), painted by paint().
    juce::Rectangle<int> drumCardBounds;
    juce::Rectangle<int> pcmCardBounds;
    juce::Rectangle<int> synthCardBounds;

    // Look-ahead transport state (message thread only — the feeder timer runs there, so no locking
    // against `sequence` is needed; precise dispatch happens on JUCE's output thread instead).
    // transportStartMs is the juce::Time::getMillisecondCounter() wall-clock at which step 0 begins
    // (a few ms in the future, so the first events are validly "in the future"); nextStepStartMs is
    // the next unfed step's start, measured relative to it. prevStepIndex feeds scheduleStep()'s
    // param dedup (-1 only for the very first fed step, to establish the whole sound).
    double transportStartMs = 0.0;
    double nextStepStartMs  = 0.0;
    int    nextStepIndex    = 0;
    int    prevStepIndex    = -1;

    // The exact schedule the audio uses, so the visual playhead can track note-ons instead of
    // re-deriving position by dividing elapsed time by the *current* stepMs (which is only valid
    // if the tempo never changed since play began — dragging BPM/RATE mid-playback made that
    // formula drift the highlight ~8 steps away from the note that actually sounds). Each entry is
    // {absolute start ms (getMillisecondCounter base), stepIndex}, pushed by feedLookahead() as it
    // schedules each step and consumed by updatePlayheadStep() as `now` crosses each boundary.
    std::deque<std::pair<double, int>> scheduledPlayheadMarks;

    // Base-value sync state (message thread only). Keyed "paramId#instance" -> lockable index;
    // non-empty while a base sync is awaiting replies (the shared timer polls the receive queue).
    std::map<juce::String, int> outstandingBaseSync;
    juce::uint32 baseSyncStartedMs = 0;

    // Which lane+voice paramDisplay currently shows: -1 == nothing melodic selected (Base mode,
    // or a step selected on a lane with no NOTE page -- just this engine's p-lock pages, from
    // lockablePages); 0-3 == a PCM track's (kPcmTracks index) single-page NOTE/GATE/VEL step
    // editor for its PRIMARY voice; 10 + row*4 + voice (voice 1..3) == that PCM row's poly
    // extraVoices[voice-1]; 100 == the solo lane's OWN primary voice (sequence.steps) -- a NOTE
    // page followed by this engine's own p-lock pages, since unlike PCM/poly voices the solo lane
    // still has a real lockable set to page through; 100 + voice (voice 1..3) == the solo lane's
    // poly synthExtraVoices[voice-1]. Purely a cache so refreshParamDisplayPages() can skip
    // rebuilding pages when the target hasn't actually changed (still needs a distinct value per
    // VOICE, not just per row/lane, since switching voices within the same row needs a real
    // page-name/cell-source refresh).
    int displayedMelodicTarget = -1;

    // This engine's own p-lock pages (FILT/OSC/etc, built from sequence.lockable), cached by
    // rebuildSynthParamPages() so refreshParamDisplayPages() can prepend a NOTE page to it (solo
    // lane primary voice selected) or use it bare (Base mode / no step selected) without redoing
    // the range-seeding/continuousLockables side effects on every step-selection change.
    std::vector<ParamPageDisplay::Page> lockablePages;

    bool playing = false;
    int selectedStep = -1;                     // -1 == Base
    int playheadStep = -1;                     // -1 == hidden (stopped / before first step)
    juce::Rectangle<int> playheadLaneBounds;   // shared aligned step columns (drums + synth row)
    juce::File sequenceDefaultDirectory;

    // See FocusedTrackKind's doc comment. synthFocusBounds is the solo lane's counterpart to
    // DrumTrackControl/PcmTrackControl's own rowBounds (it has no per-row array to hold one),
    // captured by resized() and painted by paint() when soloSynth is the focused kind.
    FocusedTrackKind focusedTrackKind = FocusedTrackKind::soloSynth;
    int focusedTrackIndex = -1;                // meaningful only when focusedTrackKind != soloSynth
    juce::Rectangle<int> synthFocusBounds;

    // ---- Arranger sub-view (owner's brief: a Digitakt-style song table, reached as a MODE of
    // this tab rather than a separate top-level tab) --------------------------------------------
    juce::TextButton arrangerModeButton { "Arranger" };   // toggles between the step editor (this
                                                          // panel's own widgets) and arrangerPanel
    std::unique_ptr<ArrangerPanel> arrangerPanel;
    bool showingArranger = false;
    void setShowingArranger (bool shouldShow);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerPanel)
};
