#pragma once
#include "PluginProcessor.h"

// Interface mínima: um knob.
class T1Editor : public juce::AudioProcessorEditor
{
public:
    explicit T1Editor (T1AudioProcessor& p)
        : juce::AudioProcessorEditor (&p),
          attachment (p.apvts, T1AudioProcessor::kSaturationID, knob)
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
        knob.setNumDecimalPlacesToDisplay (1);
        knob.setDoubleClickReturnValue (true, 3.0);
        addAndMakeVisible (knob);

        title.setText ("SATURATION", juce::dontSendNotification);
        title.setJustificationType (juce::Justification::centred);
        title.setColour (juce::Label::textColourId, juce::Colour (0xffd9d2c0));
        addAndMakeVisible (title);

        setSize (170, 190);
    }

    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff22201c)); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        title.setBounds (area.removeFromTop (22));
        knob.setBounds (area);
    }

private:
    juce::Slider knob;
    juce::Label title;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};
