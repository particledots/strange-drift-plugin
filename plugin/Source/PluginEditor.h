// The Strange Drift window, as approved in the draft (2026-11-19): Speed, Hold, New start and the blend outputs on the
// left; the three circuit cards (ripple, tide, glacier), each with glowing X, Y, Z lights and level bars, Lean, Nudge
// and Nudge from on ripple and tide; the MIDI out channel, Lights | Orbits and the version at the bottom.
// The panel is drawn at its design size (780 x 470) and the editor scales it (80% to 200%), as on Ogdoad.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "PluginProcessor.h"

namespace DriftStyle
{
    inline const juce::Colour panel     { 0xff2c2c2a };
    inline const juce::Colour cardFill  { 0xff363634 };
    inline const juce::Colour knobFill  { 0xff444441 };
    inline const juce::Colour line      { 0xff5f5e5a };
    inline const juce::Colour dim       { 0xff888780 };
    inline const juce::Colour text      { 0xffd3d1c7 };
    inline const juce::Colour bright    { 0xfff1efe8 };
    inline const juce::Colour amber     { 0xfffac775 };
    inline const juce::Colour blue      { 0xff378add };

    // How strongly a light shines for a level from 0 to 1: a faint glow at 0, clearly lit already at 30%, full at 1
    // (Ogdoad 0.2.1's curve).
    inline float lightAlpha (double level)
    {
        return 0.10f + 0.90f * (float) std::pow (juce::jlimit (0.0, 1.0, level), 0.6);
    }

    // The three cards: left edge of each (design coordinates), and the centres of their X, Y, Z columns.
    inline constexpr int kCardX[3] = { 165, 365, 565 };
    inline constexpr int kCardY = 70, kCardW = 195, kCardH = 350;
    inline int columnX (int card, int col) { return kCardX[card] + 40 + 57 * col; }
}

class DriftLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DriftLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (12.5f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions (h < 22 ? 12.0f : 13.0f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (13.0f)); }
};

// Everything that moves: the lights and bars (or the Orbits traces), the trip times, the Speed read-out and the
// blend lights. Drawn over the panel, under the controls, and never takes mouse clicks.
class LightsLayer : public juce::Component, private juce::Timer
{
public:
    explicit LightsLayer (DriftProcessor& p);
    ~LightsLayer() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void setOrbits (bool shouldShowOrbits) { if (orbits != shouldShowOrbits) { orbits = shouldShowOrbits; repaint(); } }
    bool isOrbits() const { return orbits; }

private:
    void timerCallback() override;
    void paintLights (juce::Graphics&, int card);
    void paintOrbit (juce::Graphics&, int card);
    DriftProcessor& proc;
    bool orbits = false;
    std::array<std::array<float, strangedrift::Engine::kTracePoints * 2>, strangedrift::kNumCircuits> traces {};
    std::array<int, strangedrift::kNumCircuits> traceCounts {};

    // What was last drawn, so only the parts that moved are drawn again (0.1.1: lighter on the Intel Mac).
    std::array<int, strangedrift::kNumOutputs> drawnLevel {};
    std::array<float, strangedrift::kNumCircuits> drawnTraceEnd {};
    float drawnSpeed = -1.0f;
    bool drawnHeld = false, drawnOrbits = false, firstFrame = true;
public:
    static juce::Rectangle<int> cardArea (int card) { return { DriftStyle::kCardX[card] + 4, 76, DriftStyle::kCardW - 8, 232 }; }
    static juce::Rectangle<int> blendArea()         { return { 24, 320, 122, 80 }; }
    static juce::Rectangle<int> speedArea()         { return { 8, 162, 124, 18 }; }
    // One column of lights (Lights view): the light, its bar and labels. Y's column also covers Z.
    static juce::Rectangle<int> columnArea (int card, int col, int cols) { return { DriftStyle::columnX (card, col) - 28, 108, 57 * cols - 1, 196 }; }
    int framesDrawn = 0;
    double areaDrawn = 0.0;                  // for the offline checks: total area drawn again, in panel pixels
};

// The whole panel at its design size (780 x 470). The editor below scales it to the window.
class DriftPanel : public juce::Component, private juce::ValueTree::Listener
{
public:
    explicit DriftPanel (DriftProcessor&);
    ~DriftPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // For the offline checks: every control and the parameter it moves.
    struct ControlInfo { juce::Component* control; juce::String paramId; };
    std::vector<ControlInfo> controls();

    // Lights | Orbits: what the three cards show. Saved with the project (not an audio parameter).
    juce::TextButton lightsButton { "Lights" }, orbitsButton { "Orbits" };
    juce::TextButton newStartButton { "New start" };
    bool isOrbitsView() const { return lights.isOrbits(); }
    const LightsLayer& lightsLayer() const { return lights; }
    static inline const juce::Identifier orbitsViewProperty { "orbitsView" };

    static constexpr int kWidth = 780, kHeight = 470;      // the design size; everything is laid out at this size

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeRedirected (juce::ValueTree&) override;
    void showView (bool orbits);

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    DriftProcessor& proc;
    DriftLookAndFeel lnf;
    LightsLayer lights;

    juce::Slider speedKnob;
    std::array<juce::Slider, 2> leanKnobs, nudgeKnobs;
    std::array<juce::ComboBox, 2> nudgeFromBoxes;
    juce::ComboBox channelBox;
    juce::TextButton holdButton { "Hold" };

    std::unique_ptr<SliderAttachment> speedAtt;
    std::array<std::unique_ptr<SliderAttachment>, 2> leanAtt, nudgeAtt;
    std::array<std::unique_ptr<ComboAttachment>, 2> nudgeFromAtt;
    std::unique_ptr<ComboAttachment> channelAtt;
    std::unique_ptr<ButtonAttachment> holdAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriftPanel)
};

// The plugin window: resizable between 80% and 200% of the design size, proportions locked. The panel is drawn at its
// design size and scaled to fit, so every control, light and letter grows together. The width is saved with the project.
class DriftEditor : public juce::AudioProcessorEditor
{
public:
    explicit DriftEditor (DriftProcessor&);
    ~DriftEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // For the offline checks (they forward to the panel).
    std::vector<DriftPanel::ControlInfo> controls() { return panel.controls(); }
    bool isOrbitsView() const { return panel.isOrbitsView(); }

    static constexpr int kWidth = DriftPanel::kWidth, kHeight = DriftPanel::kHeight;
    static constexpr int kMinWidth = 624, kMaxWidth = 1560;     // 80% and 200%
    static inline const juce::Identifier widthProperty { "editorWidth" };
    static int heightForWidth (int width) { return juce::roundToInt ((double) width * kHeight / kWidth); }

    DriftPanel panel;

private:
    DriftProcessor& proc;
    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriftEditor)
};
