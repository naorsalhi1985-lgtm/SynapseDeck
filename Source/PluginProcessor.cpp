#include "PluginProcessor.h"
#include <algorithm>
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
            if (pi != lastPi) { lastPi = pi; fade = 0.0f; }
            const float subTarget = sub;

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
                subS += (subTarget - subS) * 0.002f;
                if (fade < 1.0f) fade = juce::jmin (1.0f, fade + 1.0f / 512.0f);
                const float s = std::sin ((float) (phSub * juce::MathConstants<double>::twoPi)) * subS * 1.1f;
                const float gain = env * noteVel * 0.2f * fade;
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
        int lastPi = -1;
        float fade = 1.0f, subS = 0.0f;
    };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SynapseDeckProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;
    using R = juce::NormalisableRange<float>;
    using I = juce::AudioParameterInt;

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
        juce::StringArray { "Goa Classic", "Full-On", "Progressive", "Dark Psy", "Forest", "Goa Rolling", "Goa Trippy", "Full-On Uplift", "Full-On Bounce", "Psytrance Zoom", "Psy Twilight", "Hi-Tech 172" }, 0));
    p.push_back (std::make_unique<B> ("bass", "Rolling Bass", true));
    p.push_back (std::make_unique<C> ("bassRoot", "Bass Root",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9));
    p.push_back (std::make_unique<F> ("drumLevel", "Drum Level", R (0.0f, 1.0f), 0.8f));
    p.push_back (std::make_unique<B> ("freeRun", "Drums Free Run", false));
    p.push_back (std::make_unique<F> ("freeBpm", "Free Run Tempo", R (70.0f, 180.0f, 1.0f), 142.0f));

    p.push_back (std::make_unique<B> ("scaleMinor", "Scale Minor", true));
    p.push_back (std::make_unique<I> ("octave", "Octave", 1, 6, 3));
    p.push_back (std::make_unique<B> ("arpOn", "Arp", false));
    p.push_back (std::make_unique<C> ("arpRate", "Arp Rate", juce::StringArray { "1/8", "1/16" }, 1));
    p.push_back (std::make_unique<C> ("arpPattern", "Arp Pattern", juce::StringArray { "UP", "DN", "U/D", "RND" }, 0));
    p.push_back (std::make_unique<C> ("arpRange", "Arp Range", juce::StringArray { "1 oct", "2 oct", "3 oct" }, 1));
    const char* fxIds[] = { "fxPump", "fxStutter", "fxTrance", "fxTape", "fxPitchUp", "fxPitchDn", "fxLofi", "fxPhone", "fxSpace", "fxFlange", "fxMute" };
    for (auto* id : fxIds) p.push_back (std::make_unique<B> (id, id, false));

    return { p.begin(), p.end() };
}

SynapseDeckProcessor::SynapseDeckProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < 128; ++i) noteMap[i] = i;
    heldNotes.reserve (256);
    seq.reserve (1024);
    arpOut.ensureSize (8192);
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
    flangeBuf.setSize (2, 4096);
    flangeBuf.clear();
    flangeW = 0;
    heldNotes.clear(); arpCur = -1; arpCounter = 0.0;
}

void SynapseDeckProcessor::runArp (juce::MidiBuffer& in, juce::MidiBuffer& out, int n, double bpm, int rate, int pat, int range)
{
    for (const auto meta : in)
    {
        const auto m = meta.getMessage();
        const int pos = meta.samplePosition;
        if (m.isNoteOn())
        {
            const int nn = m.getNoteNumber();
            if (std::find (heldNotes.begin(), heldNotes.end(), nn) == heldNotes.end())
            {
                const bool wasEmpty = heldNotes.empty();
                heldNotes.push_back (nn);
                std::sort (heldNotes.begin(), heldNotes.end());
                if (wasEmpty) { arpCounter = 0.0; arpIdx = 0; }
            }
        }
        else if (m.isNoteOff())
        {
            heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), m.getNoteNumber()), heldNotes.end());
            if (heldNotes.empty() && arpCur >= 0) { out.addEvent (juce::MidiMessage::noteOff (1, arpCur), pos); arpCur = -1; }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            heldNotes.clear();
            if (arpCur >= 0) { out.addEvent (juce::MidiMessage::noteOff (1, arpCur), pos); arpCur = -1; }
        }
        else
            out.addEvent (m, pos);
    }

    if (heldNotes.empty()) arpCounter = 0.0;
    const double stepLen = sr * 60.0 / juce::jmax (40.0, bpm) / (rate == 0 ? 2.0 : 4.0);
    for (int i = 0; i < n; ++i)
    {
        if (arpCur >= 0)
        {
            arpOffCounter -= 1.0;
            if (arpOffCounter <= 0.0) { out.addEvent (juce::MidiMessage::noteOff (1, arpCur), i); arpCur = -1; }
        }
        if (! heldNotes.empty() && arpCounter <= 0.0)
        {
            if (arpCur >= 0) { out.addEvent (juce::MidiMessage::noteOff (1, arpCur), i); arpCur = -1; }
            seq.clear();
            for (int o = 0; o <= range; ++o)
                for (int hn : heldNotes) seq.push_back (hn + 12 * o);
            const int sz = (int) seq.size();
            int idx = 0;
            if (pat == 0)      { idx = arpIdx % sz; ++arpIdx; }
            else if (pat == 1) { idx = sz - 1 - (arpIdx % sz); ++arpIdx; }
            else if (pat == 2) { const int period = juce::jmax (1, 2 * sz - 2); const int k = arpIdx % period; idx = k < sz ? k : period - k; ++arpIdx; }
            else               { idx = arpRng.nextInt (sz); }
            const int note = juce::jlimit (0, 127, seq[(size_t) idx]);
            out.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), i);
            arpCur = note;
            arpOffCounter = stepLen * 0.7;
            arpCounter += stepLen;
        }
        arpCounter -= 1.0;
    }
}

void SynapseDeckProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyState.processNextMidiBuffer (midi, 0, n, true);
    {
        synthMidi.clear();
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            if (msg.getChannel() == 10)
            {
                if (msg.isNoteOn()) drums.hitNote (msg.getNoteNumber(), meta.samplePosition, msg.getFloatVelocity());
            }
            else
            {
                auto m = msg;
                const bool pUp = apvts.getRawParameterValue ("fxPitchUp")->load() > 0.5f;
                const bool pDn = apvts.getRawParameterValue ("fxPitchDn")->load() > 0.5f;
                if (m.isNoteOn())
                {
                    const int nn = juce::jlimit (0, 127, m.getNoteNumber() + (pUp ? 12 : 0) - (pDn ? 12 : 0));
                    noteMap[m.getNoteNumber()] = nn;
                    m = juce::MidiMessage::noteOn (m.getChannel(), nn, m.getVelocity());
                }
                else if (m.isNoteOff())
                {
                    const int nn = noteMap[m.getNoteNumber()];
                    noteMap[m.getNoteNumber()] = m.getNoteNumber();
                    m = juce::MidiMessage::noteOff (m.getChannel(), nn, m.getVelocity());
                }
                synthMidi.addEvent (m, meta.samplePosition);
            }
        }
        midi.swapWith (synthMidi);
    }

    auto prm = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    // tempo for arp / fx
    double curBpm = prm ("freeBpm");
    if (auto* ph0 = getPlayHead())
        if (auto pos0 = ph0->getPosition())
            if (auto b0 = pos0->getBpm()) curBpm = *b0;

    // ---- arpeggiator ----
    if (prm ("arpOn") > 0.5f)
    {
        arpOut.clear();
        runArp (midi, arpOut, n, curBpm, (int) prm ("arpRate"), (int) prm ("arpPattern"), (int) prm ("arpRange"));
        midi.swapWith (arpOut);
    }
    else if (arpCur >= 0 || ! heldNotes.empty())
    {
        if (arpCur >= 0) midi.addEvent (juce::MidiMessage::noteOff (1, arpCur), 0);
        arpCur = -1; heldNotes.clear();
    }

    // ---- synth ----
    synth.renderNextBlock (buffer, midi, 0, n);

    // ---- performance fx pads (synth only) ----
    const bool fPump = prm ("fxPump") > 0.5f, fStut = prm ("fxStutter") > 0.5f, fTrance = prm ("fxTrance") > 0.5f,
               fTape = prm ("fxTape") > 0.5f, fLofi = prm ("fxLofi") > 0.5f, fPhone = prm ("fxPhone") > 0.5f,
               fSpace = prm ("fxSpace") > 0.5f, fFlange = prm ("fxFlange") > 0.5f, fMute = prm ("fxMute") > 0.5f;
    {
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);
        const double beatInc = juce::jmax (40.0, curBpm) / 60.0 / sr;
        const bool anyGate = fPump || fStut || fTrance;
        if (anyGate || fxGainS < 0.999f)
        {
            for (int i = 0; i < n; ++i)
            {
                fxBeat += beatInc;
                if (fxBeat > 100000.0) fxBeat -= 100000.0;
                const float f1 = (float) (fxBeat - std::floor (fxBeat));
                const double s4 = fxBeat * 4.0, s2 = fxBeat * 2.0;
                const float f16 = (float) (s4 - std::floor (s4)), f8 = (float) (s2 - std::floor (s2));
                float tgt = 1.0f;
                if (fPump)   tgt *= 1.0f - 0.92f * std::exp (-5.0f * f1);
                if (fStut)   tgt *= f16 < 0.5f ? 1.0f : 0.0f;
                if (fTrance) tgt *= f8 < 0.6f ? 1.0f : 0.1f;
                fxGainS += (tgt - fxGainS) * 0.05f;
                L[i] *= fxGainS; R[i] *= fxGainS;
            }
        }
        if (fLofi)
        {
            float* ch2[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                int cnt = lofiCnt;
                for (int i = 0; i < n; ++i)
                {
                    if ((cnt++ & 3) == 0) lofiHold[c] = std::round (ch2[c][i] * 24.0f) / 24.0f;
                    ch2[c][i] = lofiHold[c];
                }
            }
            lofiCnt = (lofiCnt + n) & 3;
        }
        if (fPhone)
        {
            const float hpA = std::exp (-juce::MathConstants<float>::twoPi * 500.0f / (float) sr);
            const float lpA = std::exp (-juce::MathConstants<float>::twoPi * 3200.0f / (float) sr);
            float* ch2[2] = { L, R };
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    phoneLp[c] += (1.0f - lpA) * (ch2[c][i] - phoneLp[c]);
                    const float y1 = phoneLp[c];
                    phoneHp[c] += (1.0f - hpA) * (y1 - phoneHp[c]);
                    ch2[c][i] = (y1 - phoneHp[c]) * 1.6f;
                }
        }
        if (fFlange)
        {
            const int N = flangeBuf.getNumSamples();
            auto* fb0 = flangeBuf.getWritePointer (0);
            auto* fb1 = flangeBuf.getWritePointer (1);
            for (int i = 0; i < n; ++i)
            {
                const float lfo = 0.5f + 0.5f * std::sin ((float) (flangeLfo * juce::MathConstants<double>::twoPi));
                flangeLfo += 0.3 / sr; if (flangeLfo >= 1.0) flangeLfo -= 1.0;
                const float dl = (0.7f + lfo * 3.5f) * 0.001f * (float) sr;
                float rpos = (float) flangeW - dl;
                while (rpos < 0.0f) rpos += (float) N;
                const int i0 = (int) rpos % N, i1 = (i0 + 1) % N;
                const float fr = rpos - std::floor (rpos);
                const float y0 = fb0[i0] * (1.0f - fr) + fb0[i1] * fr;
                const float y1 = fb1[i0] * (1.0f - fr) + fb1[i1] * fr;
                const float x0 = L[i], x1 = R[i];
                fb0[flangeW] = x0 + y0 * 0.45f; fb1[flangeW] = x1 + y1 * 0.45f;
                L[i] = x0 * 0.7f + y0 * 0.7f; R[i] = x1 * 0.7f + y1 * 0.7f;
                flangeW = (flangeW + 1) % N;
            }
        }
        // tape stop: fade + darken while held, quick recovery on release
        const float tapeTarget = fTape ? 0.0f : 1.0f;
        const float tapeStep = (float) n / (float) (sr * (fTape ? 0.9 : 0.12));
        const float tapePrev = tapeLvl;
        tapeLvl = tapeLvl < tapeTarget ? juce::jmin (tapeTarget, tapeLvl + tapeStep) : juce::jmax (tapeTarget, tapeLvl - tapeStep);
        if (tapePrev < 0.999f || tapeLvl < 0.999f)
            buffer.applyGainRamp (0, n, tapePrev * tapePrev, tapeLvl * tapeLvl);
    }

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
    const float cutoff = prm ("cutoff") * (0.04f + 0.96f * tapeLvl);
    const float q = prm ("reso");
    ladder.setCutoffFrequencyHz (cutoff);
    ladder.setResonance (juce::jmap (q, 0.7f, 14.0f, 0.05f, 0.80f));
    {
        juce::dsp::AudioBlock<float> blk (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        ladder.process (ctx);
    }
    // safety: a filter blow-up (NaN/inf) would otherwise silence everything until reload
    {
        bool bad = false;
        for (int ch = 0; ch < 2 && ! bad; ++ch)
        {
            auto* d = buffer.getReadPointer (ch);
            for (int i = 0; i < n; ++i) if (! std::isfinite (d[i]) || std::abs (d[i]) > 8.0f) { bad = true; break; }
        }
        if (bad) { ladder.reset(); reverb.reset(); limiter.reset(); buffer.clear(); }
    }

    // reverb (parallel wet)
    const float rv = prm ("reverb");
    juce::dsp::Reverb::Parameters rp;
    rp.roomSize = fSpace ? 0.95f : 0.78f; rp.damping = fSpace ? 0.2f : 0.4f;
    const float wet = fSpace ? juce::jmax (rv, 0.7f) : rv;
    rp.wetLevel = wet; rp.dryLevel = 1.0f - wet * 0.35f; rp.width = 1.0f;
    reverb.setParameters (rp);
    {
        juce::dsp::AudioBlock<float> blk (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        reverb.process (ctx);
    }

    auto renderHitsOnly = [&]
    {
        if (drumBuf.getNumSamples() < n) drumBuf.setSize (2, n, false, false, true);
        drumBuf.clear();
        if ((int) duck.size() < n) duck.assign ((size_t) n, 0.0f);
        drums.processHits (drumBuf.getWritePointer (0), drumBuf.getWritePointer (1), n, prm ("drumLevel"), duck.data());
        const float sc = prm ("sidechain");
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i) d[i] *= 1.0f - sc * duck[(size_t) i];
        }
        buffer.addFrom (0, 0, drumBuf, 0, 0, n);
        buffer.addFrom (1, 0, drumBuf, 1, 0, n);
    };

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
        if (! run) { drums.resetStep(); renderHitsOnly(); }
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
    {
        drums.resetStep();
        renderHitsOnly();
    }

    { const float muteT = fMute ? 0.0f : 1.0f; buffer.applyGainRamp (0, n, muteS, muteT); muteS = muteT; }
    { const float vol = prm ("volume"); buffer.applyGainRamp (0, n, lastVol, vol); lastVol = vol; }
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i) d[i] = std::isfinite (d[i]) ? juce::jlimit (-4.0f, 4.0f, d[i]) : 0.0f;
    }
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
