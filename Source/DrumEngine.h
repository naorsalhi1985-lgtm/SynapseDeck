#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

// Psy / Goa drum machine + rolling bass. Sample accurate, host synced.
struct RhythmStyle
{
    const char* name;
    double bpm;
    const char *kick, *openHat, *closedHat, *clap, *perc, *ride, *bass;
    int bassOct[16];
};

static const RhythmStyle kStyles[12] = {
    { "Goa Classic", 142, "x...x...x...x...", "..x...x...x...x.", ".x.x.x.x.x.x.x.x", "....x.......x...", "................", "................",
      ".xxx.xxx.xxx.xxx", { 0,0,0,12,0,0,12,0,0,0,0,12,0,12,0,12 } },
    { "Full-On", 145, "x...x...x...x...", "..x...x...x...x.", "x.x.x.x.x.x.x.x.", "....x.......x.x.", "...x.......x..x.", "................",
      ".xxx.xxx.xxx.xxx", { 0,0,12,0,0,12,0,12,0,0,12,0,0,12,0,0 } },
    { "Progressive", 136, "x...x...x...x...", "..x...x...x...x.", ".x.x.x.x.x.x.x.x", "....x.......x...", "................", "x...x...x...x...",
      "..x...x...x...xx", { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,12 } },
    { "Dark Psy", 148, "x...x...x...x...", "..x...x...x...xx", "xxxxxxxxxxxxxxxx", "....x.......x...", "..x..x..x..x..x.", "................",
      ".xxx.xxx.xxx.xxx", { 0,0,12,0,0,0,1,0,0,12,0,0,0,1,0,0 } },
    { "Forest", 152, "x...x...x...x...", "..x...x...x...x.", ".x.x.x.x.x.x.x.x", "....x.......x...", "...x..x....x..x.", "x.x.x.x.x.x.x.x.",
      ".xx..xxx.xx..xxx", { 0,0,0,12,0,0,3,12,0,0,0,12,0,0,5,12 } },
    { "Goa Rolling", 144, "x...x...x...x...", "..x...x...x...x.", ".x.x.x.x.x.x.x.x", "....x.......x...", "...x.......x....", "................",
      ".xxx.xxx.xxx.xxx", { 0,0,12,0,0,12,0,12,0,0,12,0,0,12,0,0 } },
    { "Goa Trippy", 138, "x...x...x...x..x", "..x...x...x...x.", ".x.x.x.x.x.x.x.x", "....x.......x...", ".x..x...x..x....", "................",
      ".x.xx.x..x.xx.x.", { 0,0,0,12,0,0,0,0,0,0,0,12,0,0,0,0 } },
    { "Full-On Uplift", 146, "x...x...x...x...", "..x...x...x...x.", "x.x.x.x.x.x.x.x.", "....x.......x.x.", "..x...x...x...x.", "................",
      ".xxx.xxx.xxx.xxx", { 0,0,12,0,0,12,0,0,0,0,12,0,0,12,0,0 } },
    { "Full-On Bounce", 148, "x...x...x...x...", "..x...x...x...x.", ".x.x.x.x.x.x.x.x", "....x.......x...", "...x.......x..x.", "x.x.x.x.x.x.x.x.",
      "x.xx.xx.x.xx.xx.", { 0,0,12,0,0,12,0,0,0,0,12,0,0,12,0,0 } },
    { "Psytrance Zoom", 145, "x...x...x...x...", "..x...x...x...x.", ".x.x.x.x.x.x.x.x", "....x.......x...", "................", "................",
      ".xxxxxxx.xxxxxxx", { 0,0,12,0,12,0,12,0,0,0,12,0,12,0,12,0 } },
    { "Psy Twilight", 150, "x...x...x...x...", "..x...x...x...xx", "xxxxxxxxxxxxxxxx", "....x.......x...", "..x..x..x..x..x.", "................",
      ".xx.xx.xx.xx.xx.", { 0,0,12,0,0,13,0,0,12,0,0,13,0,0,12,0 } },
    { "Hi-Tech 172", 172, "x...x...x...x...", "..x...x...x...x.", "x.x.x.x.x.x.x.x.", "....x.......x...", ".x..x..x.x..x...", "................",
      "xxxxxxxxxxxxxxxx", { 0,12,0,12,0,12,0,12,0,12,0,12,0,12,0,13 } }
};

class DrumEngine
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto& h : hits) h.active = false;
        lastStep = -1000000;
        rng.setSeedRandomly();
    }

    void reset() { for (auto& h : hits) h.active = false; lastStep = -1000000; }

    void resetStep() { lastStep = -1000000; }

    // Drum notes on MIDI channel 10 (GM-ish map). delaySamples = offset inside the block.
    void hitNote (int note, int delaySamples, float vel)
    {
        const float v = juce::jlimit (0.2f, 1.0f, vel) ;
        const float ds = (float) delaySamples / (float) sampleRate;
        switch (note)
        {
            case 36: { auto* h = alloc(); *h = Hit();
                       h->active = true; h->kind = Kick; h->vol = 0.95f * v; h->len = (int) (0.27 * sampleRate);
                       h->f0 = 260.0; h->f1 = 47.0; h->delay = delaySamples; duckEnv = 1.0f; break; }
            case 39: noise (0.02f, 0.22f * v, 1400.0f, 1.2f, false, ds);
                     noise (0.02f, 0.22f * v, 1400.0f, 1.2f, false, ds + 0.012f);
                     noise (0.14f, 0.20f * v, 1400.0f, 1.2f, false, ds + 0.024f); break;
            case 42: noise (0.04f, 0.07f * v, 8500.0f, 0.7f, true, ds); break;
            case 46: noise (0.15f, 0.15f * v, 7000.0f, 0.7f, true, ds); break;
            case 51: noise (0.22f, 0.05f * v, 9500.0f, 0.7f, true, ds); break;
            case 45: { auto* h = alloc(); *h = Hit();
                       h->active = true; h->kind = Perc; h->vol = 0.3f * v; h->len = (int) (0.13 * sampleRate);
                       h->f0 = 420.0; h->f1 = 190.0; h->delay = delaySamples; break; }
            default: break;
        }
    }

    // Renders already-triggered hits without running the step sequencer.
    void processHits (float* L, float* R, int n, float level, float* duckOut)
    {
        const float duckDecay = std::exp (-1.0f / (0.17f * (float) sampleRate));
        for (int i = 0; i < n; ++i)
        {
            if (duckOut != nullptr) duckOut[i] = duckEnv;
            duckEnv *= duckDecay;
            float s = 0.0f;
            for (auto& h : hits) if (h.active) s += render (h);
            s *= level;
            L[i] += s;
            R[i] += s;
        }
    }

    // Renders into L/R (adds). stepStartPos is the position in 16th notes at the first sample.
    void process (float* L, float* R, int n, double stepStartPos, double bpm,
                  int style, bool bassOn, int bassRoot, float level, float* duckOut)
    {
        const double stepsPerSample = bpm / 60.0 * 4.0 / sampleRate;
        const auto& st = kStyles[juce::jlimit (0, 11, style)];
        const float sixteenth = (float) (sampleRate * 60.0 / bpm / 4.0);
        const float duckDecay = std::exp (-1.0f / (0.17f * (float) sampleRate));

        for (int i = 0; i < n; ++i)
        {
            const int step = (int) std::floor (stepStartPos + i * stepsPerSample);
            if (step != lastStep)
            {
                lastStep = step;
                trigger (st, ((step % 16) + 16) % 16, bassOn, bassRoot, sixteenth);
            }
            if (duckOut != nullptr) duckOut[i] = duckEnv;
            duckEnv *= duckDecay;
            float s = 0.0f;
            for (auto& h : hits) if (h.active) s += render (h);
            s *= level;
            L[i] += s;
            R[i] += s;
        }
    }

private:
    enum Kind { Kick, Noise, Perc, Bass };

    struct Hit
    {
        bool active = false;
        Kind kind = Kick;
        int delay = 0, age = 0, len = 1;
        float vol = 1.0f;
        // osc
        double phase = 0.0, phase2 = 0.0, f0 = 100.0, f1 = 50.0;
        // svf
        bool highpass = true;
        float g = 0.0f, k = 1.0f, ic1 = 0.0f, ic2 = 0.0f;
        float lpCut0 = 400.0f, lpCut1 = 130.0f;
    };

    static bool on (const char* s, int p) { return s[p] == 'x'; }

    Hit* alloc()
    {
        for (auto& h : hits) if (! h.active) return &h;
        return &hits[0];
    }

    void setSvf (Hit& h, float cutoff, float q, bool hp)
    {
        h.highpass = hp;
        h.g = std::tan (juce::MathConstants<float>::pi * juce::jmin (cutoff, (float) sampleRate * 0.45f) / (float) sampleRate);
        h.k = 1.0f / q;
        h.ic1 = h.ic2 = 0.0f;
    }

    void noise (float lenSec, float vol, float cutoff, float q, bool hp, float delaySec = 0.0f)
    {
        auto* h = alloc();
        *h = Hit();
        h->active = true; h->kind = Noise; h->vol = vol;
        h->len = juce::jmax (8, (int) (lenSec * sampleRate));
        h->delay = (int) (delaySec * sampleRate);
        setSvf (*h, cutoff, q, hp);
    }

    void trigger (const RhythmStyle& st, int p, bool bassOn, int bassRoot, float sixteenth)
    {
        if (on (st.kick, p))
        {
            auto* h = alloc(); *h = Hit();
            h->active = true; h->kind = Kick; h->vol = 0.95f; h->len = (int) (0.27 * sampleRate);
            h->f0 = 260.0; h->f1 = 47.0;
            duckEnv = 1.0f;
            noise (0.008f, 0.30f, 4200.0f, 0.7f, true);
        }
        if (on (st.openHat, p))        noise (0.15f, 0.15f, 7000.0f, 0.7f, true);
        else if (on (st.closedHat, p)) noise (0.04f, 0.07f, 8500.0f, 0.7f, true);
        if (on (st.clap, p))
        {
            noise (0.02f, 0.22f, 1400.0f, 1.2f, false, 0.0f);
            noise (0.02f, 0.22f, 1400.0f, 1.2f, false, 0.012f);
            noise (0.14f, 0.20f, 1400.0f, 1.2f, false, 0.024f);
        }
        if (on (st.ride, p)) noise (0.22f, 0.05f, 9500.0f, 0.7f, true);
        if (on (st.perc, p))
        {
            auto* h = alloc(); *h = Hit();
            h->active = true; h->kind = Perc; h->vol = 0.3f; h->len = (int) (0.13 * sampleRate);
            h->f0 = 420.0; h->f1 = 190.0;
        }
        if (bassOn && on (st.bass, p))
        {
            auto* h = alloc(); *h = Hit();
            const int midi = 36 + bassRoot + st.bassOct[p];
            h->active = true; h->kind = Bass; h->vol = 0.34f;
            h->lpCut0 = 2400.0f; h->lpCut1 = 150.0f;
            h->len = juce::jmax (64, (int) (sixteenth * 0.85f));
            h->f0 = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
            setSvf (*h, 1500.0f, 4.2f, false);
        }
    }

    float svfRun (Hit& h, float x)
    {
        // TPT state variable filter
        const float a1 = 1.0f / (1.0f + h.g * (h.g + h.k));
        const float a2 = h.g * a1;
        const float a3 = h.g * a2;
        const float v3 = x - h.ic2;
        const float v1 = a1 * h.ic1 + a2 * v3;
        const float v2 = h.ic2 + a2 * h.ic1 + a3 * v3;
        h.ic1 = 2.0f * v1 - h.ic1;
        h.ic2 = 2.0f * v2 - h.ic2;
        if (h.kind == Bass) return v2;                 // low-pass
        if (h.highpass) return x - h.k * v1 - v2;      // high-pass
        return v1;                                     // band-pass
    }

    float render (Hit& h)
    {
        if (h.delay > 0) { --h.delay; return 0.0f; }
        if (h.age >= h.len) { h.active = false; return 0.0f; }

        const float t = (float) h.age / (float) h.len;
        const float env = std::exp (-6.9f * t);
        float out = 0.0f;
        const double twoPi = juce::MathConstants<double>::twoPi;

        switch (h.kind)
        {
            case Kick:
            {
                const double f = h.f1 + (h.f0 - h.f1) * std::exp (-(double) h.age / (0.016 * sampleRate));
                h.phase += twoPi * f / sampleRate;
                const float atk = juce::jmin (1.0f, (float) h.age / (0.003f * (float) sampleRate));
                out = std::tanh (2.6f * (float) std::sin (h.phase)) * 0.95f * env * atk * h.vol;
                break;
            }
            case Perc:
            {
                const double f = h.f1 + (h.f0 - h.f1) * std::exp (-(double) h.age / (0.03 * sampleRate));
                h.phase += twoPi * f / sampleRate;
                out = (float) std::sin (h.phase) * env * h.vol;
                break;
            }
            case Noise:
            {
                const float n = rng.nextFloat() * 2.0f - 1.0f;
                out = svfRun (h, n) * env * h.vol;
                break;
            }
            case Bass:
            {
                h.phase += h.f0 / sampleRate;
                if (h.phase >= 1.0) h.phase -= 1.0;
                h.phase2 += h.f0 * 1.006 / sampleRate;
                if (h.phase2 >= 1.0) h.phase2 -= 1.0;
                const float saw = (float) ((h.phase * 2.0 - 1.0) * 0.6 + (h.phase2 * 2.0 - 1.0) * 0.4);
                const float sub = (float) std::sin (twoPi * h.phase);
                const float cut = h.lpCut0 * std::pow (h.lpCut1 / h.lpCut0, juce::jmin (1.0f, t / 0.55f));
                h.g = std::tan (juce::MathConstants<float>::pi * juce::jmin (cut, (float) sampleRate * 0.45f) / (float) sampleRate);
                const float filtered = svfRun (h, saw);
                const float a = juce::jmin (1.0f, (float) h.age / (0.004f * (float) sampleRate));
                const float rel = t > 0.7f ? juce::jmax (0.0f, 1.0f - (t - 0.7f) / 0.3f) : 1.0f;
                out = std::tanh ((filtered * 1.4f + sub * 0.7f)) * a * rel * h.vol;
                break;
            }
        }
        ++h.age;
        return out;
    }

    float duckEnv = 0.0f;
    double sampleRate = 44100.0;
    int lastStep = -1000000;
    std::array<Hit, 48> hits;
    juce::Random rng;
};
