#pragma once
#include <JuceHeader.h>

class MidiEngine : private juce::MidiInputCallback,
                   private juce::Thread,
                   private juce::AsyncUpdater
{
public:
    using Bytes = std::vector<uint8_t>;

    // All listener callbacks arrive on the message thread.
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void midiReceived (const Bytes& bytes) = 0;
        virtual void sendProgress (int jobId, int index, int total, bool done) = 0;
    };

    struct OpenResult { juce::String inName, outName; };

    MidiEngine();
    ~MidiEngine() override;

    OpenResult openPorts (const juce::String& inId, const juce::String& outId);
    void closePorts();

    void send (const Bytes& bytes);
    void sendPaced (int jobId, std::vector<Bytes> messages, int gapMs);

    void addListener (Listener* l)     { listeners.add (l); }
    void removeListener (Listener* l)  { listeners.remove (l); }

private:
    struct Job   { int id = 0; std::vector<Bytes> messages; int gapMs = 0; };
    struct Event { bool isMidi = false; Bytes bytes; int jobId = 0, index = 0, total = 0; bool done = false; };

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void run() override;
    void handleAsyncUpdate() override;

    void post (Event e);
    bool pause (int ms);

    std::unique_ptr<juce::MidiInput>  midiIn;
    std::unique_ptr<juce::MidiOutput> midiOut;
    juce::CriticalSection outLock;

    std::deque<Job> jobs;
    juce::CriticalSection jobLock;

    std::vector<Event> events;
    juce::CriticalSection eventLock;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiEngine)
};
