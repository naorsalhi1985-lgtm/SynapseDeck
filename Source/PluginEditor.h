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
        return { b.getX() + 60.0f + cx * (b.getWidth() - 120.0f), b.getY() + 42.0f + (1.0f - ry) * (b.getHeight() - 84.0f) };
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0x44ffffff));
        for (float x = b.getX() + 14.0f; x < b.getRight() - 14.0f; x += 9.0f)
            g.fillRect (x, b.getBottom() - 30.0f, 4.0f, 1.2f);
        auto p = orbPos();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x66ff4fd8), p.x, p.y, juce::Colours::transparentBlack, p.x + 90.0f, p.y, true));
        g.fillEllipse (p.x - 90.0f, p.y - 90.0f, 180.0f, 180.0f);
        for (int i = 0; i < 48; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 48.0f;
            g.setColour (juce::Colour (0xff30e0d0).withAlpha (i % 2 ? 0.9f : 0.5f));
            g.fillEllipse (p.x + 40.0f * std::cos (a) - 1.6f, p.y + 40.0f * std::sin (a) - 1.6f, 3.2f, 3.2f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff8fe0), p.x - 12.0f, p.y - 14.0f, juce::Colour (0xff6a2fd0), p.x + 30.0f, p.y + 30.0f, true));
        g.fillEllipse (p.x - 32.0f, p.y - 32.0f, 64.0f, 64.0f);
        g.setColour (juce::Colour (0x99ffffff));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("drag: cutoff  /  reso", getLocalBounds().removeFromBottom (24), juce::Justification::centred);
    }
    void setFromMouse (const juce::MouseEvent& e)
    {
        auto b = getLocalBounds().toFloat();
        const float cx = juce::jlimit (0.0f, 1.0f, (e.position.x - b.getX() - 60.0f) / (b.getWidth() - 120.0f));
        const float ry = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - b.getY() - 42.0f) / (b.getHeight() - 84.0f));
        apvts.getParameter ("cutoff")->setValueNotifyingHost (cx);
        apvts.getParameter ("reso")->setValueNotifyingHost (ry);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        apvts.getParameter ("cutoff")->beginChangeGesture(); apvts.getParameter ("reso")->beginChangeGesture(); setFromMouse (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { setFromMouse (e); }
    void mouseUp (const juce::MouseEvent&) override
    {
        apvts.getParameter ("cutoff")->endChangeGesture(); apvts.getParameter ("reso")->endChangeGesture();
    }
    juce::AudioProcessorValueTreeState& apvts;
};

struct EnvView : public juce::Component, private juce::Timer
{
    explicit EnvView (juce::AudioProcessorValueTreeState& s) : apvts (s) { startTimerHz (20); }
    void timerCallback() override { repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (6.0f, 18.0f);
        const float a = *apvts.getRawParameterValue ("attack"), d = *apvts.getRawParameterValue ("decay"),
                    s = *apvts.getRawParameterValue ("sustain"), r = *apvts.getRawParameterValue ("release");
        const float hold = 0.6f, total = a + d + hold + r;
        const float x0 = b.getX(), x1 = x0 + b.getWidth() * a / total, x2 = x1 + b.getWidth() * d / total,
                    x3 = x2 + b.getWidth() * hold / total, x4 = b.getRight();
        const float yb = b.getBottom(), yt = b.getY(), ys = yb - s * (yb - yt);
        juce::Path path;
        path.startNewSubPath (x0, yb); path.lineTo (x1, yt); path.lineTo (x2, ys); path.lineTo (x3, ys); path.lineTo (x4, yb);
        juce::Path fillP (path); fillP.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x887a40ff), 0.0f, yt, juce::Colour (0x112020a0), 0.0f, yb, false));
        g.fillPath (fillP);
        g.setColour (juce::Colour (0xffff9a3c));
        g.strokePath (path, juce::PathStrokeType (2.2f));
        g.setColour (juce::Colour (0xaaffffff));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("A " + juce::String ((int) (a * 1000.0f)) + " MS   R " + juce::String (r, 2) + " S",
                    getLocalBounds().removeFromTop (16), juce::Justification::right);
    }
    juce::AudioProcessorValueTreeState& apvts;
};

struct PadButton : public juce::Button
{
    PadButton (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text, juce::uint32 c)
        : juce::Button (text), col (c)
    {
        auto* prm = s.getParameter (id);
        attach = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v)
        {
            setToggleState (v > 0.5f, juce::dontSendNotification);
            repaint();
        });
        attach->sendInitialUpdate();
    }
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.5f);
        const bool on = getToggleState();
        g.setColour (on ? col.withAlpha (0.95f) : col.withAlpha (over ? 0.5f : 0.32f));
        g.fillRoundedRectangle (b, 9.0f);
        g.setColour (juce::Colour (0x66ffffff));
        g.drawRoundedRectangle (b, 9.0f, 1.2f);
        g.setColour (on ? juce::Colours::black : juce::Colours::white);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        juce::Button::mouseDown (e);
        attach->beginGesture();
        attach->setValueAsPartOfGesture (1.0f);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        attach->setValueAsPartOfGesture (0.0f);
        attach->endGesture();
        juce::Button::mouseUp (e);
    }
    juce::Colour col;
    std::unique_ptr<juce::ParameterAttachment> attach;
};

struct ChordButton : public juce::Component
{
    ChordButton (SynapseDeckProcessor& p, int d) : proc (p), deg (d) {}

    std::vector<int> chordNotes() const
    {
        static const int mn[7] = { 0, 2, 3, 5, 7, 8, 10 };
        static const int mj[7] = { 0, 2, 4, 5, 7, 9, 11 };
        const int root = (int) proc.apvts.getRawParameterValue ("bassRoot")->load();
        const bool minor = proc.apvts.getRawParameterValue ("scaleMinor")->load() > 0.5f;
        const int oct = (int) proc.apvts.getRawParameterValue ("octave")->load();
        const int* sc = minor ? mn : mj;
        const int base = 12 * (oct + 1) + root;
        std::vector<int> out;
        for (int k = 0; k < 3; ++k)
        {
            const int idx = deg + 2 * k;
            out.push_back (base + sc[idx % 7] + 12 * (idx / 7));
        }
        return out;
    }
    void refresh()
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const auto n = chordNotes();
        const int i1 = n[1] - n[0], i2 = n[2] - n[1];
        juce::String l = juce::String (names[((n[0] % 12) + 12) % 12]);
        if (i1 == 3 && i2 == 4) l << "m";
        else if (i1 == 3 && i2 == 3) l << juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
        if (l != label) { label = l; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.5f);
        g.setColour (down ? juce::Colour (0xff30e0d0) : juce::Colour (0xaa0d3a40));
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (juce::Colour (0x66ffffff));
        g.drawRoundedRectangle (b, 10.0f, 1.2f);
        g.setColour (down ? juce::Colours::black : juce::Colours::white);
        g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
        g.drawText (label, getLocalBounds(), juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent&) override
    {
        heldNotes = chordNotes();
        for (int n : heldNotes) proc.keyState.noteOn (1, juce::jlimit (0, 127, n), 0.8f);
        down = true; repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        for (int n : heldNotes) proc.keyState.noteOff (1, juce::jlimit (0, 127, n), 0.0f);
        heldNotes.clear();
        down = false; repaint();
    }
    SynapseDeckProcessor& proc;
    int deg;
    bool down = false;
    juce::String label;
    std::vector<int> heldNotes;
};

class SynapseDeckEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SynapseDeckEditor (SynapseDeckProcessor& p)
        : juce::AudioProcessorEditor (p), proc (p), orb (p.apvts), envView (p.apvts),
          keys (p.keyState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        setLookAndFeel (&look);
        bgImage = juce::ImageFileFormat::loadFrom (BgData::data, (size_t) BgData::size);

        addKnob ("cutoff", "CUTOFF", 0xffffd21f, " Hz");
        addKnob ("reso", "RESO", 0xffff5fa8, "");
        addKnob ("reverb", "REVERB", 0xffff4fd8, "");
        addKnob ("width", "WIDTH", 0xffa070ff, " ct");
        addKnob ("volume", "VOLUME", 0xfff0e040, "");
        addKnob ("attack", "ATTACK", 0xffa070ff, " s");
        addKnob ("decay", "DECAY", 0xffff5f90, " s");
        addKnob ("sustain", "SUSTAIN", 0xffff5f90, "");
        addKnob ("release", "RELEASE", 0xffff9a3c, " s");
        addKnob ("sub", "SUB", 0xff30e0d0, "");
        addKnob ("drive", "DRIVE", 0xffffd21f, "");
        addKnob ("sidechain", "SIDECHAIN", 0xff5fd0ff, "");
        addKnob ("unison", "VOICES", 0xffa0ff70, "");
        addKnob ("drumLevel", "DRUM LVL", 0xffff7040, "");

        addCombo (preset, presetA, "preset");
        addCombo (style, styleA, "style");
        addCombo (root, rootA, "bassRoot");
        addToggle (drumsB, drumsA, "drums", "DRUMS");
        addToggle (bassB, bassA, "bass", "ROLLING BASS");
        addToggle (freeB, freeA, "freeRun", "FREE RUN");
        addToggle (melodyB, melodyA, "melodyOn", "MELODY");
        addToggle (fillsB, fillsA, "fills", "DRUM FILLS");
        addCombo (melScale, melScaleA, "melodyScale");
        genB.setButtonText ("GENERATE");
        genB.onClick = [this]
        {
            auto* prm = proc.apvts.getParameter ("melodySeed");
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) juce::Random::getSystemRandom().nextInt (10000)));
            prm->endChangeGesture();
        };
        addAndMakeVisible (genB);
        densS.setSliderStyle (juce::Slider::LinearHorizontal);
        densS.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        densS.setColour (juce::Slider::trackColourId, juce::Colour (0xffff9a3c));
        densS.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        densA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "melodyDensity", densS);
        addAndMakeVisible (densS);
        expMel.setButtonText ("EXPORT MELODY MIDI");
        expMel.onClick = [this] { exportMelody(); };
        addAndMakeVisible (expMel);
        expDrums.setButtonText ("EXPORT DRUMS MIDI");
        expBass.setButtonText ("EXPORT BASS MIDI");
        expDrums.onClick = [this] { exportMidi (true); };
        expBass.onClick  = [this] { exportMidi (false); };
        addAndMakeVisible (expDrums);
        addAndMakeVisible (expBass);

        keys.setKeyWidth (46.0f);
        keys.setAvailableRange (36, 96);
        keys.setScrollButtonsVisible (false);
        keys.setLowestVisibleKey (48);
        keys.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xfff2eaff));
        keys.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff160c2a));
        keys.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0xaaff9a3c));
        addAndMakeVisible (keys);
        addAndMakeVisible (orb);
        addAndMakeVisible (envView);
        prevB.setButtonText ("<"); nextB.setButtonText (">");
        prevB.onClick = [this] { int n = preset.getSelectedId() - 1; if (n < 1) n = preset.getNumItems(); preset.setSelectedId (n); };
        nextB.onClick = [this] { int n = preset.getSelectedId() + 1; if (n > preset.getNumItems()) n = 1; preset.setSelectedId (n); };
        addAndMakeVisible (prevB); addAndMakeVisible (nextB);

        for (int d = 0; d < 7; ++d)
        {
            chords.push_back (std::make_unique<ChordButton> (p, d));
            addAndMakeVisible (*chords.back());
        }
        struct FxDef { const char* id; const char* text; juce::uint32 col; };
        const FxDef fx[] = { { "fxPump", "PUMP", 0xffe08a30 }, { "fxStutter", "STUTTER", 0xff30c0b0 }, { "fxTrance", "TRANCE GATE", 0xffa0b030 },
                             { "fxTape", "TAPE STOP", 0xffe04050 }, { "fxPitchUp", "PITCH UP", 0xffd060d0 }, { "fxPitchDn", "PITCH DN", 0xff5080e0 },
                             { "fxLofi", "LOFI", 0xffe0a030 }, { "fxPhone", "PHONE", 0xff30d0c0 }, { "fxSpace", "SPACE", 0xff6070e0 },
                             { "fxFlange", "FLANGE", 0xff90d040 }, { "fxMute", "MUTE ALL", 0xffe05060 } };
        for (auto& f : fx)
        {
            pads.push_back (std::make_unique<PadButton> (p.apvts, f.id, f.text, f.col));
            addAndMakeVisible (*pads.back());
        }
        addToggle (scaleB, scaleA, "scaleMinor", "MINOR");
        addToggle (arpB, arpOnA, "arpOn", "ARP");
        addCombo (arpRate, arpRateA, "arpRate");
        addCombo (arpPat, arpPatA, "arpPattern");
        addCombo (arpRange, arpRangeA, "arpRange");
        octDn.setButtonText ("-"); octUp.setButtonText ("+");
        auto stepOct = [this] (int dlt)
        {
            auto* prm = proc.apvts.getParameter ("octave");
            const int cur = (int) proc.apvts.getRawParameterValue ("octave")->load();
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) juce::jlimit (1, 6, cur + dlt)));
            prm->endChangeGesture();
        };
        octDn.onClick = [stepOct] { stepOct (-1); };
        octUp.onClick = [stepOct] { stepOct (1); };
        addAndMakeVisible (octDn); addAndMakeVisible (octUp);
        octL.setJustificationType (juce::Justification::centred);
        octL.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (octL);
        startTimerHz (8);
        timerCallback();
        setSize (1200, 770);
    }
    ~SynapseDeckEditor() override { setLookAndFeel (nullptr); }

    void timerCallback() override
    {
        for (auto& c : chords) c->refresh();
        const bool minor = proc.apvts.getRawParameterValue ("scaleMinor")->load() > 0.5f;
        const juce::String t = minor ? "MINOR" : "MAJOR";
        if (scaleB.getButtonText() != t) scaleB.setButtonText (t);
        const juce::String o = "OCT " + juce::String ((int) proc.apvts.getRawParameterValue ("octave")->load());
        if (octL.getText() != o) octL.setText (o, juce::dontSendNotification);
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
        panel ({ 20, 84, 580, 206 }, "SYNTH");
        panel ({ 620, 84, 560, 206 }, "ENVELOPE / ARPEGGIATOR");
        panel ({ 20, 296, 1160, 116 }, "SOUND");
        panel ({ 20, 504, 1160, 54 }, "");
        panel ({ 20, 566, 1160, 144 }, "KEYS");

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
        orb.setBounds (30, 104, 560, 150);
        scaleB.setBounds (40, 256, 100, 28);
        octDn.setBounds (156, 256, 30, 28);
        octL.setBounds (190, 256, 60, 28);
        octUp.setBounds (254, 256, 30, 28);
        envView.setBounds (630, 104, 540, 112);
        arpB.setBounds (640, 232, 110, 34);
        arpRate.setBounds (762, 232, 90, 34);
        arpPat.setBounds (864, 232, 100, 34);
        arpRange.setBounds (976, 232, 100, 34);
        const char* ids[] = { "cutoff", "reso", "reverb", "width", "volume", "sub", "drive",
                              "attack", "decay", "sustain", "release", "sidechain", "unison", "drumLevel" };
        for (int i = 0; i < 14; ++i)
            for (auto& k : knobs)
                if (k->id == ids[i]) k->s.setBounds (30 + i * 82, 312, 80, 80);
        for (int i = 0; i < 7; ++i) chords[(size_t) i]->setBounds (20 + i * 166, 420, 158, 34);
        for (int i = 0; i < 11; ++i) pads[(size_t) i]->setBounds (20 + i * 106, 460, 100, 38);
        style.setBounds (34, 514, 220, 34);
        root.setBounds (264, 514, 90, 34);
        drumsB.setBounds (370, 514, 130, 34);
        bassB.setBounds (510, 514, 160, 34);
        freeB.setBounds (680, 514, 120, 34);
        expDrums.setBounds (830, 514, 160, 34);
        expBass.setBounds (1000, 514, 160, 34);
        melodyB.setBounds (34, 560, 120, 34);
        genB.setBounds (164, 560, 120, 34);
        melScale.setBounds (294, 560, 210, 34);
        densS.setBounds (516, 560, 180, 34);
        fillsB.setBounds (710, 560, 130, 34);
        expMel.setBounds (850, 560, 200, 34);
        keys.setBounds (32, 612, 1136, 126);
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

    void exportMelody()
    {
        auto& av = proc.apvts;
        const int tab = [&] { const int sc = (int) *av.getRawParameterValue ("melodyScale");
                              return sc == 1 ? 2 : sc == 2 ? 3 : (*av.getRawParameterValue ("scaleMinor") > 0.5f ? 0 : 1); }();
        const int base = 12 * ((int) *av.getRawParameterValue ("octave") + 2) + (int) *av.getRawParameterValue ("bassRoot");
        const int styleIdx = juce::jlimit (0, 11, (int) *av.getRawParameterValue ("style"));
        const int ppq = 480, stepTicks = ppq / 4, bars = 16;
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / kStyles[styleIdx].bpm)), 0.0);
        for (int s = 0; s < bars * 16; ++s)
        {
            const int d = proc.melPat[s % 64];
            if (d < 0) continue;
            const int note = juce::jlimit (0, 127, base + SynapseDeckProcessor::kScaleTab[tab][d % 7] + 12 * (d / 7));
            const double t = (double) (s * stepTicks);
            seq.addEvent (juce::MidiMessage::noteOn (1, note, (s % 4 == 0) ? 0.85f : 0.68f), t);
            seq.addEvent (juce::MidiMessage::noteOff (1, note), t + stepTicks * 0.7);
        }
        seq.updateMatchedPairs();
        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (ppq);
        mf.addTrack (seq);
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("SynapseDeck");
        dir.createDirectory();
        auto f = dir.getChildFile ("Melody " + juce::String (proc.melSeed) + ".mid");
        f.deleteFile();
        juce::FileOutputStream os (f);
        if (os.openedOk()) { mf.writeTo (os); os.flush(); }
        f.revealToUser();
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
    juce::TextButton prevB, nextB, octDn, octUp;
    juce::Label octL;
    std::vector<std::unique_ptr<ChordButton>> chords;
    std::vector<std::unique_ptr<PadButton>> pads;
    juce::ToggleButton scaleB, arpB;
    juce::ComboBox arpRate, arpPat, arpRange;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> scaleA, arpOnA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> arpRateA, arpPatA, arpRangeA;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::ComboBox preset, style, root;
    juce::ToggleButton drumsB, bassB, freeB;
    juce::ToggleButton expDrums, expBass, melodyB, fillsB;
    juce::TextButton genB, expMel;
    juce::ComboBox melScale;
    juce::Slider densS;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> melodyA, fillsA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> melScaleA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> densA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetA, styleA, rootA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> drumsA, bassA, freeA;
    juce::MidiKeyboardComponent keys;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynapseDeckEditor)
};
