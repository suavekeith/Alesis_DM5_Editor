#include "MidiEngine.h"

MidiEngine::MidiEngine() {}

MidiEngine::~MidiEngine()
{
    closePorts();
}

juce::Array<juce::MidiDeviceInfo> MidiEngine::getInputDevices() const
{
    return juce::MidiInput::getAvailableDevices();
}

juce::Array<juce::MidiDeviceInfo> MidiEngine::getOutputDevices() const
{
    return juce::MidiOutput::getAvailableDevices();
}

bool MidiEngine::openPorts (const juce::String& inId, const juce::String& outId)
{
    closePorts();

    auto newOut = juce::MidiOutput::openDevice (outId);
    if (!newOut)
    {
        dispatchLog ("Failed to open MIDI output: " + outId);
        return false;
    }

    auto newIn = juce::MidiInput::openDevice (inId, this);
    if (!newIn)
    {
        dispatchLog ("Failed to open MIDI input: " + inId);
        return false;
    }

    midiOut = std::move (newOut);
    midiIn  = std::move (newIn);
    midiIn->start();
    sysexBuf.clear();

    dispatchLog ("Ports open — in: " + currentInName() + "  out: " + currentOutName());
    return true;
}

void MidiEngine::closePorts()
{
    if (midiIn)  { midiIn->stop(); midiIn.reset(); }
    if (midiOut) { midiOut.reset(); }
    sysexBuf.clear();
}

bool MidiEngine::isOpen() const { return midiIn != nullptr && midiOut != nullptr; }

juce::String MidiEngine::currentInName() const
{
    return midiIn ? midiIn->getName() : juce::String{};
}

juce::String MidiEngine::currentOutName() const
{
    return midiOut ? midiOut->getName() : juce::String{};
}

void MidiEngine::sendKitRequest (int kitIndex)
{
    if (!midiOut)
    {
        dispatchLog ("No MIDI output — select ports first");
        return;
    }
    // Request opcode: 0x40 | (0x20 + kit)
    const uint8_t op = uint8_t (0x40 | (0x20 + kitIndex));
    const uint8_t msg[] = { 0xF0, 0x00, 0x00, 0x0E, 0x13, 0x7F, op, 0xF7 };
    midiOut->sendMessageNow (juce::MidiMessage (msg, (int) sizeof (msg)));
    dispatchLog ("Sent kit " + juce::String (kitIndex) + " request (op=0x"
                 + juce::String::toHexString (op) + ")");
}

void MidiEngine::setLogCallback (LogFn fn)
{
    juce::ScopedLock sl (logLock);
    logFn = std::move (fn);
}

// Called on MIDI thread — copy data and marshal everything to the message thread.
void MidiEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& msg)
{
    const uint8_t* raw = msg.getRawData();
    const int      len = msg.getRawDataSize();

    if (msg.isSysEx())
    {
        // Feed the reassembly buffer (guard against fragmentation)
        for (int i = 0; i < len; ++i)
            sysexBuf.add (raw[i]);

        // A complete SysEx ends with F7
        if (sysexBuf.getLast() == 0xF7)
        {
            // Copy out and clear buffer before dispatching (avoid reentrancy)
            juce::Array<uint8_t> complete;
            complete.swapWith (sysexBuf);

            const int  msgLen   = complete.size();
            juce::String logMsg;

            if (msgLen == 343)
            {
                const uint8_t  op       = complete[6];
                const uint8_t  expected = calcChecksum (complete.data(), msgLen);
                const uint8_t  actual   = complete[341];
                const bool     ok       = (expected == actual);

                logMsg = "SysEx " + juce::String (msgLen) + " bytes  "
                         "op=0x" + juce::String::toHexString (op) + "  "
                         "checksum " + (ok ? "OK" : "FAIL")
                         + " (exp=0x" + juce::String::toHexString (expected)
                         + " got=0x" + juce::String::toHexString (actual) + ")";
            }
            else
            {
                logMsg = "SysEx " + juce::String (msgLen)
                         + " bytes (expected 343 for a kit dump)";
            }

            dispatchLog (logMsg);
        }
    }
    // Non-SysEx messages during an ongoing fragment assembly reset the buffer
    else if (!sysexBuf.isEmpty())
    {
        sysexBuf.clear();
        dispatchLog ("SysEx fragment interrupted by non-SysEx message");
    }
}

// static
uint8_t MidiEngine::calcChecksum (const uint8_t* data, int len)
{
    int sum = 0;
    for (int i = 7; i <= 340 && i < len; ++i)
        sum += data[i];
    return uint8_t (sum & 0x7F);
}

void MidiEngine::dispatchLog (const juce::String& msg)
{
    // Capture logFn by value so this object's lifetime doesn't matter
    LogFn fn;
    {
        juce::ScopedLock sl (logLock);
        fn = logFn;
    }
    if (!fn) return;

    juce::MessageManager::callAsync ([fn, msg] { fn (msg); });
}
