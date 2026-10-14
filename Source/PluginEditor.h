#pragma once
#include "PluginProcessor.h"

// Simple, robust editor: every parameter as a knob/menu/toggle.
class SynapseDeckEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit SynapseDeckEditor (SynapseDeckProcessor& p) : juce::GenericAudioProcessorEditor (p)
    {
        setSize (520, 760);
    }
};
