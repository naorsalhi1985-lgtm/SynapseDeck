#pragma once
#include "BgData.h"
#include "PluginProcessor.h"

struct DeckLook : public juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        auto col = s.findColour (juce::Slider::rotarySliderFillColourId);
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        auto c = b.getCentre();
        const float rr = r - 5.0f;
        const float ang = a0 + pos * (a1 - a0);
        g.setColour (juce::Colour (0xcc0e0720));
        g.fillEllipse (c.x - rr + 5.0f, c.y - rr + 5.0f, (rr - 5.0f) * 2.0f, (rr - 5.0f) * 2.0f);
        juce::Path track;
        track.addCentredArc (c.x, c.y, rr, rr, 0.0f, a0, a1, true);
        g.setColour (juce::Colour (0x44ffffff));
        g.strokePath (track, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path val;
        val.addCentredArc (c.x, c.y, rr, rr, 0.0f, a0, ang, true);
        g.setColour (col.withAlpha (0.25f));
        g.strokePath (val, juce::PathStrokeType (11.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col);
        g.strokePath (val, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float tx = c.x + rr * std::sin (ang), ty = c.y - rr * std::cos (ang);
        g.setColour (col.withAlpha (0.45f));
        g.fillEllipse (tx - 8.0f, ty - 8.0f, 16.0f, 16.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (tx - 4.0f, ty - 4.0f, 8.0f, 8.0f);
        g.setFont (juce::FontOptions (juce::jmax (10.0f, r * 0.36f), juce::Font::bold));
        g.drawText (s.getTextFromValue (s.getValue()), b.toNearestInt(), juce::Justification::centred);
    }
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& t, bool, bool) override
    {
        auto b = t.getLocalBounds().toFloat().reduced (1.5f);
        const bool on = t.getToggleState();
        g.setColour (on ? juce::Colour (0xffff9a3c) : juce::Colour (0xaa10081e));
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (juce::Colour (0x88ffffff));
        g.drawRoundedRectangle (b, 8.0f, 1.2f);
        g.setColour (on ? juce::Colours::black : juce::Colours::white);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (t.getButtonText(), t.getLocalBounds(), juce::Justification::centred);
    }
};

struct OrbPad : public juce::Component, private juce::Timer
{
    explicit OrbPad (juce::AudioProcessorValueTreeState& s) : apvts (s) { startTimerHz (30); }
    void timerCallback() override { repaint(); }
    juce::Point<float> orbPos() const
    {
        auto b = getLocalBounds().toFloat();
        const float cx = apvts.getParameter ("cutoff")->getValue();
        const float ry = apvts.getParameter ("reso")->getValue();
        return { b.getX() + 60.0f + cx * (b.getWidth() - 120.0f), b.getY() + 40.0f + (1.0f - ry) * (b.getHeight() - 110.0f) };
    }
    void paint (juce::Graphics& g) override
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a0b3a), 0.0f, 0.0f, juce::Colour (0xff04101e), 0.0f, (float) getHeight(), false));
        g.fillAll();
        if (bgImage.isValid())
        {
            g.drawImage (bgImage, getLocalBounds().toFloat());
            g.setColour (juce::Colour (0x55050212));
            g.fillAll();
        }
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (36.0f, juce::Font::bold));
        g.drawText ("SYNAPSE", 28, 14, 190, 44, juce::Justification::left);
        g.setColour (juce::Colour (0xffff9a3c));
        g.drawText ("DECK", 206, 14, 110, 44, juce::Justification::left);
        g.setColour (juce::Colour (0xaaffffff));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("psy / goa engine  -  drums / rolling bass / synth", 30, 56, 420, 16, juce::Justification::left);

        auto panel = [&] (juce::Rectangle<int> r, const char* t)
        {
            g.setColour (juce::Colour (0x990a0418)); g.fillRoundedRectangle (r.toFloat(), 14.0f);
            g.setColour (juce::Colour (0x33ffffff)); g.drawRoundedRectangle (r.toFloat(), 14.0f, 1.0f);
            g.setColour (juce::Colour (0xffb070ff)); g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (t, r.getX() + 16, r.getY() + 8, 240, 14, juce::Justification::left);
        };
        panel ({ 20, 84, 580, 216 }, "SYNTH");
        panel ({ 620, 84, 560, 216 }, "ENVELOPE");
        panel ({ 20, 312, 1160, 128 }, "SOUND");
        panel ({ 20, 452, 1160, 56 }, "");
        panel ({ 20, 520, 1160, 180 }, "KEYS");

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        for (auto& k : knobs)
            g.drawText (k->label, k->s.getX() - 10, k->s.getBottom() - 6, k->s.getWidth() + 20, 16, juce::Justification::centred);
    }

    void resized() override
    {
        preset.setBounds (440, 20, 330, 40);
        prevB.setBounds (396, 22, 36, 36);
        nextB.setBounds (778, 22, 36, 36);
        orb.setBounds (30, 104, 560, 190);
        envView.setBounds (630, 104, 540, 190);
        const char* ids[] = { "cutoff", "reso", "reverb", "width", "volume", "sub", "drive",
                              "attack", "decay", "sustain", "release", "sidechain", "unison", "drumLevel" };
        for (int i = 0; i < 14; ++i)
            for (auto& k : knobs)
                if (k->id == ids[i]) k->s.setBounds (30 + i * 82, 330, 80, 80);
        style.setBounds (34, 463, 220, 34);
        root.setBounds (264, 463, 90, 34);
        drumsB.setBounds (370, 463, 130, 34);
        bassB.setBounds (510, 463, 160, 34);
        freeB.setBounds (680, 463, 120, 34);
        expDrums.setBounds (830, 463, 160, 34);
        expBass.setBounds (1000, 463, 160, 34);
        keys.setBounds (32, 548, 1136, 142);
    }

private:
    struct Knob
    {
        juce::Slider s; juce::String id, label, suffix;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    void addKnob (const juce::String& id, const juce::String& label, juce::uint32 col, const juce::String& suffix)
    {
        auto k = std::make_unique<Knob>();
        k->id = id; k->label = label; k->suffix = suffix;
        k->s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k->s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k->s.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (col));
        k->s.setNumDecimalPlacesToDisplay (id == "cutoff" || id == "unison" || id == "width" ? 0 : 2);
        k->s.setTextValueSuffix (suffix);
        k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->s);
        addAndMakeVisible (k->s);
        knobs.push_back (std::move (k));
    }
    void addCombo (juce::ComboBox& c, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& a, const juce::String& id)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
            c.addItemList (p->choices, 1);
        a = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c);
        c.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xcc1a0c33));
        c.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        c.setColour (juce::ComboBox::outlineColourId, juce::Colour (0x88ffffff));
        addAndMakeVisible (c);
    }
    void addToggle (juce::ToggleButton& b, std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& a,
                    const juce::String& id, const juce::String& text)
    {
        b.setButtonText (text);
        a = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b);
        addAndMakeVisible (b);
    }

    void exportMidi (bool drumsFile)
    {
        const int styleIdx = juce::jlimit (0, 11, (int) *proc.apvts.getRawParameterValue ("style"));
        const int rootNote = (int) *proc.apvts.getRawParameterValue ("bassRoot");
        const auto& st = kStyles[styleIdx];
        const int ppq = 480, stepTicks = ppq / 4, bars = 8;
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / st.bpm)), 0.0);
        auto add = [&] (int ch, int note, int step, int lenTicks, float vel)
        {
            const double t = (double) (step * stepTicks);
            seq.addEvent (juce::MidiMessage::noteOn (ch, note, vel), t);
            seq.addEvent (juce::MidiMessage::noteOff (ch, note), t + lenTicks);
        };
        for (int s = 0; s < bars * 16; ++s)
        {
            const int p = s % 16;
            if (drumsFile)
            {
                if (st.kick[p] == 'x')      add (10, 36, s, 60, 0.95f);
                if (st.clap[p] == 'x')      add (10, 39, s, 60, 0.9f);
                if (st.openHat[p] == 'x')   add (10, 46, s, 60, 0.8f);
                else if (st.closedHat[p] == 'x') add (10, 42, s, 40, 0.6f);
                if (st.ride[p] == 'x')      add (10, 51, s, 60, 0.6f);
                if (st.perc[p] == 'x')      add (10, 45, s, 60, 0.7f);
            }
            else if (st.bass[p] == 'x')
                add (1, 36 + rootNote + st.bassOct[p], s, (int) (stepTicks * 0.85), 0.9f);
        }
        seq.updateMatchedPairs();
        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (ppq);
        mf.addTrack (seq);
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("SynapseDeck");
        dir.createDirectory();
        auto f = dir.getChildFile (juce::String (st.name) + (drumsFile ? " Drums.mid" : " Bass.mid"));
        f.deleteFile();
        juce::FileOutputStream os (f);
        if (os.openedOk()) { mf.writeTo (os); os.flush(); }
        f.revealToUser();
    }

    SynapseDeckProcessor& proc;
    DeckLook look;
    juce::Image bgImage;
    OrbPad orb;
    EnvView envView;
    juce::TextButton prevB, nextB;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::ComboBox preset, style, root;
    juce::ToggleButton drumsB, bassB, freeB;
    juce::ToggleButton expDrums, expBass;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetA, styleA, rootA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> drumsA, bassA, freeA;
    juce::MidiKeyboardComponent keys;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynapseDeckEditor)
};
