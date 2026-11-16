#include "PluginEditor.h"

namespace
{
constexpr int resizeStripHeight = 16;

MidiEngine::Bytes toBytes (const juce::var& v)
{
    MidiEngine::Bytes bytes;

    if (auto* arr = v.getArray())
    {
        bytes.reserve ((size_t) arr->size());

        for (const auto& b : *arr)
            bytes.push_back ((uint8_t) (int) b);
    }

    return bytes;
}

juce::var portList()
{
    const auto describe = [] (const juce::Array<juce::MidiDeviceInfo>& devices)
    {
        juce::Array<juce::var> list;

        for (const auto& d : devices)
        {
            juce::DynamicObject::Ptr o { new juce::DynamicObject() };
            o->setProperty ("id", d.identifier);
            o->setProperty ("name", d.name);
            list.add (o.get());
        }

        return list;
    };

    juce::DynamicObject::Ptr ports { new juce::DynamicObject() };
    ports->setProperty ("inputs",  describe (juce::MidiInput::getAvailableDevices()));
    ports->setProperty ("outputs", describe (juce::MidiOutput::getAvailableDevices()));
    return ports.get();
}

juce::var nameOrNull (const juce::String& name)
{
    return name.isEmpty() ? juce::var() : juce::var (name);
}

juce::var fileResult (bool success)
{
    juce::DynamicObject::Ptr o { new juce::DynamicObject() };
    o->setProperty ("success", success);
    return o.get();
}

std::optional<juce::WebBrowserComponent::Resource> webResource (const juce::String& url)
{
    const auto path = url == "/" ? juce::String ("index.html") : url.fromLastOccurrenceOf ("/", false, false);

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        if (path != BinaryData::originalFilenames[i])
            continue;

        int size = 0;
        const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);

        const auto mime = path.endsWith (".html")  ? "text/html"
                        : path.endsWith (".js")    ? "text/javascript"
                        : path.endsWith (".css")   ? "text/css"
                        : path.endsWith (".woff2") ? "font/woff2"
                                                   : "application/octet-stream";

        const auto* begin = reinterpret_cast<const std::byte*> (data);
        return juce::WebBrowserComponent::Resource { { begin, begin + size }, mime };
    }

    return std::nullopt;
}
}

DM5AudioProcessorEditor::DM5AudioProcessorEditor (DM5AudioProcessor& p)
    : AudioProcessorEditor (&p),
      dm5Processor (p),
      engine (p.getMidiEngine()),
      browser (makeOptions())
{
    addAndMakeVisible (browser);

    setResizable (true, true);
    setResizeLimits (900, 500, 3840, 2160);

    auto size = dm5Processor.editorSize;

    if (size.isOrigin())
    {
        // Leave room for the host's plugin window header so the whole UI fits on screen.
        size = { 1280, 960 };

        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            size = { juce::jmin (size.x, juce::roundToInt (display->userBounds.getWidth()) - 40),
                     juce::jmin (size.y, juce::roundToInt (display->userBounds.getHeight()) - 140) };
    }

    setSize (size.x, size.y);

    engine.addListener (this);

    deviceListConnection = juce::MidiDeviceListConnection::make ([this]
    {
        browser.emitEventIfBrowserIsVisible ("portsChanged", portList());
    });

    browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
}

DM5AudioProcessorEditor::~DM5AudioProcessorEditor()
{
    engine.removeListener (this);
}

void DM5AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0908));
}

void DM5AudioProcessorEditor::resized()
{
    // The native web view would cover the resize corner, so keep a strip clear for it.
    browser.setBounds (getLocalBounds().withTrimmedBottom (resizeStripHeight));
    dm5Processor.editorSize = { getWidth(), getHeight() };
}

juce::WebBrowserComponent::Options DM5AudioProcessorEditor::makeOptions()
{
    using Args = juce::Array<juce::var>;

    return juce::WebBrowserComponent::Options()
        .withNativeIntegrationEnabled()
        .withResourceProvider ([] (const juce::String& url) { return webResource (url); })
        .withNativeFunction ("listPorts", [] (const Args&, Completion complete)
        {
            complete (portList());
        })
        .withNativeFunction ("openPorts", [this] (const Args& args, Completion complete)
        {
            const auto opened = engine.openPorts (args[0].toString(), args[1].toString());

            juce::DynamicObject::Ptr o { new juce::DynamicObject() };
            o->setProperty ("inName",  nameOrNull (opened.inName));
            o->setProperty ("outName", nameOrNull (opened.outName));
            complete (o.get());
        })
        .withNativeFunction ("send", [this] (const Args& args, Completion complete)
        {
            engine.send (toBytes (args[0]));
            complete (true);
        })
        .withNativeFunction ("sendPaced", [this] (const Args& args, Completion complete)
        {
            std::vector<MidiEngine::Bytes> messages;

            if (auto* arr = args[1].getArray())
                for (const auto& m : *arr)
                    messages.push_back (toBytes (m));

            engine.sendPaced ((int) args[0], std::move (messages), (int) args[2]);
            complete (true);
        })
        .withNativeFunction ("saveFile", [this] (const Args& args, Completion complete)
        {
            saveFile (args, std::move (complete));
        })
        .withNativeFunction ("openFile", [this] (const Args&, Completion complete)
        {
            openFile (std::move (complete));
        })
        .withNativeFunction ("getState", [] (const Args&, Completion complete)
        {
            complete (juce::var());
        })
        .withNativeFunction ("setState", [] (const Args&, Completion complete)
        {
            complete (true);
        });
}

void DM5AudioProcessorEditor::saveFile (const juce::Array<juce::var>& args, Completion complete)
{
    const auto name    = args[0].getProperty ("defaultName", "dm5-kits.json").toString();
    const auto content = args[0].getProperty ("content", "").toString();
    const auto start   = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (name);

    chooser = std::make_unique<juce::FileChooser> ("Export kits", start, "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [content, complete] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              const auto ok = file != juce::File() && file.replaceWithText (content);

                              auto result = fileResult (ok);

                              if (ok)
                                  result.getDynamicObject()->setProperty ("path", file.getFullPathName());

                              complete (result);
                          });
}

void DM5AudioProcessorEditor::openFile (Completion complete)
{
    const auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Import kits", start, "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [complete] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              const auto ok = file.existsAsFile();

                              auto result = fileResult (ok);

                              if (ok)
                                  result.getDynamicObject()->setProperty ("content", file.loadFileAsString());

                              complete (result);
                          });
}

void DM5AudioProcessorEditor::midiReceived (const MidiEngine::Bytes& bytes)
{
    juce::Array<juce::var> data;
    data.ensureStorageAllocated ((int) bytes.size());

    for (auto b : bytes)
        data.add ((int) b);

    browser.emitEventIfBrowserIsVisible ("midi", data);
}

void DM5AudioProcessorEditor::sendProgress (int jobId, int index, int total, bool done)
{
    juce::DynamicObject::Ptr o { new juce::DynamicObject() };
    o->setProperty ("jobId", jobId);
    o->setProperty ("index", index);
    o->setProperty ("total", total);
    o->setProperty ("done", done);
    browser.emitEventIfBrowserIsVisible ("sendProgress", o.get());
}
