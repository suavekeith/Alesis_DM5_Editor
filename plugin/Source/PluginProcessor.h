#pragma once
#include <JuceHeader.h>
#include "MidiEngine.h"

class DM5AudioProcessor : public juce::AudioProcessor,
                          private MidiEngine::Listener,
                          private juce::AsyncUpdater
{
public:
    DM5AudioProcessor();
    ~DM5AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    MidiEngine& getMidiEngine() { return midiEngine; }

    // Message thread only. The UI's view of the saved state; port ids are resolved to current devices.
    juce::var getStateForUi();
    void setStateFromUi (const juce::var& uiState);
    MidiEngine::OpenResult openPorts (const juce::String& inId, const juce::String& outId, bool remember);

    // Messages for the activity log, queued so ones raised while the window is closed aren't lost.
    void log (const juce::String& text, const juce::String& cls);
    juce::var takeLog();

    std::function<void()> onStateRestored;
    std::function<void()> onLogAdded;
    juce::Point<int> editorSize;

    static constexpr int numKits = 21;

    struct State
    {
        std::array<MidiEngine::Bytes, numKits> kits;
        MidiEngine::Bytes trigSetup;
        juce::String inId, inName, outId, outName;
        int midiCh = 9;
        int activeKit = -1;
        juce::String sendOnLoad { "off" };
        juce::var ui;
    };

private:
    void midiReceived (const MidiEngine::Bytes& bytes) override;
    void sendProgress (int jobId, int index, int total, bool done) override;
    void handleAsyncUpdate() override;

    State copyState() const;
    void sendStoredKits();

    MidiEngine midiEngine;

    State state;
    std::vector<std::pair<juce::String, juce::String>> pendingLog;
    juce::CriticalSection stateLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DM5AudioProcessor)
};
