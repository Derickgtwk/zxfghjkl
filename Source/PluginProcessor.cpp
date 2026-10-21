#include "PluginProcessor.h"
#include "PluginEditor.h"

T1AudioProcessor::T1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    saturation = apvts.getRawParameterValue (kSaturationID);
}

juce::AudioProcessorValueTreeState::ParameterLayout T1AudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kSaturationID, 1 },
        "Saturation",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        3.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 1); })));
    return layout;
}

bool T1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void T1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = juce::jmax (1, samplesPerBlock);
    oversampling.initProcessing ((size_t) maxBlock);
    oversampling.reset();
    setLatencySamples ((int) std::lround (oversampling.getLatencyInSamples()));

    const double osRate = sampleRate * (double) oversampling.getOversamplingFactor();

    // Tap de 250 Ω / 1:17 (entrada de microfone). Para o tap de 30 Ω / 1:43,
    // troque por t1::tap30().
    for (auto& c : cores)
        c.prepare (osRate, t1::tap250());

    driveSmoother.reset (osRate, 0.03);
    driveSmoother.setCurrentAndTargetValue (t1::knobToDrive ((double) saturation->load()));
}

void T1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    if (numCh == 0)
        return;

    driveSmoother.setTargetValue (t1::knobToDrive ((double) saturation->load()));

    juce::dsp::AudioBlock<float> full (buffer);
    auto channels = full.getSubsetChannelBlock (0, (size_t) numCh);

    for (size_t start = 0; start < channels.getNumSamples(); start += (size_t) maxBlock)
    {
        const size_t len = juce::jmin ((size_t) maxBlock, channels.getNumSamples() - start);
        auto chunk = channels.getSubBlock (start, len);

        auto up = oversampling.processSamplesUp (chunk);
        const size_t n = up.getNumSamples();

        float* l = up.getChannelPointer (0);
        float* r = numCh > 1 ? up.getChannelPointer (1) : nullptr;

        for (size_t i = 0; i < n; ++i)
        {
            const double drive = driveSmoother.getNextValue();
            l[i] = (float) cores[0].process ((double) l[i], drive);
            if (r != nullptr)
                r[i] = (float) cores[1].process ((double) r[i], drive);
        }

        oversampling.processSamplesDown (chunk);
    }
}

juce::AudioProcessorEditor* T1AudioProcessor::createEditor()
{
    return new T1Editor (*this);
}

void T1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void T1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new T1AudioProcessor();
}
