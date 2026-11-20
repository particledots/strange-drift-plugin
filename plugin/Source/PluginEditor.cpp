#include "PluginEditor.h"
#include "Parameters.h"

#include <cmath>

using namespace DriftStyle;

namespace
{
    juce::Font font (float size) { return juce::Font (juce::FontOptions (size)); }

    juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

    // Draws text with extra space between the letters (for the titles).
    void drawSpaced (juce::Graphics& g, const juce::String& s, float x, float baseline, float size, float spacing)
    {
        auto f = font (size);
        g.setFont (f);
        for (auto ch : s)
        {
            const juce::String one = juce::String::charToString (ch);
            g.drawSingleLineText (one, juce::roundToInt (x), juce::roundToInt (baseline));
            x += juce::GlyphArrangement::getStringWidth (f, one) + spacing;
        }
    }

    void centredText (juce::Graphics& g, const juce::String& s, float cx, float baseline, float size, juce::Colour c)
    {
        g.setColour (c);
        g.setFont (font (size));
        g.drawText (s, juce::Rectangle<float> (cx - 110.0f, baseline - size, 220.0f, size + 4.0f), juce::Justification::centredBottom, false);
    }

    // A round light with a soft halo; level 0 to 1 decides how brightly it shines.
    void drawLight (juce::Graphics& g, float cx, float cy, float radius, juce::Colour colour, double level)
    {
        const float lv = (float) juce::jlimit (0.0, 1.0, level);
        if (lv > 0.15f)
        {
            g.setColour (colour.withAlpha (0.10f * lv));
            g.fillEllipse (cx - radius - 5.0f, cy - radius - 5.0f, 2.0f * (radius + 5.0f), 2.0f * (radius + 5.0f));
            g.setColour (colour.withAlpha (0.18f * lv));
            g.fillEllipse (cx - radius - 3.0f, cy - radius - 3.0f, 2.0f * (radius + 3.0f), 2.0f * (radius + 3.0f));
        }
        g.setColour (knobFill);
        g.fillEllipse (cx - radius, cy - radius, 2.0f * radius, 2.0f * radius);
        g.setColour (colour.withAlpha (lightAlpha (lv)));
        g.fillEllipse (cx - radius, cy - radius, 2.0f * radius, 2.0f * radius);
    }

    const char* kCardNames[3] = { "ripple", "tide", "glacier" };
}

// ================================================================================================
DriftLookAndFeel::DriftLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff383836));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, knobFill.brighter (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, bright);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, panel);
    setColour (juce::Label::textColourId, text);
}

void DriftLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const float size = (float) juce::jmin (w, h) - 4.0f;
    const auto centre = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = size * 0.5f;
    g.setColour (knobFill);
    g.fillEllipse (centre.x - r, centre.y - r, size, size);
    g.setColour (dim);
    g.drawEllipse (centre.x - r, centre.y - r, size, size, 1.5f);

    const float angle = start + pos * (end - start);
    const bool isSpeed = s.getName() == "speed";
    const float len = r * 0.85f;
    const juce::Point<float> tip (centre.x + len * std::sin (angle), centre.y - len * std::cos (angle));
    g.setColour (isSpeed ? amber : bright);
    g.drawLine ({ centre, tip }, isSpeed ? 3.0f : 2.5f);

    if (! isSpeed)                                 // a small mark at the top: the knob's centre (zero)
    {
        g.setColour (line);
        g.fillEllipse (centre.x - 1.5f, centre.y - r - 5.0f, 3.0f, 3.0f);
    }
}

void DriftLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto b = juce::Rectangle<float> (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f);
    g.setColour (box.isMouseOver() ? text.withAlpha (0.8f) : dim);
    g.drawRoundedRectangle (b, 4.0f, 0.8f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (dim);
    g.fillPath (arrow);
}

void DriftLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 20, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

void DriftLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if (b.getToggleState() || (down && ! b.getClickingTogglesState()))
    {
        g.setColour (amber);
        g.fillRoundedRectangle (r, 4.0f);
    }
    else
    {
        g.setColour (over ? text.withAlpha (0.8f) : dim);
        g.drawRoundedRectangle (r, 4.0f, 0.8f);
    }
}

// ================================================================================================
LightsLayer::LightsLayer (DriftProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    setOpaque (false);
    proc.copyTraces (traces, traceCounts);
    startTimerHz (30);
}

void LightsLayer::timerCallback()
{
    if (orbits) proc.copyTraces (traces, traceCounts);

    // Only what has visibly moved is drawn again: a light whose level moved by more than 1/150 of its range (less
    // than half a pixel of its bar), a card whose trace moved on, the read-outs when Speed or Hold changed.
    const float speed = proc.speedNow.load();
    const bool held = proc.held.load();
    const bool all = firstFrame || speed != drawnSpeed || held != drawnHeld || orbits != drawnOrbits;
    std::array<int, strangedrift::kNumOutputs> level {};
    for (int i = 0; i < strangedrift::kNumOutputs; ++i)
        level[(size_t) i] = juce::roundToInt (proc.outputs[(size_t) i].load() * 150.0f);

    auto draw = [this] (juce::Rectangle<int> r) { repaint (r); areaDrawn += (double) r.getWidth() * r.getHeight(); };
    for (int c = 0; c < strangedrift::kNumCircuits; ++c)
    {
        const bool xMoved = level[(size_t) (c * 2)] != drawnLevel[(size_t) (c * 2)];
        const bool yMoved = level[(size_t) (c * 2 + 1)] != drawnLevel[(size_t) (c * 2 + 1)];
        const float traceEnd = traceCounts[(size_t) c] > 0
            ? traces[(size_t) c][(size_t) ((traceCounts[(size_t) c] - 1) * 2)] + 3.0f * traces[(size_t) c][(size_t) ((traceCounts[(size_t) c] - 1) * 2 + 1)] : 0.0f;
        if (all)
            draw (cardArea (c));
        else if (orbits)
        {
            if (xMoved || yMoved || traceEnd != drawnTraceEnd[(size_t) c]) draw (cardArea (c));
        }
        else
        {
            if (xMoved) draw (columnArea (c, 0, 1));
            if (yMoved) draw (columnArea (c, 1, 2));
        }
        drawnTraceEnd[(size_t) c] = traceEnd;
    }
    if (all || level[strangedrift::kOutBlend] != drawnLevel[strangedrift::kOutBlend]) draw (blendArea());
    if (all) draw (speedArea());

    drawnLevel = level;
    drawnSpeed = speed;
    drawnHeld = held;
    drawnOrbits = orbits;
    firstFrame = false;
    ++framesDrawn;
}

void LightsLayer::paintLights (juce::Graphics& g, int card)
{
    const char* labels[3] = { "X", "Y", "Z" };
    const double x = proc.outputs[(size_t) (card * 2)].load(), y = proc.outputs[(size_t) (card * 2 + 1)].load();
    const double values[3] = { x, y, -y };
    for (int col = 0; col < 3; ++col)
    {
        const float cx = (float) columnX (card, col);
        const double v = values[col];
        const auto colour = v >= 0.0 ? amber : blue;
        centredText (g, labels[col], cx, 126.0f, 13.0f, text);
        drawLight (g, cx, 150.0f, 11.0f, colour, std::fabs (v));

        // The level bar: rises above its centre line (amber) or falls below it (blue).
        const juce::Rectangle<float> bar (cx - 6.0f, 170.0f, 12.0f, 110.0f);
        g.setColour (knobFill);
        g.fillRoundedRectangle (bar, 6.0f);
        const float hgt = 55.0f * (float) juce::jlimit (0.0, 1.0, std::fabs (v));
        if (hgt > 0.3f)
        {
            g.setColour (colour.withAlpha (0.85f));
            g.fillRoundedRectangle (v >= 0.0 ? juce::Rectangle<float> (cx - 6.0f, 225.0f - hgt, 12.0f, hgt)
                                             : juce::Rectangle<float> (cx - 6.0f, 225.0f, 12.0f, hgt), juce::jmin (3.0f, hgt * 0.5f));
        }
        g.setColour (dim);
        g.fillRect (cx - 10.0f, 224.6f, 20.0f, 0.8f);

        const juce::String sub = col < 2 ? "CC " + juce::String (strangedrift::coarseCc (card * 2 + col)) : juce::String ("Y flipped");
        centredText (g, sub, cx, 298.0f, 11.0f, dim);
    }
}

void LightsLayer::paintOrbit (juce::Graphics& g, int card)
{
    const juce::Rectangle<float> box ((float) kCardX[card] + 14.0f, 110.0f, (float) kCardW - 28.0f, 192.0f);
    g.setColour (knobFill.withAlpha (0.55f));
    g.fillRoundedRectangle (box, 6.0f);
    g.setColour (line);
    g.fillRect (box.getCentreX() - 0.4f, box.getY() + 6.0f, 0.8f, box.getHeight() - 26.0f);
    g.fillRect (box.getX() + 6.0f, box.getCentreY() - 0.4f, box.getWidth() - 12.0f, 0.8f);

    auto px = [&] (float vx) { return box.getX() + 8.0f + (box.getWidth() - 16.0f) * 0.5f * (vx + 1.0f); };
    auto py = [&] (float vy) { return box.getBottom() - 8.0f - (box.getHeight() - 16.0f) * 0.5f * (vy + 1.0f); };

    const int n = traceCounts[(size_t) card];
    const auto& t = traces[(size_t) card];
    const int chunks = 8;
    for (int ch = 0; ch < chunks && n > 1; ++ch)
    {
        const int a = n * ch / chunks, b = juce::jmin (n - 1, n * (ch + 1) / chunks);
        if (b <= a) continue;
        juce::Path p;
        p.startNewSubPath (px (t[(size_t) (a * 2)]), py (t[(size_t) (a * 2 + 1)]));
        for (int k = a + 1; k <= b; ++k) p.lineTo (px (t[(size_t) (k * 2)]), py (t[(size_t) (k * 2 + 1)]));
        g.setColour (amber.withAlpha (0.10f + 0.70f * (float) (ch + 1) / (float) chunks));
        g.strokePath (p, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const float vx = proc.outputs[(size_t) (card * 2)].load(), vy = proc.outputs[(size_t) (card * 2 + 1)].load();
    drawLight (g, px (vx), py (vy), 5.0f, vx >= 0.0f ? amber : blue, 1.0);

    g.setColour (dim);
    g.setFont (font (11.0f));
    g.drawText (utf8 ("X \xe2\x86\x94   Y \xe2\x86\x95"), juce::Rectangle<float> (box.getX(), 288.0f, box.getWidth(), 14.0f), juce::Justification::centred);
}

void LightsLayer::paint (juce::Graphics& g)
{
    const bool held = proc.held.load();
    const double speed = proc.speedNow.load();

    // Speed read-out.
    centredText (g, std::fabs (speed - 1.0) < 0.005 ? utf8 ("1\xc3\x97 (as the module)") : DriftParams::speedFactorText (speed),
                 70.0f, 176.0f, 12.0f, dim);

    for (int card = 0; card < 3; ++card)
    {
        // The trip time at this speed.
        g.setColour (text);
        g.setFont (font (13.0f));
        const juce::String trip = held ? juce::String ("held") : utf8 ("\xe2\x89\x88 ") + DriftParams::durationText (strangedrift::kTripSeconds[card] / speed);
        g.drawText (trip, juce::Rectangle<float> ((float) kCardX[card] + 80.0f, 82.0f, 101.0f, 18.0f), juce::Justification::centredRight);

        if (orbits) paintOrbit (g, card);
        else paintLights (g, card);
    }

    // Blend + and blend -.
    const double b = proc.outputs[strangedrift::kOutBlend].load();
    const double plus = juce::jmax (0.0, b), minus = juce::jmax (0.0, -b);
    drawLight (g, 55.0f, 338.0f, 11.0f, amber, plus);
    drawLight (g, 115.0f, 338.0f, 11.0f, blue, minus);
    for (int k = 0; k < 2; ++k)
    {
        const float cx = k == 0 ? 55.0f : 115.0f;
        const juce::Rectangle<float> bar (cx - 6.0f, 356.0f, 12.0f, 40.0f);
        g.setColour (knobFill);
        g.fillRoundedRectangle (bar, 6.0f);
        const float hgt = 40.0f * (float) juce::jlimit (0.0, 1.0, k == 0 ? plus : minus);
        if (hgt > 0.3f)
        {
            g.setColour ((k == 0 ? amber : blue).withAlpha (0.85f));
            g.fillRoundedRectangle (k == 0 ? juce::Rectangle<float> (cx - 6.0f, 396.0f - hgt, 12.0f, hgt)
                                           : juce::Rectangle<float> (cx - 6.0f, 356.0f, 12.0f, hgt), juce::jmin (6.0f, hgt * 0.5f));
        }
    }
}

// ================================================================================================
DriftPanel::DriftPanel (DriftProcessor& p)
    : proc (p), lights (p)
{
    setLookAndFeel (&lnf);
    auto& vts = proc.getValueTreeState();

    addAndMakeVisible (lights);                    // first, so the controls sit on top of it

    auto setupKnob = [this] (juce::Slider& k, const juce::String& name, double home)
    {
        k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setRotaryParameters (juce::MathConstants<float>::pi * -0.75f, juce::MathConstants<float>::pi * 0.75f, true);
        k.setPopupDisplayEnabled (true, true, this);
        k.setName (name);
        k.setDoubleClickReturnValue (true, home);
        addAndMakeVisible (k);
    };
    setupKnob (speedKnob, "speed", 0.5);
    speedAtt = std::make_unique<SliderAttachment> (vts, DriftParams::speed, speedKnob);

    for (int c = 0; c < 2; ++c)
    {
        setupKnob (leanKnobs[(size_t) c], "lean", 0.0);
        setupKnob (nudgeKnobs[(size_t) c], "nudge", 0.0);
        leanAtt[(size_t) c]  = std::make_unique<SliderAttachment> (vts, DriftParams::lean[c], leanKnobs[(size_t) c]);
        nudgeAtt[(size_t) c] = std::make_unique<SliderAttachment> (vts, DriftParams::nudge[c], nudgeKnobs[(size_t) c]);

        auto& box = nudgeFromBoxes[(size_t) c];
        const auto& names = DriftParams::nudgeFromNames[c];
        for (int k = 0; k < names.size(); ++k)
            box.addItem ("Nudge from: " + names[k].substring (0, 1).toLowerCase() + names[k].substring (1), k + 1);
        addAndMakeVisible (box);
        nudgeFromAtt[(size_t) c] = std::make_unique<ComboAttachment> (vts, DriftParams::nudgeFrom[c], box);
    }

    for (int ch = 1; ch <= 16; ++ch) channelBox.addItem (juce::String (ch), ch);
    addAndMakeVisible (channelBox);
    channelAtt = std::make_unique<ComboAttachment> (vts, DriftParams::outChannel, channelBox);

    holdButton.setClickingTogglesState (true);
    addAndMakeVisible (holdButton);
    holdAtt = std::make_unique<ButtonAttachment> (vts, DriftParams::hold, holdButton);

    newStartButton.setTooltip ("Jump to a fresh, random place on all three circuits");
    newStartButton.onClick = [this] { proc.requestNewStart(); };
    addAndMakeVisible (newStartButton);

    // Lights | Orbits: one or the other; the choice is kept in the saved state.
    lightsButton.setRadioGroupId (4712);
    orbitsButton.setRadioGroupId (4712);
    lightsButton.setClickingTogglesState (true);
    orbitsButton.setClickingTogglesState (true);
    lightsButton.onClick = [this] { if (lightsButton.getToggleState()) proc.getValueTreeState().state.setProperty (orbitsViewProperty, false, nullptr); };
    orbitsButton.onClick = [this] { if (orbitsButton.getToggleState()) proc.getValueTreeState().state.setProperty (orbitsViewProperty, true, nullptr); };
    addAndMakeVisible (lightsButton);
    addAndMakeVisible (orbitsButton);
    showView ((bool) vts.state.getProperty (orbitsViewProperty, false));
    vts.state.addListener (this);

    setSize (kWidth, kHeight);
}

DriftPanel::~DriftPanel()
{
    proc.getValueTreeState().state.removeListener (this);
    setLookAndFeel (nullptr);
}

void DriftPanel::showView (bool orbits)
{
    lights.setOrbits (orbits);
    lightsButton.setToggleState (! orbits, juce::dontSendNotification);
    orbitsButton.setToggleState (orbits, juce::dontSendNotification);
}

void DriftPanel::valueTreeRedirected (juce::ValueTree& tree)
{
    if (tree == proc.getValueTreeState().state)
        showView ((bool) tree.getProperty (orbitsViewProperty, false));
}

void DriftPanel::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == orbitsViewProperty && tree == proc.getValueTreeState().state)
        showView ((bool) tree.getProperty (orbitsViewProperty, false));
}

std::vector<DriftPanel::ControlInfo> DriftPanel::controls()
{
    return { { &speedKnob, DriftParams::speed }, { &holdButton, DriftParams::hold },
             { &leanKnobs[0], DriftParams::lean[0] }, { &nudgeKnobs[0], DriftParams::nudge[0] }, { &nudgeFromBoxes[0], DriftParams::nudgeFrom[0] },
             { &leanKnobs[1], DriftParams::lean[1] }, { &nudgeKnobs[1], DriftParams::nudge[1] }, { &nudgeFromBoxes[1], DriftParams::nudgeFrom[1] },
             { &channelBox, DriftParams::outChannel } };
}

void DriftPanel::paint (juce::Graphics& g)
{
    g.fillAll (panel);

    // Header
    g.setColour (bright);
    drawSpaced (g, "strange drift", 20.0f, 40.0f, 22.0f, 7.0f);
    g.setColour (dim);
    g.setFont (font (13.0f));
    g.drawText (utf8 ("three slow chaos circuits \xe2\x86\x92 MIDI CC"), juce::Rectangle<int> (getWidth() - 340, 24, 320, 18), juce::Justification::centredRight);
    g.setColour (line);
    g.fillRect (20.0f, 54.0f, (float) getWidth() - 40.0f, 0.6f);

    // Speed
    g.setColour (dim);
    g.setFont (font (11.0f));
    g.drawText (utf8 ("\xe2\x85\x9b\xc3\x97"), juce::Rectangle<float> (28.0f, 137.0f, 30.0f, 14.0f), juce::Justification::centredLeft);
    g.drawText (utf8 ("512\xc3\x97"), juce::Rectangle<float> (82.0f, 137.0f, 30.0f, 14.0f), juce::Justification::centredRight);
    centredText (g, "Speed", 70.0f, 160.0f, 13.0f, text);

    // Blend box
    auto blend = juce::Rectangle<float> (20.5f, 232.5f, 129.0f, 187.0f);
    g.setColour (cardFill);
    g.fillRoundedRectangle (blend, 8.0f);
    g.setColour (line);
    g.drawRoundedRectangle (blend, 8.0f, 1.0f);
    g.setColour (text);
    drawSpaced (g, "blend", 32.0f, 256.0f, 15.0f, 2.5f);
    g.setColour (dim);
    g.setFont (font (12.0f));
    g.drawText ("tide + glacier", juce::Rectangle<float> (32.0f, 264.0f, 112.0f, 16.0f), juce::Justification::centredLeft);
    g.drawText (utf8 ("\xe2\x88\x92 ripple (Z)"), juce::Rectangle<float> (32.0f, 280.0f, 112.0f, 16.0f), juce::Justification::centredLeft);
    centredText (g, "+", 55.0f, 318.0f, 13.0f, text);
    centredText (g, utf8 ("\xe2\x88\x92"), 115.0f, 318.0f, 13.0f, text);
    centredText (g, "CC 112", 85.0f, 412.0f, 11.0f, dim);

    // The three cards
    for (int c = 0; c < 3; ++c)
    {
        auto r = juce::Rectangle<float> ((float) kCardX[c] + 0.5f, (float) kCardY + 0.5f, (float) kCardW - 1.0f, (float) kCardH - 1.0f);
        g.setColour (cardFill);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (line);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
        g.setColour (text);
        drawSpaced (g, kCardNames[c], (float) kCardX[c] + 14.0f, 96.0f, 15.0f, 2.5f);
        if (c < 2)
        {
            centredText (g, "Lean", (float) kCardX[c] + 50.0f, 378.0f, 12.0f, text);
            centredText (g, utf8 ("Nudge \xe2\x88\x92 +"), (float) kCardX[c] + 145.0f, 378.0f, 12.0f, text);
        }
        else
        {
            centredText (g, "no controls:", (float) kCardX[c] + 97.0f, 346.0f, 12.0f, dim);
            centredText (g, "left to wander on its own", (float) kCardX[c] + 97.0f, 364.0f, 12.0f, dim);
        }
    }

    // Footer
    g.setColour (line);
    g.fillRect (20.0f, 432.0f, (float) getWidth() - 40.0f, 0.6f);
    g.setColour (dim);
    g.setFont (font (13.0f));
    g.drawText ("MIDI out: channel", juce::Rectangle<int> (20, 442, 110, 20), juce::Justification::centredLeft);
    g.drawText (utf8 ("\xc2\xb7 14-bit CC \xc2\xb7 your notes pass through"), juce::Rectangle<int> (190, 442, 300, 20), juce::Justification::centredLeft);
    g.drawText ("v" + juce::String (JucePlugin_VersionString), juce::Rectangle<int> (getWidth() - 120, 442, 100, 20), juce::Justification::centredRight);
}

void DriftPanel::resized()
{
    lights.setBounds (getLocalBounds());
    speedKnob.setBounds (36, 74, 68, 68);
    holdButton.setBounds (20, 190, 58, 24);
    newStartButton.setBounds (84, 190, 66, 24);
    for (int c = 0; c < 2; ++c)
    {
        leanKnobs[(size_t) c].setBounds (kCardX[c] + 29, 321, 42, 42);
        nudgeKnobs[(size_t) c].setBounds (kCardX[c] + 124, 321, 42, 42);
        nudgeFromBoxes[(size_t) c].setBounds (kCardX[c] + 14, 388, 167, 22);
    }
    channelBox.setBounds (130, 441, 50, 22);
    lightsButton.setBounds (560, 441, 58, 20);
    orbitsButton.setBounds (622, 441, 58, 20);
}

// ================================================================================================
DriftEditor::DriftEditor (DriftProcessor& p)
    : AudioProcessorEditor (&p), panel (p), proc (p)
{
    addAndMakeVisible (panel);

    constrainer.setFixedAspectRatio ((double) kWidth / (double) kHeight);
    constrainer.setSizeLimits (kMinWidth, heightForWidth (kMinWidth), kMaxWidth, heightForWidth (kMaxWidth));
    setConstrainer (&constrainer);
    setResizable (true, true);                       // with the drag corner

    const int saved = (int) proc.getValueTreeState().state.getProperty (widthProperty, kWidth);
    const int width = juce::jlimit (kMinWidth, kMaxWidth, saved);
    setSize (width, heightForWidth (width));
}

DriftEditor::~DriftEditor() = default;

void DriftEditor::paint (juce::Graphics& g)
{
    g.fillAll (DriftStyle::panel);
}

void DriftEditor::resized()
{
    const double scale = juce::jmin ((double) getWidth() / kWidth, (double) getHeight() / kHeight);
    panel.setTransform (juce::AffineTransform::scale ((float) scale));
    panel.setBounds (0, 0, kWidth, kHeight);

    // Keep the chosen size with the project.
    auto& state = proc.getValueTreeState().state;
    if ((int) state.getProperty (widthProperty, -1) != getWidth())
        state.setProperty (widthProperty, getWidth(), nullptr);
}
