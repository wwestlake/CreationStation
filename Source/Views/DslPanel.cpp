#include "DslPanel.h"

DslPanel::DslPanel()
{
    setName("DSL");
    headerLabel.setText("Functional DSP DSL", juce::dontSendNotification);
    headerLabel.setFont(juce::Font(24.0f).boldened());
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerLabel);

    sourceEditor.setMultiLine(true);
    sourceEditor.setReturnKeyStartsNewLine(true);
    sourceEditor.setText(R"(# Creation Station DSP script
source osc1 = sine(freq = 220, amp = 0.25)
source mic1 = mic(gain = 0.70)
effect drive1 = drive(amount = 0.35)
effect tone1 = filter(cutoff = 0.62)
effect space1 = delay(time = 0.18, feedback = 0.24)
sink out = speakers(level = 1.0)
route main = osc1 -> drive1 -> tone1 -> space1 -> out
modulate drive1.amount = lfo(rate = 0.25, depth = 0.15)
)");
    addAndMakeVisible(sourceEditor);

    outputEditor.setMultiLine(true);
    outputEditor.setReadOnly(true);
    outputEditor.setText("Compile to see sources, effects, sinks, and routing diagnostics.");
    addAndMakeVisible(outputEditor);

    compileButton.onClick = [this] { compileSource(); };
    addAndMakeVisible(compileButton);

    compileSource();
}

void DslPanel::setSourceText(const juce::String& text)
{
    sourceEditor.setText(text, juce::dontSendNotification);
}

juce::String DslPanel::getSourceText() const
{
    return sourceEditor.getText();
}

void DslPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff11151c));
}

void DslPanel::resized()
{
    auto area = getLocalBounds().reduced(20);
    headerLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(10);

    auto left = area.removeFromLeft(area.getWidth() / 2 - 10);
    auto right = area;
    sourceEditor.setBounds(left.withTrimmedBottom(40));
    compileButton.setBounds(left.removeFromBottom(34).removeFromRight(140));
    outputEditor.setBounds(right);
}

void DslPanel::compileSource()
{
    auto result = compiler.compile(sourceEditor.getText());
    juce::String output;

    if (result.success)
    {
        output << "OK\n" << result.summary;
    }
    else
    {
        for (const auto& diagnostic : result.diagnostics)
            output << "Line " << diagnostic.line << ": " << diagnostic.message << "\n";
    }

    outputEditor.setText(output, juce::dontSendNotification);
}
