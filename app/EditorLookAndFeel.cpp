#include "EditorLookAndFeel.h"

using namespace EditorColours;

namespace
{
    // LookAndFeel_V4::ColourScheme's 9-arg ctor takes (windowBg, widgetBg, menuBg, outline,
    // defaultText, defaultFill, highlightedText, highlightedFill, menuText) — this one call re-skins
    // the stock buttons/sliders/comboboxes/scrollbars everywhere they aren't already given an
    // explicit per-component colour (e.g. the sequencer's step buttons keep their own state colours).
    juce::LookAndFeel_V4::ColourScheme solarizedScheme()
    {
        return juce::LookAndFeel_V4::ColourScheme (
            chassisBg,           // windowBackground
            panelBg,             // widgetBackground
            base02.darker (0.2f), // menuBackground
            border,               // outline
            textPrimary,          // defaultText
            panelBg,              // defaultFill
            base03,                // highlightedText (on an orange fill, needs to read dark)
            orange,                // highlightedFill
            textPrimary);          // menuText
    }
}

EditorLookAndFeel::EditorLookAndFeel()
{
    setColourScheme (solarizedScheme());

    // A few widget-specific colours ColourScheme doesn't reach directly.
    setColour (juce::TextButton::buttonColourId, panelBg);
    setColour (juce::TextButton::buttonOnColourId, orange);
    setColour (juce::TextButton::textColourOnId, base03);
    setColour (juce::TextButton::textColourOffId, textPrimary);

    setColour (juce::ToggleButton::textColourId, textPrimary);
    setColour (juce::ToggleButton::tickColourId, orange);
    setColour (juce::ToggleButton::tickDisabledColourId, textMuted);

    setColour (juce::Slider::backgroundColourId, panelBg);
    setColour (juce::Slider::trackColourId, blue);
    setColour (juce::Slider::thumbColourId, orange);
    setColour (juce::Slider::textBoxTextColourId, textPrimary);
    setColour (juce::Slider::textBoxBackgroundColourId, chassisBg);
    setColour (juce::Slider::textBoxOutlineColourId, border);

    setColour (juce::ComboBox::backgroundColourId, chassisBg);
    setColour (juce::ComboBox::outlineColourId, border);
    setColour (juce::ComboBox::textColourId, textPrimary);
    setColour (juce::ComboBox::arrowColourId, base1);

    setColour (juce::Label::textColourId, textPrimary);

    setColour (juce::TooltipWindow::backgroundColourId, base02.darker (0.15f));
    setColour (juce::TooltipWindow::textColourId, textPrimary);
    setColour (juce::TooltipWindow::outlineColourId, border);

    setColour (juce::DocumentWindow::backgroundColourId, chassisBg);
    setColour (juce::ResizableWindow::backgroundColourId, chassisBg);

    setColour (juce::TextEditor::backgroundColourId, chassisBg);
    setColour (juce::TextEditor::textColourId, textPrimary);
    setColour (juce::TextEditor::outlineColourId, border);
    setColour (juce::TextEditor::focusedOutlineColourId, orange);

    setColour (juce::PopupMenu::backgroundColourId, base02.darker (0.2f));
    setColour (juce::PopupMenu::textColourId, textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, orange);
    setColour (juce::PopupMenu::highlightedTextColourId, base03);
}

//==============================================================================
// Rotary knob drawn like a panel indicator: a recessed "glass" body, a track arc in panelBg, a
// value arc + pointer in orange when live, muted grey when disabled. Chosen over the stock V4 dial
// because these knobs are the single most-repeated widget in the app (every step's note/gate/
// velocity, every solo-synth Slider param rendered as RenderMode::Knob) — the highest-leverage
// place to make the hardware-panel identity land.
void EditorLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto lineW = juce::jmin (5.0f, radius * 0.26f);
    const auto arcRadius = radius - lineW * 0.5f;
    const bool enabled = slider.isEnabled();

    // Glass body
    const auto bodyRadius = radius - lineW;
    g.setColour (chassisBg);
    g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour (border.withAlpha (0.7f));
    g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.0f);

    // Track arc (full sweep)
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (panelBg);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc (start -> current position)
    if (sliderPos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
        g.setColour (enabled ? orange : textMuted);
        g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer needle
    juce::Path pointer;
    const auto pointerLength = bodyRadius * 0.68f;
    const auto pointerThickness = 2.0f;
    pointer.addRectangle (-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength);
    pointer.applyTransform (juce::AffineTransform::rotation (toAngle).translated (centre));
    g.setColour (enabled ? textHeader : textMuted);
    g.fillPath (pointer);

    // LED tip
    const auto tip = centre.getPointOnCircumference (bodyRadius - 3.0f, toAngle);
    g.setColour ((enabled ? orange : textMuted).withAlpha (0.9f));
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (tip));
}

//==============================================================================
// Segmented hardware-style tab bar (flat, equal-width, orange underline on the active mode)
// instead of JUCE's default overlapping curved tabs — reads like a mode-select strip on a real
// synth front panel rather than a browser-tab metaphor.
void EditorLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                                        bool isMouseOver, bool isMouseDown)
{
    auto bounds = button.getActiveArea().toFloat();
    const bool active = button.getToggleState();

    g.setColour (active ? panelBg : chassisBg);
    g.fillRect (bounds);

    if (! active && (isMouseOver || isMouseDown))
    {
        g.setColour (base01.withAlpha (0.15f));
        g.fillRect (bounds);
    }

    g.setColour (border.withAlpha (0.4f));
    g.drawRect (bounds, 1.0f);

    if (active)
    {
        auto underline = bounds.removeFromBottom (3.0f);
        g.setColour (orange);
        g.fillRect (underline);
    }

    juce::Font font (juce::FontOptions (13.0f, juce::Font::bold));
    font.setExtraKerningFactor (0.06f);
    g.setFont (font);
    g.setColour (active ? textHeader : textMuted);
    g.drawFittedText (button.getButtonText().trim().toUpperCase(), bounds.toNearestInt(),
                       juce::Justification::centred, 1);
}

void EditorLookAndFeel::drawTabbedButtonBarBackground (juce::TabbedButtonBar& bar, juce::Graphics& g)
{
    g.setColour (chassisBg);
    g.fillRect (bar.getLocalBounds());
    g.setColour (border.withAlpha (0.5f));
    g.drawLine (0.0f, (float) bar.getHeight() - 1.0f, (float) bar.getWidth(), (float) bar.getHeight() - 1.0f, 1.0f);
}

int EditorLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& button, int /*tabDepth*/)
{
    juce::Font font (juce::FontOptions (13.0f, juce::Font::bold));
    const int textWidth = juce::GlyphArrangement::getStringWidthInt (font, button.getButtonText().trim().toUpperCase());
    return juce::jmax (110, textWidth + 44);
}

//==============================================================================
juce::Font EditorLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (15.0f, (float) buttonHeight * 0.55f), juce::Font::bold));
}
