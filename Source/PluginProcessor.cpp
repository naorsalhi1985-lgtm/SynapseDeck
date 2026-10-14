#include "PluginProcessor.h"
#include "PluginEditor.h"

const SynapseDeckProcessor::Preset SynapseDeckProcessor::kPresets[4] = {
    { 0, 0, 12.0f,  0, 0.0f },   // Utopia
    { 0, 1,  9.0f,  0, 0.0f },   // Festival
    { 2, 0,  6.0f,  0, 0.0f },   // Pluck
    { 0, 1,  3.0f, -1, 0.8f }    // Sub Bass
};

namespace
{
    struct SynthSound : public juce::SynthesiserSound
    {
        bool appliesToNote (int) override { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    inline float polyBlep (double t, double dt)
    {
        if (t < dt)       { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    inline float osc (int wave, double ph, double dt)
    {
        switch (wave)
        {
            case 0:  return (float) (2.0 * ph - 1.0) - polyBlep (ph, dt);                    // saw
            case 1:
            {
                float v = ph < 0.5 ? 1.0f : -1.0f;
                v += polyBlep (ph, dt);
                v -= polyBlep (std::fmod (ph + 0.5, 1.0), dt);
                return v;                                                                       // square
            }
            default: return (float) (4.0 * std::abs (ph - 0.5) - 1.0);                        // triangle
        }
    }

    class SynapseVoice : public juce::SynthesiserVoice
    {
    public:
        explicit SynapseVoice (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

        bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SynthSound*> (s) != nullptr; }

        void startNote (int midi, float vel, juce::SynthesiserSound*, int) override
        {
            noteVel = vel;
            baseMidi = midi;
            for (int u = 0; u < kMaxUni; ++u) ph[u] = std::fmod ((double) (baseMidi * 7 + u * 13) * 0.0618, 1.0);
            phSub = 0.0;
            updateParams();
            adsr.noteOn();
        }

        void stopNote (float, bool allowTail) override
        {
            if (allowTail) adsr.noteOff();
            else { adsr.reset(); clearCurrentNote(); }
        }

        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}

        void prepare (double sampleRate)
        {
            setCurrentPlaybackSampleRate (sampleRate);
            adsr.setSampleRate (sampleRate);
        }

        void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
        {
            if (! isVoiceActive()) return;
            updateParams();

            const int pi = juce::jlimit (0, 3, (int) apvts.getRawParameterValue ("preset")->load());
            const auto& P = SynapseDeckProcessor::kPresets[pi];
            const double sr = getSampleRate();
            const double f = 440.0 * std::pow (2.0, (baseMidi - 69 + 12 * P.oct) / 12.0);
            const double spread = P.det * apvts.getRawParameterValue ("width")->load() / 10.0;
            const int N = juce::jlimit (1, kMaxUni, (int) apvts.getRawParameterValue ("unison")->load());
            const float sub = juce::jmax (P.sub, apvts.getRawParameterValue ("sub")->load());
            const float norm = 1.0f / std::sqrt ((float) N);

            double dt[kMaxUni]; float gl[kMaxUni], gr[kMaxUni]; int wv[kMaxUni];
            for (int u = 0; u < N; ++u)
            {
                const double pos = N > 1 ? (double) u / (N - 1) * 2.0 - 1.0 : 0.0;
                dt[u] = f * std::pow (2.0, pos * spread / 1200.0) / sr;
                wv[u] = (u % 2 == 0) ? P.w1 : P.w2;
                const float pan = (float) (0.5 + 0.45 * pos);
                gl[u] = std::cos (pan * juce::MathConstants<float>::halfPi);
                gr[u] = std::sin (pan * juce::MathConstants<float>::halfPi);
            }
            const double dS = (f * 0.5) / sr;

            auto* L = out.getWritePointer (0, start);
            auto* R = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;

            for (int i = 0; i < num; ++i)
            {
                const float env = adsr.getNextSample();
                float sl = 0.0f, sr2 = 0.0f;
                for (int u = 0; u < N; ++u)
                {
                    const float o = osc (wv[u], ph[u], dt[u]);
                    sl += o * gl[u]; sr2 += o * gr[u];
                    ph[u] += dt[u]; if (ph[u] >= 1.0) ph[u] -= 1.0;
                }
                const float s = std::sin ((float) (phSub * juce::MathConstants<double>::twoPi)) * sub * 1.1f;
                const float gain = env * noteVel * 0.2f;
                L[i] += (sl * norm * 1.2f + s) * gain;
                if (R) R[i] += (sr2 * norm * 1.2f + s) * gain;
                phSub += dS; if (phSub >= 1.0) phSub -= 1.0;
            }

            if (! adsr.isActive()) clearCurrentNote();
        }

    private:
        void updateParams()
        {
            juce::ADSR::Parameters p;
            p.attack  = *apvts.getRawParameterValue ("attack");
            p.decay   = *apvts.getRawParameterValue ("decay");
            p.sustain = *apvts.getRawParameterValue ("sustain");
            p.release = *apvts.getRawParameterValue ("release");
            adsr.setParameters (p);
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::ADSR adsr;
        static constexpr int kMaxUni = 9;
        double ph[kMaxUni] {};
        double phSub = 0;
        float noteVel = 1.0f;
        int baseMidi = 60;
    };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SynapseDeckProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;
    using R = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<C> ("preset", "Sound", juce::StringArray { "Utopia", "Festival", "Pluck", "Sub Bass" }, 0));

    R cutR (200.0f, 12000.0f); cutR.setSkewForCentre (1500.0f);
    p.push_back (std::make_unique<F> ("cutoff", "Cutoff", cutR, 3200.0f));
    p.push_back (std::make_unique<F> ("reso", "Reso", R (0.7f, 14.0f), 1.2f));
    p.push_back (std::make_unique<F> ("reverb", "Reverb", R (0.0f, 0.7f), 0.22f));
    p.push_back (std::make_unique<F> ("width", "Width", R (0.0f, 40.0f), 10.0f));
    p.push_back (std::make_unique<F> ("drive", "Drive", R (0.0f, 1.0f), 0.0f));
    p.push_back (std::make_unique<F> ("sub", "Sub", R (0.0f, 1.0f), 0.0f));
    p.push_back (std::make_unique<F> ("unison", "Supersaw Voices", R (1.0f, 9.0f, 1.0f), 5.0f));
    p.push_back (std::make_unique<F> ("sidechain", "Kick Sidechain", R (0.0f, 1.0f), 0.6f));

    R atkR (0.002f, 2.0f); atkR.setSkewForCentre (0.1f);
    R decR (0.05f, 2.0f);  decR.setSkewForCentre (0.4f);
    R relR (0.05f, 3.0f);  relR.setSkewForCentre (0.5f);
    p.push_back (std::make_unique<F> ("attack", "Attack", atkR, 0.28f));
    p.push_back (std::make_unique<F> ("decay", "Decay", decR, 0.5f));
    p.push_back (std::make_unique<F> ("sustain", "Sustain", R (0.0f, 1.0f), 0.8f));
    p.push_back (std::make_unique<F> ("release", "Release", relR, 1.3f));
    p.push_back (std::make_unique<F> ("volume", "Volume", R (0.0f, 1.0f), 0.8f));

    p.push_back (std::make_unique<B> ("drums", "Drums", false));
    p.push_back (std::make_unique<C> ("style", "Rhythm Style",
        juce::StringArray { "Goa Classic", "Full-On", "Progressive", "Dark Psy", "Forest" }, 0));
    p.push_back (std::make_unique<B> ("bass", "Rolling Bass", true));
    p.push_back (std::make_unique<C> ("bassRoot", "Bass Root",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9));
    p.push_back (std::make_unique<F> ("drumLevel", "Drum Level", R (0.0f, 1.0f), 0.8f));
    p.push_back (std::make_unique<B> ("freeRun", "Drums Free Run", false));
    p.push_back (std::make_unique<F> ("freeBpm", "Free Run Tempo", R (70.0f, 180.0f, 1.0f), 142.0f));

    return { p.begin(), p.end() };
}

SynapseDeckProcessor::SynapseDeckProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    synth.addSound (new SynthSound());
    for (int i = 0; i < 12; ++i)
        synth.addVoice (new SynapseVoice (apvts));
}

bool SynapseDeckProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SynapseDeckProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynapseVoice*> (synth.getVoice (i))) v->prepare (sampleRate);

    juce::dsp::ProcessSpec stereo { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    ladder.prepare (stereo);
    ladder.setMode (juce::dsp::LadderFilterMode::LPF24);
    ladder.setDrive (1.4f);
    reverb.prepare (stereo);
    limiter.prepare (stereo);
    limiter.setThreshold (-1.0f);
    limiter.setRelease (80.0f);
    duck.assign ((size_t) samplesPerBlock, 0.0f);

    drums.prepare (sampleRate);
    drumBuf.setSize (2, samplesPerBlock);
    freeRunPos = 0.0;
}

void SynapseDeckProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    auto prm = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    // ---- synth ----
    synth.renderNextBlock (buffer, midi, 0, n);

    // drive
    const float drive = prm ("drive");
    if (drive > 0.02f)
    {
        const float k = drive * 6.0f, norm = 1.0f / std::tanh (k);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i) d[i] = std::tanh (k * d[i] * 3.0f) * norm * 0.33f;
        }
    }

    // filter
    const float cutoff = prm ("cutoff");
    const float q = prm ("reso");
    ladder.setCutoffFrequencyHz (cutoff);
    ladder.setResonance (juce::jmap (q, 0.7f, 14.0f, 0.05f, 0.92f));
    {
        juce::dsp::AudioBlock<float> blk (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        ladder.process (ctx);
    }

    // reverb (parallel wet)
    const float rv = prm ("reverb");
    juce::dsp::Reverb::Parameters rp;
    rp.roomSize = 0.78f; rp.damping = 0.4f; rp.wetLevel = rv; rp.dryLevel = 1.0f - rv * 0.35f; rp.width = 1.0f;
    reverb.setParameters (rp);
    {
        juce::dsp::AudioBlock<float> blk (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        reverb.process (ctx);
    }

    // ---- drums + bass (bypass the synth filter) ----
    if (prm ("drums") > 0.5f)
    {
        double bpm = 0.0, stepPos = 0.0;
        bool run = false;
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
            {
                if (pos->getIsPlaying())
                {
                    if (auto b = pos->getBpm())      bpm = *b;
                    if (auto q16 = pos->getPpqPosition()) { stepPos = *q16 * 4.0; run = (bpm > 0.0); }
                }
            }
        }
        if (! run && prm ("freeRun") > 0.5f)
        {
            bpm = prm ("freeBpm");
            stepPos = freeRunPos;
            freeRunPos += n * bpm / 60.0 * 4.0 / sr;
            run = true;
        }
        if (! run) { drums.reset(); }
        else
        {
            if (drumBuf.getNumSamples() < n) drumBuf.setSize (2, n, false, false, true);
            drumBuf.clear();
            if ((int) duck.size() < n) duck.assign ((size_t) n, 0.0f);
            drums.process (drumBuf.getWritePointer (0), drumBuf.getWritePointer (1), n, stepPos, bpm,
                           (int) prm ("style"), prm ("bass") > 0.5f, (int) prm ("bassRoot"), prm ("drumLevel"), duck.data());
            const float sc = prm ("sidechain");
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                for (int i = 0; i < n; ++i) d[i] *= 1.0f - sc * duck[(size_t) i];
            }
            buffer.addFrom (0, 0, drumBuf, 0, 0, n);
            buffer.addFrom (1, 0, drumBuf, 1, 0, n);
        }
    }
    else
        drums.reset();

    buffer.applyGain (prm ("volume"));
    {
        juce::dsp::AudioBlock<float> blk (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        limiter.process (ctx);
    }
}

juce::AudioProcessorEditor* SynapseDeckProcessor::createEditor() { return new SynapseDeckEditor (*this); }

void SynapseDeckProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void SynapseDeckProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SynapseDeckProcessor(); }
