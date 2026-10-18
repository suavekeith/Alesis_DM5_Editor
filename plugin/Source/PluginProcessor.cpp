#include "PluginProcessor.h"
#include "PluginEditor.h"

DM5AudioProcessor::DM5AudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

DM5AudioProcessor::~DM5AudioProcessor() {}

void DM5AudioProcessor::prepareToPlay (double, int) {}
void DM5AudioProcessor::releaseResources() {}

bool DM5AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;
    return true;
}

void DM5AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // Pass audio through unchanged — plugin is a UI panel, not a DSP effect.
    (void) buffer;
}

juce::AudioProcessorEditor* DM5AudioProcessor::createEditor()
{
    return new DM5AudioProcessorEditor (*this);
}

void DM5AudioProcessor::getStateInformation (juce::MemoryBlock&) {}
void DM5AudioProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DM5AudioProcessor();
}
