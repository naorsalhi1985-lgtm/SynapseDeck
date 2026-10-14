#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "DrumEngine.h"

class SynapseDeckProcessor : public juce::AudioProcessor
{
public:
    SynapseDeckProcessor();
    ~SynapseDeckProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Synapse Deck"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // preset table shared with the voices
    struct Preset { int w1, w2; float det; int oct; float sub; };   // wave: 0 saw, 1 square, 2 tri
    static const Preset kPresets[4];

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::Synthesiser synth;
    DrumEngine drums;
    juce::dsp::LadderFilter<float> ladder;
    juce::dsp::Reverb reverb;
    juce::dsp::Limiter<float> limiter;
    std::vector<float> duck;
    juce::AudioBuffer<float> drumBuf;
    double freeRunPos = 0.0;
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynapseDeckProcessor)
};
