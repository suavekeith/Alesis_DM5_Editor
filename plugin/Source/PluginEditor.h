#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class DM5AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DM5AudioProcessorEditor (DM5AudioProcessor&);
    ~DM5AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DM5AudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DM5AudioProcessorEditor)
};
