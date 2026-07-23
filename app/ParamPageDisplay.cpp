#include "ParamPageDisplay.h"

#include "EditorLookAndFeel.h"
#include "casioxw/NoteNames.h"
#include "casioxw/Sequence.h"

namespace
{
    constexpr int kPageKeyHeight = 26;
    constexpr int kPageKeyGap    = 6;
    constexpr int kGlassPad      = 10;
    constexpr int kHeaderHeight  = 22;
    constexpr int kGridCols      = 4;
    constexpr int kGridRows      = 2;

    // The GATE knob's drag sweep: half for the continuous 1..100% zone, half for the 15 whole-step
    // multiples (2x..16x) above it -- a plain linear 1..1600 range would squeeze the common
    // 1..100% zone into ~6% of the sweep (100 of 1600 units), which read as "stuck near zero" for
    // any everyday gate value. snapToLegalValue enforces the actual constraint (no landing between
    // two multiples above 100%); the split just keeps both zones comfortable to dial in.
    constexpr double kGateSplit = 0.5;

    juce::NormalisableRange<double> makeGateRange (double rangeMin, double rangeMax)
    {
        return juce::NormalisableRange<double> (
            rangeMin, rangeMax,
            [] (double, double rangeEnd, double proportion) -> double   // convertFrom0To1
            {
                if (proportion <= kGateSplit)
                    return 1.0 + (proportion / kGateSplit) * 99.0;
                const double t = (proportion - kGateSplit) / (1.0 - kGateSplit);
                return 200.0 + t * (rangeEnd - 200.0);
            },
            [] (double, double rangeEnd, double value) -> double        // convertTo0To1
            {
                if (value <= 100.0)
                    return (value - 1.0) / 99.0 * kGateSplit;
                const double t = (value - 200.0) / (rangeEnd - 200.0);
                return kGateSplit + juce::jlimit (0.0, 1.0, t) * (1.0 - kGateSplit);
            },
            [] (double, double rangeEnd, double value) -> double        // snapToLegalValue
            {
                // rangeEnd IS this cell's gate ceiling (maxGatePercent(stepCount), always a clean
                // multiple of 100) -- dividing back out recovers the stepCount snapGatePercent needs,
                // with no extra state to capture into this otherwise-stateless lambda.
                const int stepCount = (int) std::llround (rangeEnd / 100.0);
                return (double) casioxw::snapGatePercent ((int) std::llround (value), stepCount);
            });
    }

    // A double-click reset here isn't "set this value" (juce::Slider::setDoubleClickReturnValue's
    // usual job) -- it's an ACTION the owner must interpret against p-lock state (clear the lock
    // if the cell is currently locked; see SequencerPanel::onParamReset). Overriding
    // mouseDoubleClick intercepts it entirely rather than letting Slider turn it into a value
    // change that would flow through onValueEdited and (while a step is selected) create a NEW
    // lock at that value instead of clearing the existing one.
    class ResetOnDoubleClickSlider : public juce::Slider
    {
    public:
        using juce::Slider::Slider;
        std::function<void()> onDoubleClick;
        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            if (onDoubleClick)
                onDoubleClick();
        }
    };
}

//==============================================================================
/** One parameter cell on the glass: short name up top, rotary knob, live value readout below.
    Inverts (amber fill, dark text) while its parameter is locked on the selected step. */
struct ParamPageDisplay::Cell : public juce::Component
{
    Cell (ParamPageDisplay& ownerIn, const CellSpec& specIn, int pageIndexIn)
        : owner (ownerIn), spec (specIn), pageIndex (pageIndexIn)
    {
        int rangeMin = spec.rawMin, rangeMax = spec.rawMax;
        if (spec.info != nullptr)
        {
            kind = casioxw::decideControlKind (*spec.info, spec.instance);
            const auto enumName = casioxw::resolveEnumName (*spec.info, spec.instance);
            enumTable = enumName.isNotEmpty() ? owner.model.enumValues (enumName) : nullptr;
            rangeMin = spec.info->range.min;
            rangeMax = spec.info->range.max;
        }

        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        if (spec.info == nullptr && spec.rawFormat == ValueFormat::GateLength)
            knob.setNormalisableRange (makeGateRange ((double) rangeMin, (double) rangeMax));
        else
            knob.setRange ((double) rangeMin, (double) rangeMax, 1.0);
        knob.onValueChange = [this]
        {
            repaint();
            if (owner.onValueEdited != nullptr)
                owner.onValueEdited (spec.lockableIndex, (int) knob.getValue());
        };
        knob.onDoubleClick = [this]
        {
            if (owner.onValueReset != nullptr)
                owner.onValueReset (spec.lockableIndex);
        };
        addAndMakeVisible (knob);
        applyColours();
    }

    void setLocked (bool shouldLock)
    {
        if (locked == shouldLock)
            return;
        locked = shouldLock;
        applyColours();
        repaint();
    }

    void applyColours()
    {
        using namespace EditorColours;
        // On amber (locked) the arc flips dark so the inversion is total, not just the text.
        knob.setColour (juce::Slider::rotarySliderFillColourId,    locked ? base03 : screenAccent);
        knob.setColour (juce::Slider::rotarySliderOutlineColourId, locked ? screenInvert.darker (0.35f)
                                                                          : screenDim.withAlpha (0.35f));
    }

    juce::String valueText() const
    {
        const int v = (int) knob.getValue();

        if (spec.info == nullptr)
        {
            switch (spec.rawFormat)
            {
                case ValueFormat::Note: return casioxw::midiNoteName (v);
                case ValueFormat::GateLength:
                    return v <= 100 ? juce::String (v) + "%" : juce::String (v / 100) + "x";
                case ValueFormat::Plain: default: return juce::String (v);
            }
        }

        if (kind == casioxw::ControlKind::Toggle)
            return v != 0 ? "ON" : "OFF";
        if (enumTable != nullptr)
        {
            for (const auto& e : *enumTable)
                if (e.value == v)
                    return e.label;
        }
        if (spec.info->unit == "note")
            return casioxw::midiNoteName (v);
        return juce::String (v);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        if (locked)
        {
            g.setColour (EditorColours::screenInvert);
            g.fillRoundedRectangle (b.toFloat().reduced (2.0f), 5.0f);
        }

        const auto nameColour  = locked ? EditorColours::base03 : EditorColours::screenDim;
        const auto valueColour = locked ? EditorColours::base03 : EditorColours::screenText;

        g.setFont (EditorFonts::mono (10.5f, true));
        g.setColour (nameColour);
        g.drawFittedText (spec.shortName.toUpperCase(), b.removeFromTop (14).reduced (3, 0),
                          juce::Justification::centred, 1);

        g.setFont (EditorFonts::mono (12.5f, locked));
        g.setColour (valueColour);
        g.drawFittedText (valueText(), b.removeFromBottom (16).reduced (2, 0),
                          juce::Justification::centred, 1);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        b.removeFromTop (14);
        b.removeFromBottom (16);
        knob.setBounds (b.reduced (2));
    }

    ParamPageDisplay& owner;
    CellSpec spec;
    int pageIndex = 0;
    casioxw::ControlKind kind = casioxw::ControlKind::Slider;
    const std::vector<casioxw::EnumEntry>* enumTable = nullptr;
    ResetOnDoubleClickSlider knob;
    bool locked = false;
};

//==============================================================================
ParamPageDisplay::ParamPageDisplay (const casioxw::ParamModel& modelIn)
    : model (modelIn)
{
}

ParamPageDisplay::~ParamPageDisplay() = default;

void ParamPageDisplay::setPages (std::vector<Page> pages)
{
    cells.clear();
    pageKeys.clear();
    pageDefs = std::move (pages);
    currentPage = 0;

    for (size_t p = 0; p < pageDefs.size(); ++p)
    {
        for (const auto& spec : pageDefs[p].cells)
        {
            // spec.info == nullptr is valid -- a "raw" cell (e.g. a sequencer step's note/gate/
            // velocity) with no SysEx address, formatted via spec.rawFormat instead.
            auto cell = std::make_unique<Cell> (*this, spec, (int) p);
            addChildComponent (*cell);   // visibility managed per page
            cells.push_back (std::move (cell));
        }

        auto key = std::make_unique<juce::TextButton> (pageDefs[p].name);
        key->setClickingTogglesState (true);
        key->setRadioGroupId (0x9A6E);
        key->setColour (juce::TextButton::buttonColourId, EditorColours::panelBg);
        key->setColour (juce::TextButton::buttonOnColourId, EditorColours::orange);
        key->setColour (juce::TextButton::textColourOnId, EditorColours::base03);
        const int page = (int) p;
        key->onClick = [this, page] { setPage (page); };
        addAndMakeVisible (*key);
        pageKeys.push_back (std::move (key));
    }

    updateCellVisibility();
    resized();
}

void ParamPageDisplay::setPage (int pageIndex)
{
    currentPage = juce::jlimit (0, juce::jmax (0, (int) pageDefs.size() - 1), pageIndex);
    updateCellVisibility();
    repaint();
}

void ParamPageDisplay::updateCellVisibility()
{
    for (auto& cell : cells)
        cell->setVisible (cell->pageIndex == currentPage);
    for (size_t p = 0; p < pageKeys.size(); ++p)
        pageKeys[p]->setToggleState ((int) p == currentPage, juce::dontSendNotification);
}

void ParamPageDisplay::setStatus (const juce::String& text)
{
    if (statusText == text)
        return;
    statusText = text;
    repaint (glassBounds().removeFromTop (kGlassPad + kHeaderHeight));
}

void ParamPageDisplay::setCellState (int lockableIndex, int value, bool locked)
{
    for (auto& cell : cells)
    {
        if (cell->spec.lockableIndex != lockableIndex)
            continue;
        cell->knob.setValue ((double) value, juce::dontSendNotification);
        cell->setLocked (locked);
        cell->repaint();   // value readout is painted, not a child widget
        return;
    }
}

juce::Rectangle<int> ParamPageDisplay::glassBounds() const
{
    return getLocalBounds().withTrimmedBottom (kPageKeyHeight + kPageKeyGap);
}

void ParamPageDisplay::paint (juce::Graphics& g)
{
    auto glass = glassBounds().toFloat();

    g.setColour (EditorColours::screenBg);
    g.fillRoundedRectangle (glass, 6.0f);
    g.setColour (EditorColours::border.withAlpha (0.8f));
    g.drawRoundedRectangle (glass.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (EditorColours::screenLine);
    g.drawRoundedRectangle (glass.reduced (2.0f), 4.5f, 1.0f);

    auto header = glassBounds().reduced (kGlassPad, 0).removeFromTop (kGlassPad + kHeaderHeight)
                               .withTrimmedTop (kGlassPad);

    g.setFont (EditorFonts::mono (12.0f, true));
    g.setColour (EditorColours::screenText);
    g.drawText (statusText.toUpperCase(), header, juce::Justification::centredLeft);

    if (! pageDefs.empty())
    {
        const auto& page = pageDefs[(size_t) currentPage];
        g.setColour (EditorColours::screenDim);
        g.drawText (page.name.toUpperCase() + "  " + juce::String (currentPage + 1) + "/"
                        + juce::String ((int) pageDefs.size()),
                    header, juce::Justification::centredRight);
    }

    g.setColour (EditorColours::screenLine);
    g.drawHorizontalLine (header.getBottom() + 3, (float) header.getX(), (float) header.getRight());
}

void ParamPageDisplay::resized()
{
    auto keysRow = getLocalBounds().removeFromBottom (kPageKeyHeight);
    int keyX = keysRow.getX();
    for (auto& key : pageKeys)
    {
        key->setBounds (keyX, keysRow.getY(), 64, keysRow.getHeight());
        keyX += 64 + 4;
    }

    auto grid = glassBounds().reduced (kGlassPad).withTrimmedTop (kHeaderHeight + 4);
    const int cellW = grid.getWidth() / kGridCols;
    const int cellH = grid.getHeight() / kGridRows;

    std::vector<int> perPageCount ((size_t) juce::jmax (1, (int) pageDefs.size()), 0);
    for (auto& cell : cells)
    {
        const int i = perPageCount[(size_t) cell->pageIndex]++;
        const int col = i % kGridCols;
        const int row = i / kGridCols;
        cell->setBounds (grid.getX() + col * cellW, grid.getY() + row * cellH, cellW, cellH);
    }
}
