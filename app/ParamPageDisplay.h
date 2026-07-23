#pragma once

#include "casioxw/ParamModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

//==============================================================================
/** The sequencer's pageable parameter sub-window, drawn as a hardware LCD (the Digitakt screen
    metaphor): a near-black glass panel showing up to 8 parameter cells (2 rows x 4 columns) for
    ONE page at a time, with page-select keys underneath — because the p-lockable parameter set is
    far too large to show as one flat list.

    Each cell is a compact rotary knob + short name + live value readout, phosphor-cyan on glass.
    A cell whose parameter holds a p-lock on the currently selected step renders INVERTED (amber
    fill, dark text) — the same convention the Digitakt manual uses for a locked value.

    Dumb like ParamControl: it knows nothing about the Sequence or MIDI. The owner describes the
    pages once (setPages), pushes state in (setCellState / setStatus), and receives edits back
    through onValueEdited(lockableIndex, value). lockableIndex is the owner's own flat index
    (SequencerPanel: the index into sequence.lockable), carried through untouched.

    A cell doesn't have to be a real synth parameter: CellSpec::info may be nullptr for a "raw"
    cell (range/formatting from rawMin/rawMax/rawFormat instead) -- SequencerPanel uses this for a
    PCM track's per-step NOTE/GATE/VEL, which have no SysEx address at all. setPages() can be
    called again later to swap the whole page set (e.g. between the Solo Synth's lockable pages
    and a PCM track's step-edit page) -- it fully rebuilds, so do it on selection changes, not
    per-frame. */
class ParamPageDisplay : public juce::Component
{
public:
    /** How a raw (non-ParamInfo) cell's value is formatted for display. Ignored when `info` is
        set -- that path already derives formatting from the param's own metadata (unit=="note",
        enum tables, etc). GateLength is its own thing, not a generic percent: 1..100 reads as a
        percent, but the knob can't land in between two whole-step multiples above 100 (snapped via
        casioxw::snapGatePercent), so those read "2x".."16x" instead. */
    enum class ValueFormat { Plain, Note, GateLength };

    struct CellSpec
    {
        // ParamInfo-backed cell (the usual case: a real synth parameter from codec.model()):
        const casioxw::ParamInfo* info = nullptr;   // must outlive the display (codec.model())
        int instance = 1;

        // Raw cell (info == nullptr): a value with no SysEx address at all -- e.g. a sequencer
        // step's note/gate/velocity. Range and formatting come from these fields instead.
        int rawMin = 0;
        int rawMax = 127;
        ValueFormat rawFormat = ValueFormat::Plain;

        juce::String shortName;                     // 3-4 char hardware-style label ("CUT", "RES")
        int lockableIndex = 0;                      // owner's flat index, echoed in onValueEdited
    };

    struct Page
    {
        juce::String name;              // page-key cap, e.g. "FLTR"
        std::vector<CellSpec> cells;    // up to 8 (2 rows x 4 columns)
    };

    explicit ParamPageDisplay (const casioxw::ParamModel& model);
    ~ParamPageDisplay() override;

    /** Build the cell grid + page keys. Call once after construction (rebuilding later is
        allowed but the owner has no need to). */
    void setPages (std::vector<Page> pages);

    void setPage (int pageIndex);

    /** Header line inside the glass, left-justified — the edit-target readout
        ("P-LOCK  STEP 05", "BASE SOUND", ...). The page name/count fills the right side. */
    void setStatus (const juce::String& text);

    /** Push one parameter's current value + locked flag into its cell (never fires
        onValueEdited). */
    void setCellState (int lockableIndex, int value, bool locked);

    /** Fired once per user-driven knob edit: (lockableIndex, new UI-space value). */
    std::function<void (int lockableIndex, int value)> onValueEdited;

    /** Fired on a cell's double-click, INSTEAD of onValueEdited -- resetting is an action the
        owner must interpret (e.g. SequencerPanel: clear the step's p-lock if this cell is
        currently locked, reverting it to the base value), not a value ParamPageDisplay could
        compute itself. Never fired for a raw cell with no lockableIndex semantics of its own. */
    std::function<void (int lockableIndex)> onValueReset;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Cell;

    const casioxw::ParamModel& model;
    std::vector<Page> pageDefs;
    std::vector<std::unique_ptr<Cell>> cells;                 // all pages' cells, flat
    std::vector<std::unique_ptr<juce::TextButton>> pageKeys;
    int currentPage = 0;
    juce::String statusText;

    juce::Rectangle<int> glassBounds() const;
    void updateCellVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamPageDisplay)
};
