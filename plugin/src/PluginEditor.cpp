#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Version.h"

The89thEditor::The89thEditor (The89thProcessor& p)
    : AudioProcessorEditor (p),
      params_ (p)
{
    stamp_.setText (the89th_version::banner(), juce::dontSendNotification);
    stamp_.setJustificationType (juce::Justification::centred);
    stamp_.setColour (juce::Label::textColourId, juce::Colours::white);
    stamp_.setColour (juce::Label::backgroundColourId, juce::Colour (0xff0d0d0d));
    stamp_.setFont (juce::FontOptions (13.0f));
    stamp_.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (stamp_);
    addAndMakeVisible (params_);

    // GenericAudioProcessorEditor may report 0 until laid out; fall back so the
    // host still opens a usable window with the stamp visible.
    const int w = juce::jmax (params_.getWidth(), 400);
    const int h = juce::jmax (params_.getHeight(), 280);
    setSize (w, h + kStampH);
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
