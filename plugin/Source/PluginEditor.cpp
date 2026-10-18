#include "PluginEditor.h"

DM5AudioProcessorEditor::DM5AudioProcessorEditor (DM5AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (1280, 960);
}

DM5AudioProcessorEditor::~DM5AudioProcessorEditor() {}

void DM5AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (20.0f);
    g.drawFittedText ("DM5 Editor — Phase 0 placeholder",
                      getLocalBounds(), juce::Justification::centred, 1);
}

void DM5AudioProcessorEditor::resized() {}
