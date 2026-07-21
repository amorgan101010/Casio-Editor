#pragma once

#include "EditorLookAndFeel.h"

#include "casioxw/MidiIO.h"
#include "casioxw/Sequence.h"
#include "casioxw/Song.h"
#include "casioxw/SysExCodec.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

//==============================================================================
/** Digitakt-style song arranger: a scrollable table of rows, each row loading either one bundled
    sequence SET (.xwset -- solo+drums+pcm together, exclusive of anything else) or any combination
    of an independently-picked solo (.xwseq), drums (.xwdrm), and melody/pcm (.xwpcm) file, plus a
    per-row repeat count and a strip of 10 lane mutes (Solo Synth + Drum 1-5 + Bass/Solo1/Solo2/
    Chords -- SequencerPanel's own fixed lane roster, mirrored via casioxw::kSongLaneCount so a
    row's mute state means the same thing regardless of which files it loads).

    Deliberately an INDEPENDENT playback engine (owner's explicit choice over reusing the live step
    editor's state): this panel parses its own `casioxw::Sequence`/track copies from whichever
    files a row references and drives its own look-ahead feeder + casioxw::scheduleStep() calls,
    so playing the arrangement never disturbs whatever pattern is open in SequencerPanel's own step
    editor. The two transports are still mutually exclusive at the MidiIO/output level -- only one
    should be sending at a time -- enforced by SequencerPanel calling stop() on whichever panel
    isn't the one just pressed play.

    `casioxw::Song`'s tempoBpm is the single clock for the whole arrangement -- each row's own
    referenced file(s) may carry a different saved tempoBpm/stepsPerBeat, but during arranger
    playback every loaded lane's tempo is overridden to the song's tempo (only the RATE --
    stepsPerBeat, e.g. 16ths vs 8ths -- is taken from whichever lane defines the row's clock,
    same convention SequencerPanel itself already uses to keep drum/pcm lanes locked to the main
    sequence's rate). */
class ArrangerPanel : public juce::Component, private juce::Timer
{
public:
    ArrangerPanel (casioxw::SysExCodec& codec, casioxw::MidiIO& midiIO);
    ~ArrangerPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isPlaying() const { return playing; }

    /** Stops arranger playback if running: all-notes-off + reset every currently-loaded lane's
        p-lockable params to base, same reasoning as SequencerPanel::stop(). Public so SequencerPanel
        can enforce mutual exclusivity (only one transport sends MIDI at a time) when the OTHER
        transport's play button is pressed. */
    void stop();

    /** Called at the start of play(), before this panel starts sending -- SequencerPanel wires this
        to its own stop(), so pressing the arranger's Play button stops the step editor's transport
        first (the two share one MidiIO output; only one should be scheduling at a time). */
    std::function<void()> beforePlay;

    /** Shared with SequencerPanel's own "Seq Dir" setting, so both panels browse the same saved-
        sequence library and a Song's relative file references resolve consistently. */
    void setSequenceDirectory (const juce::File& dir);

    /** tools/gui-preview only: seed a representative multi-row arrangement (a couple of rows with
        files assigned, some lanes muted, one marked as the "currently playing" row) so an offscreen
        snapshot can verify the table's real layout rather than an empty one-row default. Never
        called by the app itself. */
    void applyPreviewDemoState();

    /** One drum lane's musical content, parsed independently from a .xwdrm file's "tracks" array
        (SequencerPanel::serializeDrumsToJson's shape) -- NOT casioxw::Sequence, because that's not
        how drum lanes are stored (a fixed note/velocity per lane, not a per-step melodic Sequence,
        matching SequencerPanel::DrumTrackControl). Public only so the free parsing helper in
        ArrangerPanel.cpp's anonymous namespace can name the type; nothing outside this file uses it. */
    struct DrumTrackData
    {
        int channel = 10;
        int note = 36;
        int baseVelocity = 100;
        std::array<bool, 16> steps {};
        std::array<std::optional<int>, 16> velocityLocks {};
    };

private:
    //==========================================================================
    /** The fully-resolved, independent copy of whatever a row's referenced file(s) contain --
        reloaded fresh at every row-boundary from the row's current file references, never shared
        with SequencerPanel's live widgets. */
    struct RowRuntime
    {
        std::optional<casioxw::Sequence> solo;
        bool hasDrums = false;
        std::array<DrumTrackData, 5> drums {};
        bool hasPcm = false;
        std::array<std::optional<casioxw::Sequence>, 4> pcm {};
    };

    /** One row's widgets. `owner` lets each widget's callback find its own row index by scanning
        `rowWidgets` for `this` at call time (rather than capturing an index that would go stale the
        moment an earlier row is removed). */
    struct RowWidgets : public juce::Component
    {
        juce::Label indexLabel;
        juce::TextEditor labelEditor;
        juce::ComboBox setCombo;
        juce::ComboBox soloCombo;
        juce::ComboBox drumsCombo;
        juce::ComboBox pcmCombo;
        juce::Label fromSetLabel { {}, juce::CharPointer_UTF8 ("\xc2\xb7 from set \xc2\xb7") };   // shown instead of the 3 combos when setCombo != none
        juce::Slider repeatSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxRight };

        // Elektron-style loop line: 0 == no loop line on this row. Count/infinite are hidden
        // (nothing to configure) while loopBackSlider reads 0.
        juce::Slider loopBackSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxRight };
        juce::Slider loopCountSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxRight };
        juce::TextButton loopInfiniteButton { juce::CharPointer_UTF8 ("\xe2\x88\x9e") };   // "∞"

        std::array<juce::TextButton, casioxw::kSongLaneCount> muteChips;
        juce::TextButton removeButton { juce::CharPointer_UTF8 ("\xc3\x97") };

        void resized() override;
    };

    void addRow();
    void removeRow (RowWidgets* widgets);
    void rebuildRowWidgets();       // full teardown+rebuild of rowWidgets from song.rows (add/remove only)
    void configureRowWidgets (RowWidgets& w);        // one-time wiring of a fresh RowWidgets' callbacks
    void syncRowWidgetsFromSong (int rowIndex);      // push song.rows[rowIndex] state into its widgets
    void onRowFieldChanged (RowWidgets& w);          // pull widget state back into song.rows[index-of(w)]
    void updateLoopWidgetVisibility (RowWidgets& w); // show loopCount/Infinite only when loopBack > 0
    int indexOfWidgets (const RowWidgets* w) const;
    void refreshFileCombos();       // rescan sequenceDirectory, repopulate every row's 4 file combos
    void populateFileCombo (juce::ComboBox& combo, const juce::String& wildcard, const juce::String& currentValue) const;
    void layoutRowContainer();

    RowRuntime loadRowRuntime (const casioxw::SongRow& row) const;
    juce::File resolveFile (const juce::String& relativeName) const;

    void play();
    void timerCallback() override;
    void feedLookahead (double lookaheadMs);
    void resetCurrentRuntimeToBase();

    /** Queues only the lockable params whose effective value at `stepIndex` actually differs from
        `lastAppliedParams` (the last value this engine believes is on the device), instead of a
        full kPrevStepFresh-style dump of every lockable param -- that dump is exactly the "SysEx
        burst fired alongside the first notes" lurch SequencerPanel's own play() already had to
        avoid (see its kPrevStepBaseline comment), just recurring at every row transition here
        instead of once at song start. The resulting batch is paced/capped by scheduleParamBurst()
        (ArrangerPanel.cpp, anonymous namespace) rather than stamped at one instant -- `stepMs` is
        the current step interval, needed to bound how far the pacing may reach back. Updates
        lastAppliedParams only for whatever it actually sends (not anything truncated away). Called
        at every row transition (never mid-row -- normal per-step p-locks are paced the same way
        directly in feedLookahead()). */
    void queueDiffEstablish (juce::MidiBuffer& buffer, const casioxw::Sequence& seq, int stepIndex,
                             double timeMs, double stepMs);

    std::vector<juce::MidiMessage> paramMessages (const juce::String& paramId, int instance, int value) const;
    void sendParamNow (const juce::String& paramId, int instance, int value);

    void saveSongToFile();
    void loadSongFromFile();
    void applyLoadedSong (const casioxw::Song& loaded);

    casioxw::SysExCodec& codec;
    casioxw::MidiIO& midiIO;
    juce::File sequenceDirectory;

    casioxw::Song song;

    juce::Label titleLabel { {}, "ARRANGER" };
    juce::TextButton playStopButton { "Play" };
    juce::TextButton loopArrangementButton { "Loop" };   // whole-arrangement loop (song.loopEnabled);
                                                          // per-row loop LINES are the loopBack/loopCount
                                                          // controls on each RowWidgets instead
    juce::TextButton addRowButton { "+ Row" };
    juce::TextButton refreshButton { "Refresh" };
    juce::TextButton saveButton { "Save Song" };
    juce::TextButton loadButton { "Load Song" };
    juce::Slider tempoSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label tempoLabel { {}, "BPM" };
    juce::Label statusLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Label colLabelHeader { {}, "LABEL" };
    juce::Label colContentHeader { {}, "CONTENT (SET, or SOLO / DRUMS / MELODY)" };
    juce::Label colRepeatHeader { {}, "REPEAT" };
    juce::Label colLoopHeader { {}, "LOOP LINE (back / times)" };
    juce::Label colMuteHeader { {}, juce::CharPointer_UTF8 ("MUTE: SYNTH \xc2\xb7 DRUMS 1-5 \xc2\xb7 BASS/SOLO1/SOLO2/CHORDS") };

    /** Arrangement-wide mute strip (song.globalLaneMuted), pinned above the scrolling row table --
        NOT part of rowWidgets/rowContainer, since it applies across every row rather than to one.
        Laid out with the exact same per-lane x-offsets as RowWidgets::muteChips (both go through
        the shared layoutLaneChips() helper in ArrangerPanel.cpp) so it visually aligns as a "master
        row" sitting directly above the per-row mute chips it overrides. No caption label -- the
        band + alignment with the per-row chips it sits above is what identifies it. */
    std::array<juce::TextButton, casioxw::kSongLaneCount> globalMuteChips;
    // Full-width strip behind globalMuteChips, painted in paint() -- gives the global row its own
    // visually distinct band instead of reading as one more line of header text stacked against
    // colLoopHeader/colMuteHeader above it.
    juce::Rectangle<int> globalMuteRowBounds;

    juce::Viewport viewport;
    juce::Component rowContainer;
    std::vector<std::unique_ptr<RowWidgets>> rowWidgets;

    // ---- independent playback state (never touches SequencerPanel's own sequence/tracks) -------
    bool playing = false;
    bool songEnded = false;
    casioxw::SongPosition currentPosition { 0, 0, {} };
    int  runtimeLoadedForRow = -1;   // which song.rows[] index `currentRuntime` was built from, -1 == none yet
    RowRuntime currentRuntime;
    // Every row's files parsed ONCE at play() (disk I/O + JSON parsing), never from inside the
    // real-time feeder -- loadRowRuntime() used to be called mid-feed at every row boundary, which
    // stalled the message thread long enough to cause audible lateness ("lurching"), worse with
    // more/shorter rows (more boundaries hit during playback).
    std::vector<RowRuntime> preloadedRuntimes;
    int lastHighlightedRow = -1;   // paints only when the playing row actually changes, not every tick

    // What this engine believes is currently on the device for each lockable param, keyed
    // "paramId#instance" (matching SequencerPanel::outstandingBaseSync's key convention). Updated
    // every time a paramChange is actually scheduled (see feedLookahead()/queueDiffEstablish()) so
    // a row transition can send only what changed instead of a full re-establish burst. Cleared at
    // play() -- the device's actual state is unknown at song start, same reasoning as
    // prevStepIndex's kPrevStepFresh there.
    std::map<juce::String, int> lastAppliedParams;

    double transportStartMs = 0.0;
    double nextStepStartMs  = 0.0;
    int    nextStepIndex    = 0;
    int    prevStepIndex    = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangerPanel)
};
