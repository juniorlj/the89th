#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Version.h"

The89thEditor::The89thEditor (The89thProcessor& p)
    : AudioProcessorEditor (p),
      params_ (p)
{
    stamp_.setText (the89th_version::banner(), juce::dontSendNotification);
    stamp_.setJustificationType (juce::Justification::centred);
    stamp_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
    stamp_.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1a1a1a));
    stamp_.setFont (juce::FontOptions (12.0f).withStyle ("Regular"));
    stamp_.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (stamp_);
    addAndMakeVisible (params_);

    setSize (params_.getWidth(), params_.getHeight() + kStampH);
}

void The89thEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));
}

void The89thEditor::resized()
{
    auto r = getLocalBounds();
    stamp_.setBounds (r.removeFromTop (kStampH));
    params_.setBounds (r);
}
