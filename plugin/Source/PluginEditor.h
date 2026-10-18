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
    void refreshDeviceLists();
    void openSelectedPorts();
    void appendLog (const juce::String& msg);

    DM5AudioProcessor& audioProcessor;

    juce::Label     inLabel  { {}, "MIDI In:" };
    juce::Label     outLabel { {}, "MIDI Out:" };
    juce::ComboBox  inCombo, outCombo;
    juce::TextButton refreshBtn { "Refresh" };
    juce::TextButton requestBtn { "Request Kit 0" };
    juce::TextEditor logBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DM5AudioProcessorEditor)
};
