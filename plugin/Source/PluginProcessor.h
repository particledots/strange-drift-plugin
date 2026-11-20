// What Logic sees: a MIDI effect (AU type aumi) with no audio. Incoming MIDI passes through untouched; the seven
// outputs are added as smooth 14-bit CC on the chosen channel (15 by default). The incoming mod wheel (CC 1) is also
// read, for the Nudge from menus.
//
// Where the chaos is gets saved with the project (property "chaosState"), so a project reopens where it left off.
// A new copy of the plugin starts at a random place; New start jumps to a fresh one.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "strangedrift/Engine.h"
#include "Parameters.h"

#include <array>
#include <atomic>
#include <vector>

class DriftProcessor : public juce::AudioProcessor
{
public:
    DriftProcessor();
    ~DriftProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}   // MIDI passes, nothing added

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Strange Drift"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }

    // New start: the next block jumps to a fresh random place (pressed on the panel).
    void requestNewStart() { newStartRequested.store (true); }

    // For the offline checks: start from a known place instead of a random one.
    void seedForChecks (uint64_t seed);

    // Where the chaos is now, as saved with the project.
    juce::String currentChaosState();

    // For the panel: the latest value of each output (written by the audio thread after every block) and a copy of
    // the Orbits traces.
    std::array<std::atomic<float>, strangedrift::kNumOutputs> outputs {};
    std::atomic<bool> held { false };
    std::atomic<float> speedNow { 1.0f };
    std::atomic<int> blocksDone { 0 };
    // Copies the three Orbits traces (X, Y pairs, oldest first) into dest; returns false if not available right now.
    bool copyTraces (std::array<std::array<float, strangedrift::Engine::kTracePoints * 2>, strangedrift::kNumCircuits>& dest,
                     std::array<int, strangedrift::kNumCircuits>& counts);

    static inline const juce::Identifier chaosStateProperty { "chaosState" };

private:
    struct Sink : strangedrift::CcSink
    {
        juce::MidiBuffer* out = nullptr;
        int channel = strangedrift::kDefaultChannel;
        void cc (int offset, int number, int value) override
        {
            out->addEvent (juce::MidiMessage::controllerEvent (channel, number, value), offset);
        }
    };
    void publishToPanel();
    void readSettings();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* speedParam = nullptr;
    std::atomic<float>* holdParam = nullptr;
    std::array<std::atomic<float>*, 2> leanParam {}, nudgeParam {}, nudgeFromParam {};
    std::atomic<float>* outChannelParam = nullptr;

    strangedrift::Engine engine;
    std::vector<strangedrift::InputEvent> inputs;
    juce::MidiBuffer outBuffer;
    Sink sink;
    juce::Random random;
    std::atomic<bool> newStartRequested { false };

    // `shadow` is a copy of the engine the message thread can read (for saving and the Orbits view); `pending` is a
    // position loaded from a project, handed to the audio thread at its next block. Both are guarded by `lock`
    // (the audio thread only ever tries it, never waits).
    juce::SpinLock lock;
    strangedrift::Engine shadow, pending;
    bool hasPending = false;
    int blocksSinceShadow = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriftProcessor)
};
