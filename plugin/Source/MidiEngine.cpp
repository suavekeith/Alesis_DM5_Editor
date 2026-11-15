#include "MidiEngine.h"

MidiEngine::MidiEngine() : juce::Thread ("DM5 paced send") {}

MidiEngine::~MidiEngine()
{
    stopThread (2000);
    closePorts();
    cancelPendingUpdate();
}

MidiEngine::OpenResult MidiEngine::openPorts (const juce::String& inId, const juce::String& outId)
{
    closePorts();

    OpenResult result;

    if (outId.isNotEmpty())
    {
        if (auto out = juce::MidiOutput::openDevice (outId))
        {
            result.outName = out->getName();
            const juce::ScopedLock sl (outLock);
            midiOut = std::move (out);
        }
    }

    if (inId.isNotEmpty())
    {
        if (auto in = juce::MidiInput::openDevice (inId, this))
        {
            result.inName = in->getName();
            midiIn = std::move (in);
            midiIn->start();
        }
    }

    return result;
}

void MidiEngine::closePorts()
{
    if (midiIn != nullptr)
    {
        midiIn->stop();
        midiIn.reset();
    }

    const juce::ScopedLock sl (outLock);
    midiOut.reset();
}

void MidiEngine::send (const Bytes& bytes)
{
    if (bytes.empty())
        return;

    const juce::ScopedLock sl (outLock);

    if (midiOut != nullptr)
        midiOut->sendMessageNow (juce::MidiMessage (bytes.data(), (int) bytes.size()));
}

void MidiEngine::sendPaced (int jobId, std::vector<Bytes> messages, int gapMs)
{
    {
        const juce::ScopedLock sl (jobLock);
        jobs.push_back ({ jobId, std::move (messages), gapMs });
    }

    if (! isThreadRunning())
        startThread();

    notify();
}

// Waits for the full gap even if notify() wakes the thread early; false means the thread should exit.
bool MidiEngine::pause (int ms)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;

    while (! threadShouldExit())
    {
        const auto now = juce::Time::getMillisecondCounter();

        if (now >= end)
            return true;

        wait ((int) (end - now));
    }

    return false;
}

void MidiEngine::run()
{
    while (! threadShouldExit())
    {
        Job job;
        bool haveJob = false;

        {
            const juce::ScopedLock sl (jobLock);

            if (! jobs.empty())
            {
                job = std::move (jobs.front());
                jobs.pop_front();
                haveJob = true;
            }
        }

        if (! haveJob)
        {
            wait (-1);
            continue;
        }

        const auto total = (int) job.messages.size();

        for (int i = 0; i < total; ++i)
        {
            send (job.messages[(size_t) i]);

            Event progress;
            progress.jobId = job.id;
            progress.index = i;
            progress.total = total;
            post (std::move (progress));

            if (i < total - 1 && ! pause (job.gapMs))
                return;
        }

        Event finished;
        finished.jobId = job.id;
        finished.total = total;
        finished.done = true;
        post (std::move (finished));
    }
}

void MidiEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& msg)
{
    if (! msg.isSysEx())
        return;

    Event e;
    e.isMidi = true;
    e.bytes.assign (msg.getRawData(), msg.getRawData() + msg.getRawDataSize());
    post (std::move (e));
}

void MidiEngine::post (Event e)
{
    {
        const juce::ScopedLock sl (eventLock);
        events.push_back (std::move (e));
    }

    triggerAsyncUpdate();
}

void MidiEngine::handleAsyncUpdate()
{
    std::vector<Event> pending;

    {
        const juce::ScopedLock sl (eventLock);
        pending.swap (events);
    }

    for (const auto& e : pending)
    {
        if (e.isMidi)
            listeners.call ([&] (Listener& l) { l.midiReceived (e.bytes); });
        else
            listeners.call ([&] (Listener& l) { l.sendProgress (e.jobId, e.index, e.total, e.done); });
    }
}
