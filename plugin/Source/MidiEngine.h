#pragma once
#include <JuceHeader.h>
#include <functional>

class MidiEngine : public juce::MidiInputCallback
{
public:
    using LogFn = std::function<void(const juce::String&)>;

    MidiEngine();
    ~MidiEngine() override;

    juce::Array<juce::MidiDeviceInfo> getInputDevices()  const;
    juce::Array<juce::MidiDeviceInfo> getOutputDevices() const;

    bool openPorts  (const juce::String& inId, const juce::String& outId);
    void closePorts();
    bool isOpen() const;

    juce::String currentInName()  const;
    juce::String currentOutName() const;

    void sendKitRequest (int kitIndex);

    void setLogCallback (LogFn fn);

    // MidiInputCallback — called on MIDI thread
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

private:
    static uint8_t calcChecksum (const uint8_t* data, int len);
    void dispatchLog (const juce::String& msg);

    std::unique_ptr<juce::MidiInput>  midiIn;
    std::unique_ptr<juce::MidiOutput> midiOut;

    // Guard buffer for fragmented SysEx (JUCE usually delivers complete, but just in case)
    juce::Array<uint8_t> sysexBuf;

    LogFn             logFn;
    juce::CriticalSection logLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiEngine)
};
