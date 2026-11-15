#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class DM5AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private MidiEngine::Listener
{
public:
    explicit DM5AudioProcessorEditor (DM5AudioProcessor&);
    ~DM5AudioProcessorEditor() override;

    void resized() override;

private:
    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;

    void midiReceived (const MidiEngine::Bytes& bytes) override;
    void sendProgress (int jobId, int index, int total, bool done) override;

    juce::WebBrowserComponent::Options makeOptions();
    void saveFile (const juce::Array<juce::var>& args, Completion complete);
    void openFile (Completion complete);

    MidiEngine& engine;
    juce::WebBrowserComponent browser;
    juce::MidiDeviceListConnection deviceListConnection;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DM5AudioProcessorEditor)
};
