#include "ParamControl.h"

using casioxw::ControlKind;

namespace
{
    constexpr int kLabelWidth = 170;
    constexpr int kControlHeight = 24;

    // Chunk 7f: both compact render modes share ONE cell width now — owner feedback that the
    // knob row and the vertical-fader row (different widths, 88 vs 56, before this chunk) looked
    // like two mismatched grids stacked on top of each other. Must match kCompactCellWidth in
    // SoloSynthPanel.cpp ("ParamControl owns its own size, the grid just tiles it" pattern).
    constexpr int kCompactCellWidth = 100;

    // Label height bumped (16/20 -> 34) so a 2-line wrap at a readable font size replaces the old
    // aggressive single-line shrink-to-fit (owner: "the labels are too dang small"). See the
    // comment below on why minimumHorizontalScale was dropped in favour of natural wrapping.
    constexpr int kCompactLabelHeight = 34;
    constexpr int kCompactTextBoxHeight = 16;   // shared by both compact modes' value text box

    // Knob: labelHeight + rotary-and-textbox region. Fader: labelHeight + the vertical throw +
    // textbox. Both taller than their Chunk 7e predecessors (92/150) by exactly the label-height
    // increase, so the rotary/throw area itself is unchanged (label growth is scoped to just the
    // label). Must match kKnobCellHeight/kFaderCellHeight in SoloSynthPanel.cpp.
    constexpr int kKnobHeight = kCompactLabelHeight + 76;
    constexpr int kFaderHeight = kCompactLabelHeight + 130;

    juce::String displayName (const casioxw::ParamInfo& info)
    {
        juce::String s = info.name.isNotEmpty() ? info.name : info.id;
        // "note" is a DISPLAY FORMATTER SELECTOR (-> Slider::textFromValueFunction below), not a
        // unit of measure — appending "(note)" to every Key Follow Base label would be noise.
        if (info.unit.isNotEmpty() && info.unit != "note")
            s << " (" << info.unit << ")";
        return s;
    }

    // juce::Slider's stock LinearVertical always maps min->bottom/max->top (the mixing-console
    // "up is more" convention). The Drawbar Organ's Position fader needs the opposite (owner
    // feedback: physical drawbars are pulled DOWN/out for more volume) — juce::Slider exposes
    // exactly this customization point as two overridable virtuals; overriding them flips only
    // the visual thumb position, value semantics (getValue()/setValue()/onValueChange) are
    // untouched, so nothing else in ParamControl needs to know the difference.
    class InvertedVerticalSlider : public juce::Slider
    {
    public:
        using juce::Slider::Slider;

        double valueToProportionOfLength (double value) override
        {
            return 1.0 - juce::Slider::valueToProportionOfLength (value);
        }

        double proportionOfLengthToValue (double proportion) override
        {
            return juce::Slider::proportionOfLengthToValue (1.0 - proportion);
        }

        // Stock LookAndFeel_V4::drawLinearSlider always fills the coloured "value track" from
        // the BOTTOM up to the thumb — correct when the bottom end is the slider's own minimum
        // (true for every other fader in this app), but backwards here: this slider puts the
        // MAXIMUM at the bottom, so the stock fill made the loudest setting look nearly bare/grey
        // (thumb sitting right at the fill's own bottom-anchored start) and the quietest setting
        // look nearly fully coloured (owner: "when it is at 0 the slider should be grey, and when
        // it is at 8/100% it should be blue -- opposite how it is now"). Overriding paint() keeps
        // every other visual (background track, thumb, colours) identical to the stock
        // LookAndFeel_V4 rendering (mirrored below) — only the fill's anchor moves from bottom to
        // top, so the coloured portion now grows as the drawbar gets louder instead of as the
        // thumb gets closer to the bottom pixel.
        void paint (juce::Graphics& g) override
        {
            const auto trackBounds = getLookAndFeel().getSliderLayout (*this).sliderBounds.toFloat();
            const float trackWidth = juce::jmin (6.0f, trackBounds.getWidth() * 0.25f);
            const juce::Point<float> top    (trackBounds.getCentreX(), trackBounds.getY());
            const juce::Point<float> bottom (trackBounds.getCentreX(), trackBounds.getBottom());
            const juce::Point<float> thumb  (trackBounds.getCentreX(), getPositionOfValue (getValue()));

            juce::Path background;
            background.startNewSubPath (bottom);
            background.lineTo (top);
            g.setColour (findColour (juce::Slider::backgroundColourId));
            g.strokePath (background, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

            juce::Path valueTrack;
            valueTrack.startNewSubPath (top);   // anchor at the TOP (the quiet/0 end), not bottom
            valueTrack.lineTo (thumb);
            g.setColour (findColour (juce::Slider::trackColourId));
            g.strokePath (valueTrack, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

            const float thumbWidth = getLookAndFeel().getSliderThumbRadius (*this);
            g.setColour (findColour (juce::Slider::thumbColourId));
            g.fillEllipse (juce::Rectangle<float> (thumbWidth, thumbWidth).withCentre (thumb));
        }
    };
}

ParamControl::ParamControl (const casioxw::ParamModel& model, const casioxw::ParamInfo& infoIn, int instanceIn,
                             RenderMode modeIn, juce::String labelOverride, bool invertVerticalFader)
    : info (infoIn), instance (instanceIn), kind (casioxw::decideControlKind (info, instance)),
      mode (kind == ControlKind::Slider ? modeIn : RenderMode::Default)
{
    const bool knobMode = mode == RenderMode::Knob;
    const bool faderMode = mode == RenderMode::VerticalFader;
    const bool compact = knobMode || faderMode;

    setSize (compact ? kCompactCellWidth : kLabelWidth + 220,
             knobMode ? kKnobHeight : faderMode ? kFaderHeight : kControlHeight);

    const juce::String label = labelOverride.isNotEmpty() ? labelOverride : displayName (info);
    nameLabel.setText (label, juce::dontSendNotification);
    nameLabel.setJustificationType (compact ? juce::Justification::centred
                                             : juce::Justification::centredLeft);
    if (compact)
    {
        // Chunk 7f: the old approach (aggressive setMinimumHorizontalScale down to 0.6, keeping
        // everything on one line) is exactly what made these labels "too dang small" -- squeezing
        // "Pitch Env Attack Time" onto one 100px line needs a tiny font. juce::Label's normal
        // paint path (Graphics::drawFittedText under the hood) already WRAPS onto multiple lines
        // on its own once the box is tall enough, at full font size, only shrinking as a last
        // resort -- so simply giving it two lines of height (kCompactLabelHeight) at a normal
        // font and NOT setting a minimum scale gets natural, readable wrapping instead. Tooltip
        // stays as a fallback for the rare name that's long even wrapped.
        nameLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
        nameLabel.setTooltip (label);
    }
    addAndMakeVisible (nameLabel);

    switch (kind)
    {
        case ControlKind::Toggle:
        {
            toggle = std::make_unique<juce::ToggleButton>();
            toggle->onClick = [this] { notify (toggle->getToggleState() ? 1 : 0); };
            addAndMakeVisible (*toggle);
            break;
        }

        case ControlKind::ComboEnum:
        case ControlKind::ComboEnumPerOsc:
        {
            const juce::String enumName = casioxw::resolveEnumName (info, instance);
            const auto* entries = model.enumValues (enumName);

            // Wave-number params (vt=="wf") back their combo with the full wave library --
            // hundreds of entries (Hex Layer's Wave, 789) into the low thousands (Solo Synth's
            // PCM wave picker, 2158) -- where juce::ComboBox's PopupMenu-backed dropdown freezes
            // the message thread on open (see WavePicker.h's doc comment / .wolf/buglog.json).
            // Every other enum in this app tops out around 19 entries, where plain ComboBox is
            // genuinely fine -- so the switch is keyed on vt=="wf" (the one class of param whose
            // enum can legitimately be this large), not a size threshold.
            if (info.vt == "wf")
            {
                wavePicker = std::make_unique<WavePicker>();
                wavePicker->setEntries (entries);
                wavePicker->onValueChanged = [this] (int value) { notify (value); };
                addAndMakeVisible (*wavePicker);
            }
            else
            {
                combo = std::make_unique<juce::ComboBox>();
                if (entries != nullptr)
                    for (const auto& e : *entries)
                        combo->addItem (e.label, e.value + 1);   // JUCE ids are 1-based; 0 illegal
                combo->onChange = [this]
                {
                    const int id = combo->getSelectedId();
                    if (id > 0)
                        notify (id - 1);
                };
                addAndMakeVisible (*combo);
            }
            break;
        }

        case ControlKind::ComboRange:
        {
            combo = std::make_unique<juce::ComboBox>();
            for (int v = info.range.min; v <= info.range.max; ++v)
                combo->addItem (juce::String (v), (v - info.range.min) + 1);
            combo->onChange = [this]
            {
                const int id = combo->getSelectedId();
                if (id > 0)
                    notify ((id - 1) + info.range.min);
            };
            addAndMakeVisible (*combo);
            break;
        }

        case ControlKind::Slider:
        {
            // Chunk 7e item 3: the 9 envelope-stage params (SoloSynthPanel's per-param
            // envelopeStageIds() check) render as a compact VERTICAL fader instead of the
            // original full-width bar, laid out side-by-side in a row by the owning panel — the
            // actual space-saving move (9 vertical faders in a row vs. 9 full-width bars
            // stacked). Every other Slider-kind param still renders as a compact rotary knob
            // (Chunk 7d item 2, unchanged); anything left over stays the default full-width bar.
            const auto sliderStyle = knobMode  ? juce::Slider::RotaryHorizontalVerticalDrag
                                    : faderMode ? juce::Slider::LinearVertical
                                                : juce::Slider::LinearHorizontal;
            const auto textBoxPos = compact ? juce::Slider::TextBoxBelow : juce::Slider::TextBoxRight;
            if (faderMode && invertVerticalFader)
                slider = std::make_unique<InvertedVerticalSlider> (sliderStyle, textBoxPos);
            else
                slider = std::make_unique<juce::Slider> (sliderStyle, textBoxPos);
            slider->setRange ((double) info.range.min, (double) info.range.max, 1.0);
            slider->onValueChange = [this] { notify ((int) slider->getValue()); };
            // Double-click resets to the last value pushed via setValueFromSync() (a hardware
            // sync reply, or the JSON default seeded just below) -- range.min is only the
            // fallback for the (currently nonexistent, per params/xwp1.json) case where a param
            // has no JSON default and no sync has happened yet. Kept in sync with reality by
            // setValueFromSync() re-arming this on every call, not just here at construction.
            slider->setDoubleClickReturnValue (true, (double) info.range.min);
            if (compact)
                slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                                         kCompactCellWidth - 8, kCompactTextBoxHeight);

            // Key Follow Base params (unit=="note", metadata-driven per gen_xwp1.py's OVR table,
            // not a hardcoded param-id list): show/accept MIDI note names ("C-1".."G9") instead
            // of raw integers, via casioxw::midiNoteName()/noteNameToMidi() (Chunk 7c item 3).
            if (info.unit == "note")
            {
                slider->textFromValueFunction = [] (double v) { return casioxw::midiNoteName ((int) v); };
                slider->valueFromTextFunction = [] (const juce::String& t) -> double
                {
                    const auto n = casioxw::noteNameToMidi (t);
                    return n.has_value() ? (double) *n : 0.0;
                };
                // NOT slider->setValue(slider->getValue(), ...) -- Slider::setValue() no-ops when
                // the new value equals the current one (no repaint), so that "refresh" silently
                // did nothing whenever the default being displayed was already correct (e.g. 0).
                // updateText() is JUCE's purpose-built call for "re-render the text box now".
                slider->updateText();
            }

            addAndMakeVisible (*slider);
            break;
        }

        case ControlKind::Disabled:
        {
            nameLabel.setText (displayName (info) + "  (n/a for this instance)", juce::dontSendNotification);
            nameLabel.setEnabled (false);
            break;
        }
    }

    if (info.defaultValue.has_value() && kind != ControlKind::Disabled)
        setValueFromSync (*info.defaultValue);

    // setSize() above ran before toggle/combo/slider existed (they're constructed lazily inside
    // the switch, since a ParamControl is exactly ONE of several mutually-exclusive widget types
    // -- unlike e.g. a fixed pair of always-present knobs). That first resized() pass therefore
    // skipped positioning the widget entirely (resized()'s null guard). Whether anything ever
    // repositions it again depends on the CALLER's later setBounds() using a genuinely different
    // size -- true for every list-style control (SoloSynthPanel assigns the real container width,
    // which differs from this constructor's default and so DOES re-trigger resized()), but FALSE
    // for compact-mode controls (Knob AND VerticalFader), where the owning grid/row layout reuses
    // this exact same fixed cell size, so JUCE's setBounds() sees no size change and never calls
    // resized() again -- leaving the widget stuck at its default zero bounds forever (confirmed
    // via tools/gui-preview: a rendered knob produced zero non-text pixels before this fix; same
    // check re-run against the new vertical-fader mode below). Same failure class as
    // bug-009/MainWindow's fix; force one final, correct layout pass now that the widget is
    // guaranteed to exist, regardless of what the caller does with setBounds() afterward.
    resized();
}

void ParamControl::notify (int uiValue)
{
    if (onValueChanged)
        onValueChanged (uiValue);
}

void ParamControl::setValueFromSync (int value)
{
    switch (kind)
    {
        case ControlKind::Toggle:
            if (toggle != nullptr)
                toggle->setToggleState (value != 0, juce::dontSendNotification);
            break;

        case ControlKind::ComboEnum:
        case ControlKind::ComboEnumPerOsc:
            // Clamp before display: a synced value can legitimately fall outside the UI's known
            // range (e.g. tssOSCwf decoding to -1 when the real hardware's wire value is 0 --
            // verified against franky's own SX2v.wf formula and reproducible across repeated
            // reads, not a decode bug -- the Lua's own encoder comment notes waveform wire values
            // "start at 1", so 0 is simply outside the normal domain for this tone's current
            // state). Without this, setSelectedId() on an out-of-range id silently selects
            // nothing (JUCE combo id 0 = no selection), leaving the combo looking broken/blank
            // with no indication why. Clamping to the nearest real entry keeps the display honest
            // (shows a plausible value) rather than mysterious. WavePicker applies the same clamp
            // internally (setSelectedValue).
            if (wavePicker != nullptr)
                wavePicker->setSelectedValue (value, juce::dontSendNotification);
            else if (combo != nullptr && combo->getNumItems() > 0)
                combo->setSelectedId (juce::jlimit (0, combo->getNumItems() - 1, value) + 1,
                                      juce::dontSendNotification);
            break;

        case ControlKind::ComboRange:
            if (combo != nullptr)
            {
                const int clamped = juce::jlimit (info.range.min, info.range.max, value);
                combo->setSelectedId ((clamped - info.range.min) + 1, juce::dontSendNotification);
            }
            break;

        case ControlKind::Slider:
            // juce::Slider clamps to its own setRange() bounds internally, so this is defensive
            // consistency with the combo cases above rather than a required fix on its own.
            if (slider != nullptr)
            {
                const double clamped = juce::jlimit ((double) info.range.min, (double) info.range.max,
                                                      (double) value);
                slider->setValue (clamped, juce::dontSendNotification);
                // Re-arm the double-click reset target on every sync push (construction-time
                // default AND every later hardware read-back) -- see the constructor's comment.
                slider->setDoubleClickReturnValue (true, clamped);
            }
            break;

        case ControlKind::Disabled:
            break;   // nothing to display
    }
}

void ParamControl::resized()
{
    auto bounds = getLocalBounds();

    if (mode == RenderMode::Knob)
    {
        nameLabel.setBounds (bounds.removeFromTop (kCompactLabelHeight));
        if (slider != nullptr)
            slider->setBounds (bounds);
        return;
    }

    if (mode == RenderMode::VerticalFader)
    {
        nameLabel.setBounds (bounds.removeFromTop (kCompactLabelHeight));
        if (slider != nullptr)
            slider->setBounds (bounds);
        return;
    }

    nameLabel.setBounds (bounds.removeFromLeft (kLabelWidth));

    if (toggle != nullptr)
        toggle->setBounds (bounds.removeFromLeft (60));
    else if (combo != nullptr)
        combo->setBounds (bounds);
    else if (wavePicker != nullptr)
        wavePicker->setBounds (bounds);
    else if (slider != nullptr)
        slider->setBounds (bounds);
}
