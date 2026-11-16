#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using Bytes = MidiEngine::Bytes;
using State = DM5AudioProcessor::State;

constexpr size_t kitDumpSize = 343;
constexpr size_t trigDumpSize = 69;
constexpr uint8_t editBufferOpcode = 0x01;
constexpr uint8_t trigSetupOpcode = 0x05;
constexpr uint8_t firstKitOpcode = 0x20;
constexpr int kitSendGapMs = 400;

bool isDm5Dump (const Bytes& b, size_t expectedSize)
{
    static const uint8_t header[] = { 0xF0, 0x00, 0x00, 0x0E, 0x13 };

    if (b.size() != expectedSize || ! std::equal (std::begin (header), std::end (header), b.begin()) || b.back() != 0xF7)
        return false;

    int sum = 0;

    for (size_t i = 7; i < b.size() - 2; ++i)
        sum += b[i];

    return (sum & 0x7F) == b[b.size() - 2];
}

juce::var bytesToVar (const Bytes& b, bool asBase64)
{
    if (b.empty())
        return {};

    if (asBase64)
        return juce::Base64::toBase64 (b.data(), b.size());

    juce::Array<juce::var> ints;
    ints.ensureStorageAllocated ((int) b.size());

    for (auto v : b)
        ints.add ((int) v);

    return ints;
}

Bytes varToBytes (const juce::var& v)
{
    Bytes bytes;

    if (v.isString())
    {
        juce::MemoryOutputStream decoded;

        if (juce::Base64::convertFromBase64 (decoded, v.toString()))
        {
            const auto* data = static_cast<const uint8_t*> (decoded.getData());
            bytes.assign (data, data + decoded.getDataSize());
        }
    }
    else if (auto* arr = v.getArray())
    {
        for (const auto& b : *arr)
            bytes.push_back ((uint8_t) (int) b);
    }

    return bytes;
}

juce::var stateToVar (const State& s, bool forSave)
{
    juce::Array<juce::var> kits;

    for (const auto& k : s.kits)
        kits.add (bytesToVar (k, forSave));

    juce::DynamicObject::Ptr o { new juce::DynamicObject() };
    o->setProperty ("version", 1);
    o->setProperty ("kits", kits);
    o->setProperty ("trigSetup", bytesToVar (s.trigSetup, forSave));
    o->setProperty ("midiCh", s.midiCh);
    o->setProperty ("activeKit", s.activeKit);
    o->setProperty ("sendOnLoad", s.sendOnLoad);
    o->setProperty ("ui", s.ui);
    o->setProperty ("inId", s.inId);
    o->setProperty ("inName", s.inName);
    o->setProperty ("outId", s.outId);
    o->setProperty ("outName", s.outName);
    return o.get();
}

void applyVar (State& s, const juce::var& v, bool includePorts)
{
    const auto* kits = v["kits"].getArray();

    for (int i = 0; i < DM5AudioProcessor::numKits; ++i)
    {
        auto kit = kits != nullptr && i < kits->size() ? varToBytes (kits->getReference (i)) : Bytes{};
        s.kits[(size_t) i] = isDm5Dump (kit, kitDumpSize) ? std::move (kit) : Bytes{};
    }

    auto trig = varToBytes (v["trigSetup"]);
    s.trigSetup = isDm5Dump (trig, trigDumpSize) ? std::move (trig) : Bytes{};

    s.midiCh     = juce::jlimit (0, 15, (int) v.getProperty ("midiCh", 9));
    s.activeKit  = juce::jlimit (-1, DM5AudioProcessor::numKits - 1, (int) v.getProperty ("activeKit", -1));
    s.sendOnLoad = v.getProperty ("sendOnLoad", "off").toString();
    s.ui         = v["ui"];

    if (includePorts)
    {
        s.inId    = v["inId"].toString();
        s.inName  = v["inName"].toString();
        s.outId   = v["outId"].toString();
        s.outName = v["outName"].toString();
    }
}

// Device identifiers can change between sessions, so fall back to matching by name.
juce::String resolveDevice (const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& id, const juce::String& name)
{
    if (id.isEmpty() && name.isEmpty())
        return {};

    for (const auto& d : devices)
        if (d.identifier == id)
            return d.identifier;

    for (const auto& d : devices)
        if (name.isNotEmpty() && d.name == name)
            return d.identifier;

    return {};
}
}

DM5AudioProcessor::DM5AudioProcessor()
    : AudioProcessor (BusesProperties())
{
    midiEngine.addListener (this);
}

DM5AudioProcessor::~DM5AudioProcessor()
{
    cancelPendingUpdate();
    midiEngine.removeListener (this);
}

void DM5AudioProcessor::prepareToPlay (double, int) {}
void DM5AudioProcessor::releaseResources() {}

void DM5AudioProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    // The track's MIDI passes through untouched; the plugin is only an editor panel.
}

juce::AudioProcessorEditor* DM5AudioProcessor::createEditor()
{
    return new DM5AudioProcessorEditor (*this);
}

DM5AudioProcessor::State DM5AudioProcessor::copyState() const
{
    const juce::ScopedLock sl (stateLock);
    return state;
}

void DM5AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto v = stateToVar (copyState(), true);
    v.getDynamicObject()->setProperty ("editorW", editorSize.x);
    v.getDynamicObject()->setProperty ("editorH", editorSize.y);

    const auto json = juce::JSON::toString (v, true);
    destData.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void DM5AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto v = juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (data), sizeInBytes));

    if (v.getDynamicObject() == nullptr)
        return;

    State restored;
    applyVar (restored, v, true);

    {
        const juce::ScopedLock sl (stateLock);
        state = std::move (restored);
    }

    editorSize = { (int) v.getProperty ("editorW", 0), (int) v.getProperty ("editorH", 0) };

    // Port access and UI updates belong on the message thread.
    triggerAsyncUpdate();
}

void DM5AudioProcessor::handleAsyncUpdate()
{
    sendStoredKits();

    if (onStateRestored)
        onStateRestored();
}

void DM5AudioProcessor::sendStoredKits()
{
    const auto s = copyState();
    const auto all = s.sendOnLoad == "all";

    if (! all && s.sendOnLoad != "active")
        return;

    const auto outId = resolveDevice (juce::MidiOutput::getAvailableDevices(), s.outId, s.outName);

    if (outId.isEmpty())
        return;

    const auto inId = resolveDevice (juce::MidiInput::getAvailableDevices(), s.inId, s.inName);
    midiEngine.openPorts (inId, outId);

    std::vector<Bytes> messages;
    const Bytes* active = s.activeKit >= 0 && ! s.kits[(size_t) s.activeKit].empty() ? &s.kits[(size_t) s.activeKit] : nullptr;

    if (all)
    {
        for (const auto& k : s.kits)
            if (! k.empty())
                messages.push_back (k);
    }
    else if (active != nullptr)
    {
        messages.push_back (*active);
    }

    if (messages.empty())
        return;

    if (active != nullptr)
    {
        // Slot dumps don't touch the edit buffer, which is what the DM5 plays.
        auto editBuffer = *active;
        editBuffer[6] = editBufferOpcode;
        messages.push_back (std::move (editBuffer));
    }

    if (s.activeKit >= 0)
        messages.push_back ({ (uint8_t) (0xC0 | s.midiCh), (uint8_t) s.activeKit });

    // Dumps captured from the module carry its MIDI channel; 7F addresses it whatever channel it is on now.
    for (auto& m : messages)
        if (m.size() > 5 && m[0] == 0xF0)
            m[5] = 0x7F;

    midiEngine.sendPaced (-1, std::move (messages), kitSendGapMs);
}

juce::var DM5AudioProcessor::getStateForUi()
{
    const auto s = copyState();
    auto v = stateToVar (s, false);
    v.getDynamicObject()->setProperty ("inId",  resolveDevice (juce::MidiInput::getAvailableDevices(),  s.inId,  s.inName));
    v.getDynamicObject()->setProperty ("outId", resolveDevice (juce::MidiOutput::getAvailableDevices(), s.outId, s.outName));
    return v;
}

void DM5AudioProcessor::setStateFromUi (const juce::var& uiState)
{
    if (uiState.getDynamicObject() == nullptr)
        return;

    const juce::ScopedLock sl (stateLock);
    applyVar (state, uiState, false);
}

MidiEngine::OpenResult DM5AudioProcessor::openPorts (const juce::String& inId, const juce::String& outId, bool remember)
{
    const auto opened = midiEngine.openPorts (inId, outId);

    if (remember)
    {
        const juce::ScopedLock sl (stateLock);
        state.inId    = inId;
        state.inName  = opened.inName;
        state.outId   = outId;
        state.outName = opened.outName;
    }

    return opened;
}

// Keeps dumps that arrive while the plugin window is closed.
void DM5AudioProcessor::midiReceived (const Bytes& bytes)
{
    if (bytes.size() < 7)
        return;

    const auto opcode = bytes[6];
    const juce::ScopedLock sl (stateLock);

    if (opcode >= firstKitOpcode && opcode < firstKitOpcode + numKits && isDm5Dump (bytes, kitDumpSize))
        state.kits[(size_t) (opcode - firstKitOpcode)] = bytes;
    else if (opcode == trigSetupOpcode && isDm5Dump (bytes, trigDumpSize))
        state.trigSetup = bytes;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DM5AudioProcessor();
}
