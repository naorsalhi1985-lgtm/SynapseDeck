#pragma once
#include <vector>
#include <cstdint>
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
    juce::MidiKeyboardState keyState;
    juce::MidiBuffer synthMidi;
    float lastVol = 0.8f;

    // arpeggiator + fx state (audio thread)
    void runArp (juce::MidiBuffer& in, juce::MidiBuffer& out, int n, double bpm, int rate, int pat, int range);
    juce::MidiBuffer arpOut;
    std::vector<int> heldNotes, seq;
    int arpCur = -1, arpIdx = 0;
    double arpCounter = 0.0, arpOffCounter = 0.0;
    juce::Random arpRng;
    int noteMap[128];
    double fxBeat = 0.0;
    float fxGainS = 1.0f, tapeLvl = 1.0f, muteS = 1.0f;
    float lofiHold[2] = { 0.0f, 0.0f };
    int lofiCnt = 0;
    float phoneLp[2] = { 0.0f, 0.0f }, phoneHp[2] = { 0.0f, 0.0f };
    juce::AudioBuffer<float> flangeBuf;
    int flangeW = 0;
    double flangeLfo = 0.0;

    // melody generator + fills (audio thread)
    void regenMelody (int seed, int density100);
    int melPat[64];
    int melSeed = -1, melDens = -1, melCur = -1, melOffAt = 0;
    long long melLastStep = -1000000;
    double melFree = 0.0;
    static const int kScaleTab[4][7];

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
