#include "PluginProcessor.h"
#include "PluginEditor.h"

DriftProcessor::DriftProcessor()
    : AudioProcessor (BusesProperties()),                       // a MIDI effect: no audio buses
      apvts (*this, nullptr, "StrangeDrift", DriftParams::createLayout())
{
    speedParam      = apvts.getRawParameterValue (DriftParams::speed);
    holdParam       = apvts.getRawParameterValue (DriftParams::hold);
    for (int c = 0; c < 2; ++c)
    {
        leanParam[(size_t) c]      = apvts.getRawParameterValue (DriftParams::lean[c]);
        nudgeParam[(size_t) c]     = apvts.getRawParameterValue (DriftParams::nudge[c]);
        nudgeFromParam[(size_t) c] = apvts.getRawParameterValue (DriftParams::nudgeFrom[c]);
    }
    outChannelParam = apvts.getRawParameterValue (DriftParams::outChannel);
    inputs.reserve (4096);
    outBuffer.ensureSize (65536);

    // A new copy starts somewhere new on each butterfly.
    engine.prepare (48000.0);
    engine.seed ((uint64_t) juce::Random::getSystemRandom().nextInt64());
    shadow = engine;
    publishToPanel();
}

void DriftProcessor::seedForChecks (uint64_t seed)
{
    const juce::SpinLock::ScopedLockType l (lock);
    engine.seed (seed);
    shadow = engine;
    hasPending = false;
    publishToPanel();
}

void DriftProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);            // the chaos stays where it is
    outBuffer.ensureSize (65536);
}

bool DriftProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet().isDisabled() && layouts.getMainInputChannelSet().isDisabled();
}

void DriftProcessor::readSettings()
{
    strangedrift::Settings s;
    s.speed = speedParam->load();
    s.hold  = holdParam->load() >= 0.5f;
    for (int c = 0; c < 2; ++c)
    {
        s.lean[c]      = leanParam[(size_t) c]->load();
        s.nudge[c]     = nudgeParam[(size_t) c]->load();
        s.nudgeFrom[c] = juce::roundToInt (nudgeFromParam[(size_t) c]->load());
    }
    engine.setSettings (s);
}

void DriftProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    // A position loaded from a project, or New start.
    {
        const juce::SpinLock::ScopedTryLockType l (lock);
        if (l.isLocked() && hasPending)
        {
            engine.takePositionFrom (pending);
            hasPending = false;
        }
    }
    if (newStartRequested.exchange (false))
        engine.seed ((uint64_t) random.nextInt64());

    readSettings();

    // Everything that comes in goes out unchanged; the mod wheel is also read.
    inputs.clear();
    outBuffer.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int offset = juce::jlimit (0, numSamples - 1, meta.samplePosition);
        outBuffer.addEvent (m, offset);
        if (m.isController() && m.getControllerNumber() == 1 && inputs.size() < inputs.capacity())
            inputs.push_back ({ offset, m.getControllerValue() / 127.0 });
    }

    sink.out = &outBuffer;
    sink.channel = juce::jlimit (1, 16, juce::roundToInt (outChannelParam->load()));
    engine.render (numSamples, inputs.data(), (int) inputs.size(), sink);
    midi.swapWith (outBuffer);
    publishToPanel();

    // About every 20 ms, a copy for saving and for the Orbits view.
    blocksSinceShadow += numSamples;
    if (blocksSinceShadow >= (int) (getSampleRate() > 0 ? getSampleRate() * 0.02 : 960.0))
    {
        const juce::SpinLock::ScopedTryLockType l (lock);
        if (l.isLocked())
        {
            shadow.takePositionFrom (engine);
            blocksSinceShadow = 0;
        }
    }
}

void DriftProcessor::publishToPanel()
{
    for (int i = 0; i < strangedrift::kNumOutputs; ++i)
        outputs[(size_t) i].store ((float) engine.output (i));
    held.store (engine.isHeld());
    speedNow.store ((float) strangedrift::speedFactor (engine.getSettings().speed));
    blocksDone.fetch_add (1);
}

bool DriftProcessor::copyTraces (std::array<std::array<float, strangedrift::Engine::kTracePoints * 2>, strangedrift::kNumCircuits>& dest,
                                 std::array<int, strangedrift::kNumCircuits>& counts)
{
    const juce::SpinLock::ScopedTryLockType l (lock);
    if (! l.isLocked()) return false;
    for (int c = 0; c < strangedrift::kNumCircuits; ++c)
    {
        counts[(size_t) c] = shadow.traceCount (c);
        for (int k = 0; k < counts[(size_t) c]; ++k)
        {
            dest[(size_t) c][(size_t) (k * 2)]     = shadow.traceX (c, k);
            dest[(size_t) c][(size_t) (k * 2 + 1)] = shadow.traceY (c, k);
        }
    }
    return true;
}

juce::String DriftProcessor::currentChaosState()
{
    const juce::SpinLock::ScopedLockType l (lock);
    return juce::String (hasPending ? pending.saveState() : shadow.saveState());
}

void DriftProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (chaosStateProperty, currentChaosState(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DriftProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            const juce::String chaos = tree.getProperty (chaosStateProperty, juce::String()).toString();
            tree.removeProperty (chaosStateProperty, nullptr);
            apvts.replaceState (tree);

            strangedrift::Engine loaded;
            if (chaos.isNotEmpty() && loaded.loadState (chaos.toStdString()))
            {
                const juce::SpinLock::ScopedLockType l (lock);
                pending.takePositionFrom (loaded);
                shadow.takePositionFrom (loaded);
                hasPending = true;
            }
        }
}

juce::AudioProcessorEditor* DriftProcessor::createEditor()
{
    return new DriftEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DriftProcessor();
}
