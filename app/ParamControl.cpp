#include "ParamControl.h"

using casioxw::ControlKind;

namespace
{
    constexpr int kLabelWidth = 170;
    constexpr int kControlHeight = 24;

    juce::String displayName (const casioxw::ParamInfo& info)
    {
        juce::String s = info.name.isNotEmpty() ? info.name : info.id;
        if (info.unit.isNotEmpty())
            s << " (" << info.unit << ")";
        return s;
    }
}

ParamControl::ParamControl (const casioxw::ParamModel& model, const casioxw::ParamInfo& infoIn, int instanceIn)
    : info (infoIn), instance (instanceIn), kind (casioxw::decideControlKind (info, instance))
{
    setSize (kLabelWidth + 220, kControlHeight);

    nameLabel.setText (displayName (info), juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centredLeft);
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
            combo = std::make_unique<juce::ComboBox>();
            const juce::String enumName = casioxw::resolveEnumName (info, instance);
            if (const auto* entries = model.enumValues (enumName))
                for (const auto& e : *entries)
                    combo->addItem (e.label, e.value + 1);   // JUCE ids are 1-based; 0 is illegal
            combo->onChange = [this]
            {
                const int id = combo->getSelectedId();
                if (id > 0)
                    notify (id - 1);
            };
            addAndMakeVisible (*combo);
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
            slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            slider->setRange ((double) info.range.min, (double) info.range.max, 1.0);
            slider->onValueChange = [this] { notify ((int) slider->getValue()); };
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
            if (combo != nullptr)
                combo->setSelectedId (value + 1, juce::dontSendNotification);
            break;

        case ControlKind::ComboRange:
            if (combo != nullptr)
                combo->setSelectedId ((value - info.range.min) + 1, juce::dontSendNotification);
            break;

        case ControlKind::Slider:
            if (slider != nullptr)
                slider->setValue ((double) value, juce::dontSendNotification);
            break;

        case ControlKind::Disabled:
            break;   // nothing to display
    }
}

void ParamControl::resized()
{
    auto bounds = getLocalBounds();
    nameLabel.setBounds (bounds.removeFromLeft (kLabelWidth));

    if (toggle != nullptr)
        toggle->setBounds (bounds.removeFromLeft (60));
    else if (combo != nullptr)
        combo->setBounds (bounds);
    else if (slider != nullptr)
        slider->setBounds (bounds);
}
