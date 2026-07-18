#include "ParamPageDisplay.h"

#include "EditorLookAndFeel.h"
#include "casioxw/NoteNames.h"

namespace
{
    constexpr int kPageKeyHeight = 26;
    constexpr int kPageKeyGap    = 6;
    constexpr int kGlassPad      = 10;
    constexpr int kHeaderHeight  = 22;
    constexpr int kGridCols      = 4;
    constexpr int kGridRows      = 2;
}

//==============================================================================
/** One parameter cell on the glass: short name up top, rotary knob, live value readout below.
    Inverts (amber fill, dark text) while its parameter is locked on the selected step. */
struct ParamPageDisplay::Cell : public juce::Component
{
    Cell (ParamPageDisplay& ownerIn, const CellSpec& specIn, int pageIndexIn)
        : owner (ownerIn), spec (specIn), pageIndex (pageIndexIn)
    {
        kind = casioxw::decideControlKind (*spec.info, spec.instance);
        const auto enumName = casioxw::resolveEnumName (*spec.info, spec.instance);
        enumTable = enumName.isNotEmpty() ? owner.model.enumValues (enumName) : nullptr;

        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setRange ((double) spec.info->range.min, (double) spec.info->range.max, 1.0);
        knob.onValueChange = [this]
        {
            repaint();
            if (owner.onValueEdited != nullptr)
                owner.onValueEdited (spec.lockableIndex, (int) knob.getValue());
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
    juce::Slider knob;
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
            jassert (spec.info != nullptr);
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
