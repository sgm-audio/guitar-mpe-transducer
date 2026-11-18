#include "PluginEditor.h"

GuitarTransducerAudioProcessorEditor::GuitarTransducerAudioProcessorEditor(GuitarTransducerAudioProcessor& p)
    : AudioProcessorEditor(&p), processor_(p)
{
    addAndMakeVisible(generic_);
    setSize(420, 260);
}

void GuitarTransducerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void GuitarTransducerAudioProcessorEditor::resized()
{
    generic_.setBounds(getLocalBounds());
}
