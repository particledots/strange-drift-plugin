// Offline window picture and plugin checks (not part of the plugin; only built with -DSTRANGEDRIFT_BUILD_PREVIEW=ON).
//   1. draws the window to picture files (default, running, held, Orbits view, 512x, three sizes);
//   2. checks that all 9 ids exist, each has exactly one control, and each control moves exactly its own parameter;
//   3. checks saving and reloading: every setting, the chaos position, the Lights | Orbits choice and the window size;
//   4. checks the MIDI: notes and other messages (Ogdoad's CC included) pass through untouched, the outputs come out as
//      CC 106-119 on the chosen channel, the mod wheel nudges, bypass adds nothing, Hold, New start, and Logic's block
//      size makes no difference.
// Usage: StrangeDriftPreview <folder for the pictures>

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <iostream>
#include <map>
#include <set>

namespace
{
    int checks = 0, failures = 0;

    void expect (bool ok, const juce::String& what)
    {
        ++checks;
        if (! ok) { ++failures; std::cout << "FAIL: " << what << std::endl; }
    }

    juce::RangedAudioParameter* param (juce::AudioProcessor& p, const juce::String& id)
    {
        for (auto* x : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (x))
                if (r->paramID == id) return r;
        return nullptr;
    }

    void setParam (juce::AudioProcessor& p, const juce::String& id, float real)
    {
        if (auto* r = param (p, id)) r->setValueNotifyingHost (r->convertTo0to1 (real));
        else expect (false, "no parameter " + id);
    }

    std::map<juce::String, float> snapshot (juce::AudioProcessor& p)
    {
        std::map<juce::String, float> m;
        for (auto* x : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (x)) m[r->paramID] = r->getValue();
        return m;
    }

    void snapshotTo (juce::Component& c, const juce::File& file)
    {
        auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 2.0f);
        file.deleteFile();
        juce::FileOutputStream out (file);
        if (out.openedOk()) { juce::PNGImageFormat png; png.writeImageToStream (image, out); }
    }

    struct Msg { long long sample; juce::MidiMessage m; };

    std::vector<Msg> runBlocks (DriftProcessor& p, double seconds, int block, const std::vector<Msg>& in = {}, bool bypassed = false)
    {
        std::vector<Msg> out;
        juce::AudioBuffer<float> audio (0, 8192);
        long long pos = 0, total = (long long) (seconds * 48000.0);
        juce::Random rng (5);
        while (pos < total)
        {
            int n = block > 0 ? block : 1 + rng.nextInt (1024);
            n = (int) juce::jmin<long long> (n, total - pos);
            audio.setSize (0, n, false, false, true);
            juce::MidiBuffer midi;
            for (auto& e : in)
                if (e.sample >= pos && e.sample < pos + n) midi.addEvent (e.m, (int) (e.sample - pos));
            if (bypassed) p.processBlockBypassed (audio, midi); else p.processBlock (audio, midi);
            for (const auto meta : midi) out.push_back ({ pos + meta.samplePosition, meta.getMessage() });
            pos += n;
        }
        return out;
    }

    bool isOurCc (const juce::MidiMessage& m)
    {
        return m.isController() && m.getControllerNumber() >= 106 && m.getControllerNumber() <= 119;
    }

    bool sameStream (const std::vector<Msg>& a, const std::vector<Msg>& b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].sample != b[i].sample || a[i].m.getDescription() != b[i].m.getDescription()) return false;
        return true;
    }

    const char* allIds[] = { "speed", "hold", "lean1", "nudge1", "nudge1From", "lean2", "nudge2", "nudge2From", "outChannel" };
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    // ---- 1 and 2: the window and its controls ------------------------------------------------------------------
    {
        DriftProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        proc.seedForChecks (2026);
        expect (proc.getParameters().size() == 9, "9 parameters");
        for (auto id : allIds)
            expect (param (proc, id) != nullptr, juce::String ("parameter id ") + id);
        expect (proc.isMidiEffect() && proc.acceptsMidi() && proc.producesMidi(), "a MIDI effect");
        expect (proc.getTotalNumInputChannels() == 0 && proc.getTotalNumOutputChannels() == 0, "no audio channels");
        expect (param (proc, "speed")->getCurrentValueAsText() == juce::String::fromUTF8 ("1\xc3\x97"), "Speed starts at 1x (" + param (proc, "speed")->getCurrentValueAsText() + ")");
        expect (param (proc, "lean1")->getCurrentValueAsText() == "0%" && param (proc, "nudge2")->getCurrentValueAsText() == "0%", "Lean and Nudge start centred");
        expect (param (proc, "nudge1From")->getCurrentValueAsText() == "Off" && param (proc, "nudge2From")->getCurrentValueAsText() == "Off", "Nudge from starts on Off");
        expect (juce::roundToInt (param (proc, "outChannel")->convertFrom0to1 (param (proc, "outChannel")->getValue())) == 15, "MIDI out starts on channel 15");
        using DriftParams::durationText;
        expect (durationText (20.0) == "20 s" && durationText (75.0) == "75 s" && durationText (150.0) == "2.5 min" && durationText (2100.0) == "35 min"
                && durationText (16800.0) == "4.7 h" && durationText (2100.0 / 512.0) == "4.1 s" && durationText (20.0 / 512.0) == "0.04 s",
                "trip read-outs are short (" + durationText (75.0) + ", " + durationText (16800.0) + ", " + durationText (20.0 / 512.0) + ")");
        param (proc, "speed")->setValueNotifyingHost (1.0f);
        expect (param (proc, "speed")->getCurrentValueAsText() == juce::String::fromUTF8 ("512\xc3\x97"), "Speed fully clockwise reads 512x");
        param (proc, "speed")->setValueNotifyingHost (0.0f);
        expect (param (proc, "speed")->getCurrentValueAsText() == juce::String::fromUTF8 ("0.125\xc3\x97"), "Speed fully anticlockwise reads 0.125x");
        param (proc, "speed")->setValueNotifyingHost (0.5f);

        std::unique_ptr<DriftEditor> ed (dynamic_cast<DriftEditor*> (proc.createEditor()));
        expect (ed != nullptr, "editor opens");
        expect (ed->getWidth() == DriftEditor::kWidth && ed->getHeight() == DriftEditor::kHeight, "window size 780 x 470");

        runBlocks (proc, 0.2, 512);
        snapshotTo (*ed, outDir.getChildFile ("strangedrift-window-default.png"));

        auto controls = ed->controls();
        std::set<juce::String> ids;
        for (auto& c : controls) ids.insert (c.paramId);
        expect (controls.size() == 9 && ids.size() == 9, "every parameter has exactly one control");
        for (auto& c : controls)
        {
            auto before = snapshot (proc);
            if (auto* s = dynamic_cast<juce::Slider*> (c.control)) s->setValue (s->getMinimum() + (s->getMaximum() - s->getMinimum()) * 0.83, juce::sendNotificationSync);
            else if (auto* b = dynamic_cast<juce::ComboBox*> (c.control)) b->setSelectedItemIndex (2, juce::sendNotificationSync);
            else if (auto* t = dynamic_cast<juce::Button*> (c.control)) t->setToggleState (! t->getToggleState(), juce::sendNotificationSync);
            auto after = snapshot (proc);
            int changed = 0; juce::String which;
            for (auto& kv : after) if (kv.second != before[kv.first]) { ++changed; which = kv.first; }
            expect (changed == 1 && which == c.paramId, "control for " + c.paramId + " moves only " + c.paramId + " (moved " + which + ")");
            const auto inWindow = ed->getLocalArea (c.control, c.control->getLocalBounds());
            bool visible = true;
            for (auto* x = c.control; x != nullptr && x != ed.get(); x = x->getParentComponent()) visible = visible && x->isVisible();
            expect (visible && ! inWindow.isEmpty() && ed->getLocalBounds().contains (inWindow), "control for " + c.paramId + " is visible inside the window");
        }
        for (auto* x : proc.getParameters()) x->setValueNotifyingHost (x->getDefaultValue());

        // Menu text as drawn.
        std::vector<juce::ComboBox*> boxes;
        for (auto& c : ed->controls()) if (auto* b = dynamic_cast<juce::ComboBox*> (c.control)) boxes.push_back (b);
        expect (boxes.size() == 3 && boxes[0]->getText() == "Nudge from: off" && boxes[1]->getText() == "Nudge from: off" && boxes[2]->getText() == "15",
                "menus read 'Nudge from: off' twice and '15'");
        setParam (proc, "nudge1From", 4.0f);
        setParam (proc, "nudge2From", 2.0f);
        expect (boxes[0]->getText() == "Nudge from: glacier X" && boxes[1]->getText() == "Nudge from: ripple X",
                "ripple's menu offers the other two circuits (glacier X), tide's too (ripple X)");
        expect (DriftParams::nudgeFromNames[0].joinIntoString (",") == "Off,Mod wheel,Tide X,Tide Y,Glacier X,Glacier Y"
                && DriftParams::nudgeFromNames[1].joinIntoString (",") == "Off,Mod wheel,Ripple X,Ripple Y,Glacier X,Glacier Y",
                "Nudge from menus in their fixed order, never the circuit itself");

        // The approved example settings for the pictures: ripple nudged from glacier X, tide leaning left.
        setParam (proc, "nudge2From", 0.0f);
        setParam (proc, "nudge1", 0.35f);
        setParam (proc, "lean2", -0.4f);
        runBlocks (proc, 3.0, 512);
        snapshotTo (*ed, outDir.getChildFile ("strangedrift-window-running.png"));
        setParam (proc, "hold", 1.0f);
        runBlocks (proc, 0.1, 512);
        snapshotTo (*ed, outDir.getChildFile ("strangedrift-window-held.png"));
        expect (proc.held.load(), "panel sees Hold");
        setParam (proc, "hold", 0.0f);
        param (proc, "speed")->setValueNotifyingHost (1.0f);
        runBlocks (proc, 2.0, 512);
        snapshotTo (*ed, outDir.getChildFile ("strangedrift-window-512x.png"));

        // Lights | Orbits: starts on Lights, switches, saves, moves no parameter.
        expect (! ed->isOrbitsView() && ed->panel.lightsButton.getToggleState() && ! ed->panel.orbitsButton.getToggleState(), "window starts in Lights view");
        const auto beforeView = snapshot (proc);
        ed->panel.orbitsButton.setToggleState (true, juce::sendNotificationSync);
        expect (ed->isOrbitsView() && (bool) proc.getValueTreeState().state.getProperty (DriftPanel::orbitsViewProperty, false),
                "Orbits button shows the orbits and keeps the choice in the saved state");
        runBlocks (proc, 1.0, 512);
        juce::Thread::sleep (80);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);   // the panel's timer copies the traces
        snapshotTo (*ed, outDir.getChildFile ("strangedrift-window-orbits.png"));
        expect (snapshot (proc) == beforeView, "switching the view moves no parameter");
        param (proc, "speed")->setValueNotifyingHost (0.5f);
        runBlocks (proc, 0.5, 512);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        snapshotTo (*ed, outDir.getChildFile ("strangedrift-window-orbits-1x.png"));
        ed->panel.lightsButton.setToggleState (true, juce::sendNotificationSync);
        expect (! ed->isOrbitsView() && ! (bool) proc.getValueTreeState().state.getProperty (DriftPanel::orbitsViewProperty, false), "Lights button shows the lights again");
        expect (ed->controls().size() == 9, "the view buttons and New start are not parameter controls");

        // New start: a fresh place, no parameter moved.
        const auto beforeNew = proc.currentChaosState();
        const auto paramsBefore = snapshot (proc);
        ed->panel.newStartButton.triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        runBlocks (proc, 0.1, 512);
        expect (proc.currentChaosState() != beforeNew && snapshot (proc) == paramsBefore, "New start jumps to a fresh place and moves no parameter");

        // The window only draws what moved (0.1.1): while held, nothing is drawn again; while running at 1x, far
        // fewer than all five parts each frame.
        auto pump = [&] (double seconds)
        {
            for (int k = 0; k < (int) (seconds * 50.0); ++k)
            {
                runBlocks (proc, 0.02, 480);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            }
        };
        setParam (proc, "speed", 0.5f);
        setParam (proc, "hold", 1.0f);
        pump (0.4);
        const double heldArea = ed->panel.lightsLayer().areaDrawn;
        const int heldFrames = ed->panel.lightsLayer().framesDrawn;
        pump (1.0);
        expect (ed->panel.lightsLayer().framesDrawn > heldFrames + 10 && ed->panel.lightsLayer().areaDrawn == heldArea,
                "while held, the window draws nothing again");
        setParam (proc, "hold", 0.0f);
        pump (0.3);
        const double a0 = ed->panel.lightsLayer().areaDrawn;
        const int f0 = ed->panel.lightsLayer().framesDrawn;
        pump (4.0);
        const double share = (ed->panel.lightsLayer().areaDrawn - a0) / juce::jmax (1, ed->panel.lightsLayer().framesDrawn - f0) / (780.0 * 470.0);
        std::cout << "window at 1x: " << juce::String (100.0 * share, 1) << "% of the panel drawn again per frame (0.1.0: 100%)" << std::endl;
        expect (share > 0.002 && share < 0.25, "at 1x the window draws only what moved (" + juce::String (100.0 * share, 1) + "% of the panel per frame; 0.1.0 drew all of it)");

        bool rising = true;
        for (int k = 1; k <= 100; ++k) rising = rising && DriftStyle::lightAlpha (k / 100.0) >= DriftStyle::lightAlpha ((k - 1) / 100.0);
        expect (std::fabs (DriftStyle::lightAlpha (0.0) - 0.10f) < 1.0e-6f && std::fabs (DriftStyle::lightAlpha (1.0) - 1.0f) < 1.0e-6f
                && DriftStyle::lightAlpha (0.3) > 0.15f + 0.85f * 0.3f && rising, "light brightness curve as on Ogdoad 0.2.1");
    }

    // ---- Resizing: 80% to 200%, proportions locked; everything stays inside -------------------------------------
    {
        DriftProcessor proc; proc.prepareToPlay (48000.0, 512); proc.seedForChecks (7);
        std::unique_ptr<DriftEditor> ed (dynamic_cast<DriftEditor*> (proc.createEditor()));
        expect (ed->isResizable() && ed->getConstrainer() != nullptr, "the window is resizable");
        auto* con = ed->getConstrainer();
        const double ratio = (double) DriftEditor::kWidth / DriftEditor::kHeight;
        auto clamp = [&] (int w, int h)
        {
            juce::Rectangle<int> b (0, 0, w, h);
            con->checkBounds (b, juce::Rectangle<int> (0, 0, 780, 470), juce::Rectangle<int> (0, 0, 4000, 3000), false, false, false, true);
            return b;
        };
        auto big = clamp (3000, 3000), small = clamp (100, 100), mid = clamp (1000, 1000);
        expect (big.getWidth() <= 1560 && big.getHeight() <= 940 && small.getWidth() >= 624 && small.getHeight() >= 376
                && std::fabs ((double) mid.getWidth() / mid.getHeight() - ratio) < 0.01 && std::fabs ((double) big.getWidth() / big.getHeight() - ratio) < 0.01,
                "size limits 80% to 200% and the proportions stay locked");

        struct Size { int w; const char* name; } sizes[] = { { DriftEditor::kMaxWidth, "large" }, { DriftEditor::kMinWidth, "small" }, { 1170, "medium" } };
        for (auto& sz : sizes)
        {
            ed->setSize (sz.w, DriftEditor::heightForWidth (sz.w));
            const double scale = (double) sz.w / DriftEditor::kWidth;
            expect (std::fabs (ed->panel.getTransform().getScaleFactor() - scale) < 1.0e-3, juce::String ("panel scale at ") + sz.name + " is " + juce::String (scale, 2));
            bool inside = true;
            std::vector<juce::Component*> all;
            for (auto& c : ed->controls()) all.push_back (c.control);
            all.push_back (&ed->panel.lightsButton); all.push_back (&ed->panel.orbitsButton); all.push_back (&ed->panel.newStartButton);
            for (auto* c : all)
            {
                const auto r = ed->getLocalArea (c, c->getLocalBounds());
                inside = inside && ed->getLocalBounds().contains (r) && ! r.isEmpty();
            }
            expect (inside, juce::String ("every control inside the window at ") + sz.name + " size");
            expect ((int) proc.getValueTreeState().state.getProperty (DriftEditor::widthProperty, -1) == sz.w, juce::String ("the ") + sz.name + " width is kept in the saved state");
            runBlocks (proc, 0.4, 512);
            snapshotTo (*ed, outDir.getChildFile (juce::String ("strangedrift-window-") + sz.name + ".png"));
        }
    }

    // ---- 3: saving and reloading ----------------------------------------------------------------------------------
    {
        DriftProcessor a; a.prepareToPlay (48000.0, 512); a.seedForChecks (31);
        setParam (a, "speed", 0.83f); setParam (a, "hold", 1.0f); setParam (a, "lean1", -0.6f); setParam (a, "nudge1", 0.4f);
        setParam (a, "nudge1From", 5.0f); setParam (a, "lean2", 0.7f); setParam (a, "nudge2", -0.9f); setParam (a, "nudge2From", 1.0f);
        setParam (a, "outChannel", 5.0f);
        setParam (a, "hold", 0.0f);
        runBlocks (a, 2.0, 512);
        setParam (a, "hold", 1.0f);
        runBlocks (a, 0.1, 512);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        DriftProcessor b; b.prepareToPlay (48000.0, 512);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        expect (snapshot (a) == snapshot (b), "every setting comes back after save and reload");
        expect (b.currentChaosState() == a.currentChaosState() && a.currentChaosState().startsWith ("SD1"), "the chaos position comes back exactly");
        expect (! b.getValueTreeState().state.hasProperty (DriftProcessor::chaosStateProperty), "the position is kept out of the parameter list");
        auto outA = runBlocks (a, 0.05, 512);
        runBlocks (b, 0.05, 512);
        expect (std::fabs (a.outputs[0].load() - b.outputs[0].load()) < 1e-6f && std::fabs (a.outputs[5].load() - b.outputs[5].load()) < 1e-6f,
                "after reloading, the outputs are where they were when saved");
        setParam (a, "hold", 0.0f); setParam (b, "hold", 0.0f);
        a.prepareToPlay (48000.0, 512); b.prepareToPlay (48000.0, 512);
        auto ra = runBlocks (a, 3.0, 512), rb = runBlocks (b, 3.0, 512);
        expect (sameStream (ra, rb) && ! ra.empty(), "a reloaded copy moves on exactly like the original");

        // A project with no position saved (or a broken one) opens with its settings, at a random place.
        auto xml = a.getValueTreeState().copyState().createXml();
        juce::MemoryBlock noPos; juce::AudioProcessor::copyXmlToBinary (*xml, noPos);
        DriftProcessor c; c.prepareToPlay (48000.0, 512);
        c.setStateInformation (noPos.getData(), (int) noPos.getSize());
        expect (snapshot (c) == snapshot (a) && c.currentChaosState().startsWith ("SD1"), "a project without a saved position opens with its settings");
        xml->setAttribute (DriftProcessor::chaosStateProperty, "SD1 broken");
        juce::MemoryBlock broken; juce::AudioProcessor::copyXmlToBinary (*xml, broken);
        DriftProcessor d; d.prepareToPlay (48000.0, 512);
        const auto dBefore = d.currentChaosState();
        d.setStateInformation (broken.getData(), (int) broken.getSize());
        expect (snapshot (d) == snapshot (a) && d.currentChaosState() == dBefore, "a broken saved position is ignored safely");

        // Two new copies start in different places.
        DriftProcessor e1, e2;
        expect (e1.currentChaosState() != e2.currentChaosState(), "two new copies of the plugin start in different places");

        // The Lights | Orbits choice and the window size are saved with the project.
        a.getValueTreeState().state.setProperty (DriftPanel::orbitsViewProperty, true, nullptr);
        a.getValueTreeState().state.setProperty (DriftEditor::widthProperty, 1170, nullptr);
        juce::MemoryBlock withView; a.getStateInformation (withView);
        DriftProcessor f; f.setStateInformation (withView.getData(), (int) withView.getSize());
        std::unique_ptr<DriftEditor> edF (dynamic_cast<DriftEditor*> (f.createEditor()));
        expect (edF->isOrbitsView() && edF->panel.orbitsButton.getToggleState(), "Orbits view is saved and comes back");
        expect (edF->getWidth() == 1170 && edF->getHeight() == DriftEditor::heightForWidth (1170), "the window size comes back");
        for (auto pr : { std::pair<int, int> { 5000, DriftEditor::kMaxWidth }, { 100, DriftEditor::kMinWidth } })
        {
            DriftProcessor g1; g1.getValueTreeState().state.setProperty (DriftEditor::widthProperty, pr.first, nullptr);
            juce::MemoryBlock gb; g1.getStateInformation (gb);
            DriftProcessor g2; g2.setStateInformation (gb.getData(), (int) gb.getSize());
            std::unique_ptr<DriftEditor> edG (dynamic_cast<DriftEditor*> (g2.createEditor()));
            expect (edG->getWidth() == pr.second, "saved window width " + juce::String (pr.first) + " reopens at " + juce::String (pr.second));
        }
        DriftProcessor h;
        std::unique_ptr<DriftEditor> edH (dynamic_cast<DriftEditor*> (h.createEditor()));
        expect (! edH->isOrbitsView() && edH->getWidth() == DriftEditor::kWidth, "a new copy opens on Lights at the normal size");
    }

    // ---- 4: MIDI --------------------------------------------------------------------------------------------------
    {
        DriftProcessor p; p.prepareToPlay (48000.0, 512); p.seedForChecks (11);
        setParam (p, "speed", 0.83f);
        std::vector<Msg> in {
            { 100, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) },
            { 2000, juce::MidiMessage::controllerEvent (1, 7, 90) },
            { 3000, juce::MidiMessage::controllerEvent (16, 20, 64) },      // Ogdoad's LFO 1, coarse
            { 3000, juce::MidiMessage::controllerEvent (16, 52, 10) },      // and fine
            { 3500, juce::MidiMessage::controllerEvent (16, 106, 1) },      // channel 16, our numbers: not ours
            { 30000, juce::MidiMessage::noteOff (1, 60) },
            { 40000, juce::MidiMessage::pitchWheel (2, 9000) },
        };
        auto out = runBlocks (p, 2.0, 512, in);
        std::vector<Msg> others; int ours = 0; bool channelOk = true; std::set<int> numbers;
        for (auto& e : out)
        {
            if (isOurCc (e.m) && e.m.getChannel() == 15) { ++ours; numbers.insert (e.m.getControllerNumber()); }
            else others.push_back (e);
        }
        bool passOk = others.size() == in.size();
        for (size_t i = 0; passOk && i < in.size(); ++i)
            passOk = others[i].sample == in[i].sample && others[i].m.getDescription() == in[i].m.getDescription();
        expect (passOk, "notes, CC, Ogdoad's CC and pitch bend pass through unchanged, at the same moment");
        expect (ours > 500 && channelOk, "the outputs come out as CC on channel 15 (" + juce::String (ours) + " messages in 2 s)");
        expect (numbers.size() == 14, "CC 106-119 all used, nothing else");

        setParam (p, "outChannel", 3.0f);
        auto out3 = runBlocks (p, 0.5, 512);
        bool all3 = ! out3.empty();
        for (auto& e : out3) all3 = all3 && e.m.getChannel() == 3 && isOurCc (e.m);
        expect (all3, "MIDI out channel 3: the CC moves to channel 3");
        setParam (p, "outChannel", 15.0f);

        // The mod wheel nudges ripple when chosen; the wheel itself still passes through.
        setParam (p, "nudge1From", 1.0f); setParam (p, "nudge1", 1.0f);
        DriftProcessor q; q.prepareToPlay (48000.0, 512); q.seedForChecks (11);
        setParam (q, "speed", 0.83f); setParam (q, "outChannel", 15.0f); setParam (q, "nudge1From", 1.0f); setParam (q, "nudge1", 1.0f);
        p.seedForChecks (40); q.seedForChecks (40);
        p.prepareToPlay (48000.0, 512); q.prepareToPlay (48000.0, 512);
        auto withWheel = runBlocks (p, 2.0, 512, { { 10, juce::MidiMessage::controllerEvent (1, 1, 127) } });
        auto without = runBlocks (q, 2.0, 512);
        int wheelSeen = 0; for (auto& e : withWheel) if (e.m.isController() && e.m.getControllerNumber() == 1) ++wheelSeen;
        expect (wheelSeen == 1, "mod wheel passes through");
        expect (std::fabs (p.outputs[0].load() - q.outputs[0].load()) > 1e-4f, "mod wheel up with Nudge from: mod wheel moves ripple differently");
        setParam (p, "nudge1From", 0.0f); setParam (p, "nudge1", 0.0f);

        // Bypass: MIDI passes, nothing added.
        auto byp = runBlocks (p, 0.5, 512, { { 50, juce::MidiMessage::noteOn (1, 64, (juce::uint8) 80) } }, true);
        expect (byp.size() == 1 && byp[0].m.isNoteOn(), "bypassed: notes pass, no CC added");

        // Hold: silent after the last value.
        setParam (p, "hold", 1.0f);
        runBlocks (p, 0.2, 512);
        expect (runBlocks (p, 1.0, 512).empty(), "Hold: nothing sent while held");
        setParam (p, "hold", 0.0f);
        expect (! runBlocks (p, 0.2, 512).empty(), "after Hold the CC flows again");
    }

    // ---- block sizes inside the real plugin --------------------------------------------------------------------------
    {
        auto stream = [] (int block)
        {
            DriftProcessor p; p.prepareToPlay (48000.0, 1024); p.seedForChecks (99);
            setParam (p, "speed", 0.9f); setParam (p, "lean1", 0.3f); setParam (p, "nudge2", 0.6f); setParam (p, "nudge2From", 4.0f);
            return runBlocks (p, 5.0, block);
        };
        auto ref = stream (512);
        for (int b : { 64, 333, 1024, 0 })
            expect (sameStream (stream (b), ref), "same CC stream with block size " + (b > 0 ? juce::String (b) : juce::String ("random")));
        expect (ref.size() > 1000, "the comparison stream is not empty");
    }

    if (failures == 0) std::cout << "preview checks: " << checks << " checks passed" << std::endl;
    else std::cout << "preview checks: " << failures << " of " << checks << " checks FAILED" << std::endl;
    return failures == 0 ? 0 : 1;
}
