#include "PluginProcessor.h"
#include "ParameterIDs.h"

namespace
{
juce::String ratioToText (float v, int)
{
    // Semitones read better than a bare ratio when the control is logarithmic.
    const auto semis = 12.0f * std::log2 (v);
    return juce::String (v, 3) + "x  (" + juce::String (semis, 2) + " st)";
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout The89thProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // 0.25 to 2.0 is the hardware's span: two octaves down to one up. Skewed so
    // unity sits mid-travel rather than three quarters of the way along.
    auto pitchRange = NormalisableRange<float> (0.25f, 2.0f);
    pitchRange.setSkewForCentre (1.0f);

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::pitch, 1 }, "Pitch",
        pitchRange, 1.0f,
        AudioParameterFloatAttributes{}.withStringFromValueFunction (ratioToText)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::crosspoint1, 1 }, "Crosspoint 1",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::crosspoint2, 1 }, "Crosspoint 2",
        NormalisableRange<float> (0.0f, 1.0f), 1.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::feedback, 1 }, "Feedback",
        NormalisableRange<float> (0.0f, 0.99f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f), 1.0f));

    // Three positions because the front-panel switch has three, even though
    // true stereo cannot reach the widest one: the converter is shared, so each
    // channel only gets half the clock.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::bandwidth, 1 }, "Bandwidth",
        StringArray { "5 kHz", "10 kHz", "20 kHz (mono only)" }, 1));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { pid::freeze, 1 }, "Freeze", false));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { pid::init, 1 }, "Init", false));

    return layout;
}

The89thProcessor::The89thProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "THE89TH", createLayout())
{
    pitch_     = apvts.getRawParameterValue (pid::pitch);
    xp1_       = apvts.getRawParameterValue (pid::crosspoint1);
    xp2_       = apvts.getRawParameterValue (pid::crosspoint2);
    feedback_  = apvts.getRawParameterValue (pid::feedback);
    mix_       = apvts.getRawParameterValue (pid::mix);
    bandwidth_ = apvts.getRawParameterValue (pid::bandwidth);
    freeze_    = apvts.getRawParameterValue (pid::freeze);

    apvts.addParameterListener (pid::init, this);
}

The89thProcessor::~The89thProcessor()
{
    apvts.removeParameterListener (pid::init, this);
    cancelPendingUpdate();
}

void The89thProcessor::parameterChanged (const juce::String& id, float value)
{
    // Called from whichever thread moved the control, possibly the audio one,
    // so do nothing here but hand off.
    if (id == pid::init && value > 0.5f)
        triggerAsyncUpdate();
}

void The89thProcessor::handleAsyncUpdate()
{
    for (auto* p : getParameters())
    {
        auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);
        if (withID == nullptr || withID->paramID == pid::init)
            continue;

        p->setValueNotifyingHost (p->getDefaultValue());
    }

    // Clear the machine as well as the panel: a half-full delay line is state
    // the user cannot see, and leaving it makes Init feel like it half worked.
    // The audio thread owns the engine, so ask rather than reach in.
    resetRequested_.store (true);

    if (auto* initParam = apvts.getParameter (pid::init))
        initParam->setValueNotifyingHost (0.0f);
}

void The89thProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine_.prepare (sampleRate, samplesPerBlock);
    engine_.setParams (readParams());
    engine_.reset();

    // Conversion latency only. The delay the engine imposes is the effect.
    setLatencySamples (static_cast<int> (std::ceil (engine_.latencySamples())));
}

bool The89thProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

the89th::EngineParams The89thProcessor::readParams() const
{
    the89th::ChannelParams c;
    c.pitchRatio  = pitch_    != nullptr ? pitch_->load()    : 1.0f;
    c.crosspoint1 = xp1_      != nullptr ? xp1_->load()      : 0.0f;
    c.crosspoint2 = xp2_      != nullptr ? xp2_->load()      : 1.0f;
    c.feedback    = feedback_ != nullptr ? feedback_->load() : 0.0f;
    c.freeze      = freeze_   != nullptr && freeze_->load() > 0.5f;

    the89th::EngineParams p;
    p.left  = c;
    p.right = c;
    p.mix   = mix_ != nullptr ? mix_->load() : 1.0f;

    const int bw = bandwidth_ != nullptr ? static_cast<int> (bandwidth_->load()) : 1;
    p.bandwidth = bw == 0 ? the89th::Bandwidth::k5kHz
                : bw == 2 ? the89th::Bandwidth::k20kHz
                          : the89th::Bandwidth::k10kHz;

    return p;
}

void The89thProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (resetRequested_.exchange (false))
        engine_.reset();

    engine_.setParams (readParams());
    engine_.process (buffer.getArrayOfWritePointers(),
                     buffer.getNumChannels(),
                     buffer.getNumSamples());
}

juce::AudioProcessorEditor* The89thProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void The89thProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void The89thProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new The89thProcessor();
}
