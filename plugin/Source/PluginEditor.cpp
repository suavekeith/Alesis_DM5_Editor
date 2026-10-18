#include "PluginEditor.h"

DM5AudioProcessorEditor::DM5AudioProcessorEditor (DM5AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (640, 420);
    setResizable (true, true);

    addAndMakeVisible (inLabel);
    addAndMakeVisible (outLabel);
    addAndMakeVisible (inCombo);
    addAndMakeVisible (outCombo);
    addAndMakeVisible (refreshBtn);
    addAndMakeVisible (requestBtn);
    addAndMakeVisible (logBox);

    logBox.setMultiLine (true);
    logBox.setReadOnly (true);
    logBox.setScrollbarsShown (true);
    logBox.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, 0));

    refreshBtn.onClick = [this] { refreshDeviceLists(); };

    inCombo.onChange  = [this] { openSelectedPorts(); };
    outCombo.onChange = [this] { openSelectedPorts(); };

    requestBtn.onClick = [this] {
        audioProcessor.getMidiEngine().sendKitRequest (0);
    };

    audioProcessor.getMidiEngine().setLogCallback ([this] (const juce::String& msg) {
        appendLog (msg);
    });

    refreshDeviceLists();
}

DM5AudioProcessorEditor::~DM5AudioProcessorEditor()
{
    audioProcessor.getMidiEngine().setLogCallback (nullptr);
}

void DM5AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void DM5AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (8);
    const int rowH   = 28;
    const int labelW = 68;
    const int gap    = 4;

    auto row1 = area.removeFromTop (rowH);
    inLabel.setBounds (row1.removeFromLeft (labelW));
    row1.removeFromLeft (gap);
    refreshBtn.setBounds (row1.removeFromRight (72));
    row1.removeFromRight (gap);
    inCombo.setBounds (row1);

    area.removeFromTop (gap);

    auto row2 = area.removeFromTop (rowH);
    outLabel.setBounds (row2.removeFromLeft (labelW));
    row2.removeFromLeft (gap);
    outCombo.setBounds (row2);

    area.removeFromTop (gap);

    auto row3 = area.removeFromTop (rowH + 4);
    requestBtn.setBounds (row3.withWidth (130));

    area.removeFromTop (gap);
    logBox.setBounds (area);
}

void DM5AudioProcessorEditor::refreshDeviceLists()
{
    auto& engine = audioProcessor.getMidiEngine();

    const auto savedIn  = inCombo.getText();
    const auto savedOut = outCombo.getText();

    inCombo.clear  (juce::dontSendNotification);
    outCombo.clear (juce::dontSendNotification);

    int selIn = 0, selOut = 0;

    for (auto& d : engine.getInputDevices())
    {
        inCombo.addItem (d.name, inCombo.getNumItems() + 1);
        if (d.name == savedIn)  selIn  = inCombo.getNumItems();
    }
    for (auto& d : engine.getOutputDevices())
    {
        outCombo.addItem (d.name, outCombo.getNumItems() + 1);
        if (d.name == savedOut) selOut = outCombo.getNumItems();
    }

    inCombo.setSelectedId  (selIn,  juce::dontSendNotification);
    outCombo.setSelectedId (selOut, juce::dontSendNotification);
}

void DM5AudioProcessorEditor::openSelectedPorts()
{
    auto& engine   = audioProcessor.getMidiEngine();
    const auto inName  = inCombo.getText();
    const auto outName = outCombo.getText();

    if (inName.isEmpty() || outName.isEmpty())
        return;

    juce::String inId, outId;
    for (auto& d : engine.getInputDevices())
        if (d.name == inName)  { inId  = d.identifier; break; }
    for (auto& d : engine.getOutputDevices())
        if (d.name == outName) { outId = d.identifier; break; }

    if (inId.isNotEmpty() && outId.isNotEmpty())
        engine.openPorts (inId, outId);
}

void DM5AudioProcessorEditor::appendLog (const juce::String& msg)
{
    logBox.moveCaretToEnd();
    logBox.insertTextAtCaret (msg + "\n");
}
