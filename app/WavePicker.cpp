#include "WavePicker.h"

namespace
{
    constexpr int kPopupWidth = 280;
    constexpr int kPopupHeight = 340;
    constexpr int kSearchBoxHeight = 26;
    constexpr int kRowHeight = 22;

    /** The CallOutBox's content: a search box over a virtualized juce::ListBox. Filtering just
        rebuilds a small std::vector<int> of matching indices (a few hundred microseconds even
        for soloPcmWaves' 2158 entries -- string compares are cheap, it's PopupMenu's per-item
        Component construction that isn't) and calls ListBox::updateContent(), which only
        re-fetches rows currently on screen. */
    class WavePickerContent : public juce::Component,
                              private juce::ListBoxModel,
                              private juce::TextEditor::Listener
    {
    public:
        WavePickerContent (const std::vector<casioxw::EnumEntry>& entriesIn, int currentValue,
                           std::function<void (int)> onPick)
            : entries (entriesIn), onPickCallback (std::move (onPick))
        {
            searchBox.setTextToShowWhenEmpty ("Search waves...", juce::Colours::grey);
            searchBox.addListener (this);
            searchBox.onReturnKey = [this]
            {
                if (! filtered.empty())
                    pick (0);
            };
            addAndMakeVisible (searchBox);

            listBox.setModel (this);
            listBox.setRowHeight (kRowHeight);
            addAndMakeVisible (listBox);

            applyFilter ({});

            for (int i = 0; i < (int) filtered.size(); ++i)
                if (filtered[(size_t) i] == currentValue)
                {
                    listBox.selectRow (i, false, true);
                    break;
                }

            setSize (kPopupWidth, kPopupHeight);
            searchBox.grabKeyboardFocus();
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (4);
            searchBox.setBounds (b.removeFromTop (kSearchBoxHeight));
            b.removeFromTop (4);
            listBox.setBounds (b);
        }

        int getNumRows() override { return (int) filtered.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool rowIsSelected) override
        {
            if (! juce::isPositiveAndBelow (row, (int) filtered.size()))
                return;

            if (rowIsSelected)
                g.fillAll (juce::Colours::orange.withAlpha (0.3f));

            const auto& e = entries[(size_t) filtered[(size_t) row]];
            g.setColour (juce::Colours::white);
            g.drawText (juce::String (e.value) + "  " + e.label, 6, 0, width - 8, height,
                       juce::Justification::centredLeft);
        }

        void listBoxItemClicked (int row, const juce::MouseEvent&) override { pick (row); }
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { pick (row); }

    private:
        const std::vector<casioxw::EnumEntry>& entries;
        std::vector<int> filtered;   // indices into `entries`
        std::function<void (int)> onPickCallback;

        juce::TextEditor searchBox;
        juce::ListBox listBox;

        void pick (int row)
        {
            if (juce::isPositiveAndBelow (row, (int) filtered.size()) && onPickCallback)
                onPickCallback (entries[(size_t) filtered[(size_t) row]].value);
        }

        void textEditorTextChanged (juce::TextEditor&) override { applyFilter (searchBox.getText()); }

        void applyFilter (const juce::String& query)
        {
            filtered.clear();
            for (int i = 0; i < (int) entries.size(); ++i)
                if (query.isEmpty() || entries[(size_t) i].label.containsIgnoreCase (query))
                    filtered.push_back (i);
            listBox.updateContent();
            listBox.repaint();
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavePickerContent)
    };
}

WavePicker::WavePicker()
{
    button.onClick = [this] { showPicker(); };
    addAndMakeVisible (button);
    updateButtonText();
}

WavePicker::~WavePicker()
{
    if (activeCallout != nullptr)
        activeCallout->dismiss();
}

void WavePicker::setEntries (const std::vector<casioxw::EnumEntry>* entriesIn)
{
    entries = entriesIn;
    updateButtonText();
}

void WavePicker::setSelectedValue (int value, juce::NotificationType notification)
{
    selectedValue = entries != nullptr && ! entries->empty()
                       ? juce::jlimit (0, (int) entries->size() - 1, value)
                       : value;
    updateButtonText();

    if (notification == juce::sendNotification && onValueChanged)
        onValueChanged (selectedValue);
}

void WavePicker::updateButtonText()
{
    if (entries != nullptr)
        for (const auto& e : *entries)
            if (e.value == selectedValue)
            {
                button.setButtonText (juce::String (e.value) + "  " + e.label);
                return;
            }

    button.setButtonText (selectedValue >= 0 ? juce::String (selectedValue) : "-");
}

void WavePicker::showPicker()
{
    if (entries == nullptr || entries->empty())
        return;

    auto content = std::make_unique<WavePickerContent> (*entries, selectedValue,
        [this] (int value)
        {
            setSelectedValue (value, juce::sendNotification);
            if (activeCallout != nullptr)
                activeCallout->dismiss();
        });

    activeCallout = &juce::CallOutBox::launchAsynchronously (std::move (content),
                                                             button.getScreenBounds(), nullptr);
}

void WavePicker::resized()
{
    button.setBounds (getLocalBounds());
}
