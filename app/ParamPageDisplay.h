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
    (SequencerPanel: the index into sequence.lockable), carried through untouched. */
class ParamPageDisplay : public juce::Component
{
public:
    struct CellSpec
    {
        const casioxw::ParamInfo* info = nullptr;   // must outlive the display (codec.model())
        int instance = 1;
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
