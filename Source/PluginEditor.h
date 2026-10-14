#pragma once
#include "PluginProcessor.h"

struct DeckLook : public juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        auto col = s.findColour (juce::Slider::rotarySliderFillColourId);
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        auto c = b.getCentre();
        g.setColour (juce::Colour (0xaa10081e));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        juce::Path bg, fg;
        bg.addCentredArc (c.x, c.y, r - 2, r - 2, 0, a0, a1, true);
        g.setColour (juce::Colour (0x40ffffff));
        g.strokePath (bg, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float ang = a0 + pos * (a1 - a0);
        fg.addCentredArc (c.x, c.y, r - 2, r - 2, 0, a0, ang, true);
        g.setColour (col);
        g.strokePath (fg, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillEllipse (c.x + (r - 2) * std::sin (ang) - 4, c.y - (r - 2) * std::cos (ang) - 4, 8, 8);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (juce::jmax (10.0f, r * 0.42f), juce::Font::bold));
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

class SynapseDeckEditor : public juce::AudioProcessorEditor
{
public:
    explicit SynapseDeckEditor (SynapseDeckProcessor& p)
        : juce::AudioProcessorEditor (p), proc (p),
          keys (p.keyState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        setLookAndFeel (&look);

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

        keys.setKeyWidth (46.0f);
        keys.setAvailableRange (36, 96);
        keys.setScrollButtonsVisible (false);
        keys.setLowestVisibleKey (48);
        keys.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xfff2eaff));
        keys.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff160c2a));
        keys.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0xaaff9a3c));
        addAndMakeVisible (keys);
        setSize (1040, 640);
    }
    ~SynapseDeckEditor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a0b3a), 0.0f, 0.0f, juce::Colour (0xff04101e), 0.0f, (float) getHeight(), false));
        g.fillAll();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x55ff4fd8), 200.0f, 150.0f, juce::Colours::transparentBlack, 520.0f, 400.0f, true));
        g.fillAll();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x4430e0d0), 860.0f, 330.0f, juce::Colours::transparentBlack, 1100.0f, 600.0f, true));
        g.fillAll();
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
        g.drawText ("SYNAPSE", 28, 14, 170, 40, juce::Justification::left);
        g.setColour (juce::Colour (0xffff9a3c));
        g.drawText ("DECK", 190, 14, 100, 40, juce::Justification::left);
        g.setColour (juce::Colour (0xaaffffff));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("chords / arp / fx / pads  -  psy / goa engine", 30, 52, 400, 16, juce::Justification::left);

        auto panel = [&] (juce::Rectangle<int> r, const char* t)
        {
            g.setColour (juce::Colour (0x8c0a0418)); g.fillRoundedRectangle (r.toFloat(), 14.0f);
            g.setColour (juce::Colour (0x44ffffff)); g.drawRoundedRectangle (r.toFloat(), 14.0f, 1.0f);
            g.setColour (juce::Colour (0xffb070ff)); g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (t, r.getX() + 14, r.getY() + 6, 200, 16, juce::Justification::left);
        };
        panel ({ 20, 84, 1000, 190 }, "SOUND");
        panel ({ 20, 284, 1000, 190 }, "ENVELOPE / ENGINE");
        panel ({ 20, 484, 1000, 140 }, "KEYS");

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        for (auto& k : knobs)
            g.drawText (k->label, k->s.getX() - 10, k->s.getBottom() - 4, k->s.getWidth() + 20, 16, juce::Justification::centred);
    }

    void resized() override
    {
        preset.setBounds (360, 18, 300, 38);
        const char* r1[] = { "cutoff", "reso", "reverb", "width", "volume", "sub", "drive" };
        const char* r2[] = { "attack", "decay", "sustain", "release", "sidechain", "unison", "drumLevel" };
        auto place = [&] (const char** ids, int y)
        {
            for (int i = 0; i < 7; ++i)
                for (auto& k : knobs)
                    if (k->id == ids[i]) k->s.setBounds (50 + i * 135, y, 100, 100);
        };
        place (r1, 112);
        place (r2, 312);
        style.setBounds (30, 436, 200, 30);
        root.setBounds (240, 436, 90, 30);
        drumsB.setBounds (350, 436, 120, 30);
        bassB.setBounds (480, 436, 150, 30);
        freeB.setBounds (640, 436, 120, 30);
        keys.setBounds (32, 506, 976, 106);
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

    SynapseDeckProcessor& proc;
    DeckLook look;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::ComboBox preset, style, root;
    juce::ToggleButton drumsB, bassB, freeB;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetA, styleA, rootA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> drumsA, bassA, freeA;
    juce::MidiKeyboardComponent keys;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynapseDeckEditor)
};
