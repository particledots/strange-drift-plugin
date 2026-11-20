#include "strangedrift/Engine.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace strangedrift
{
    namespace
    {
        uint32_t nextRandom (uint32_t& s)
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return s;
        }

        double random01 (uint32_t& s)      { return (double) (nextRandom (s) & 0xFFFFFF) / (double) 0xFFFFFF; }
        double randomBipolar (uint32_t& s) { return random01 (s) * 2.0 - 1.0; }

        uint32_t mixSeed (uint64_t v)
        {
            v += 0x9E3779B97F4A7C15ull;
            v = (v ^ (v >> 30)) * 0xBF58476D1CE4E5B9ull;
            v = (v ^ (v >> 27)) * 0x94D049BB133111EBull;
            v ^= v >> 31;
            const uint32_t r = (uint32_t) (v ^ (v >> 32));
            return r == 0 ? 0x12345678u : r;
        }

        double secondsPerUnit (int c) { return kTripSeconds[c] / kMeanTripUnits; }

        // A tiny, steady jitter on x, like the noise in a real circuit. Far too small to hear or see; it only makes sure
        // a circuit can never sit exactly on the one balance point (all zero) the equations allow.
        constexpr double kJitter = 1.0e-9;
    }

    Engine::Engine()
    {
        for (int i = 0; i < kNumOutputs; ++i)
            cc.setLane (i, coarseCc (i), fineCc (i));
        seed (1);
    }

    void Engine::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        rateInt = (uint64_t) std::llround (sampleRate);
        if (rateInt < 1000) rateInt = 1000;
        // The two clocks restart; the chaos stays where it is.
        samples = steps = ticks = 0;
        samplesToTick = 0;
        cc.reset();
    }

    void Engine::seed (uint64_t seedValue)
    {
        for (int c = 0; c < kNumCircuits; ++c)
        {
            auto& r = rng[(std::size_t) c];
            r = mixSeed (seedValue * 3 + (uint64_t) c);
            auto& k = circuits[(std::size_t) c];
            k.x = (random01 (r) < 0.5 ? -1.0 : 1.0) * (2.0 + 10.0 * random01 (r));
            k.y = randomBipolar (r) * 10.0;
            k.z = 15.0 + 20.0 * random01 (r);
            k.advance (30.0, 0.0);                         // settle onto the butterfly
            traceHead[(std::size_t) c] = traceFill[(std::size_t) c] = 0;
            traceAcc[(std::size_t) c] = 0.0;
            for (int p = 0; p < kTracePoints; ++p)        // and fill the Orbits trace with where it has just been
            {
                k.advance (kTraceUnits, 0.0);
                recordTrace (c);
            }
            units[(std::size_t) c] = 0.0;
            pushes[(std::size_t) c] = 0.0;
        }
        updateOutputs();
        cc.reset();
    }

    void Engine::recordTrace (int c)
    {
        const auto& k = circuits[(std::size_t) c];
        auto& t = trace[(std::size_t) c];
        int& head = traceHead[(std::size_t) c];
        t[(std::size_t) (head * 2)]     = (float) k.outX();
        t[(std::size_t) (head * 2 + 1)] = (float) k.outY();
        head = (head + 1) % kTracePoints;
        traceFill[(std::size_t) c] = std::min (kTracePoints, traceFill[(std::size_t) c] + 1);
    }

    float Engine::traceX (int c, int k) const
    {
        const int fill = traceFill[(std::size_t) c];
        const int idx = ((traceHead[(std::size_t) c] - fill + k) % kTracePoints + kTracePoints) % kTracePoints;
        return trace[(std::size_t) c][(std::size_t) (idx * 2)];
    }

    float Engine::traceY (int c, int k) const
    {
        const int fill = traceFill[(std::size_t) c];
        const int idx = ((traceHead[(std::size_t) c] - fill + k) % kTracePoints + kTracePoints) % kTracePoints;
        return trace[(std::size_t) c][(std::size_t) (idx * 2 + 1)];
    }

    void Engine::updateOutputs()
    {
        for (int c = 0; c < kNumCircuits; ++c)
        {
            outputs[(std::size_t) (c * 2)]     = circuits[(std::size_t) c].outX();
            outputs[(std::size_t) (c * 2 + 1)] = circuits[(std::size_t) c].outY();
        }
        outputs[kOutBlend] = blendOf (outputs[kOutRippleY], outputs[kOutTideY], outputs[kOutGlacierY]);
    }

    double Engine::nudgeSource (int controlled, int which) const
    {
        switch ((NudgeFrom) std::clamp (which, 0, kNumNudgeSources - 1))
        {
            case NudgeFrom::modWheel: return modWheel;                     // 0..1: at rest it does nothing
            case NudgeFrom::firstX:   return outputs[(std::size_t) (nudgeCircuit (controlled, false) * 2)];
            case NudgeFrom::firstY:   return outputs[(std::size_t) (nudgeCircuit (controlled, false) * 2 + 1)];
            case NudgeFrom::secondX:  return outputs[(std::size_t) (nudgeCircuit (controlled, true) * 2)];
            case NudgeFrom::secondY:  return outputs[(std::size_t) (nudgeCircuit (controlled, true) * 2 + 1)];
            case NudgeFrom::off:
            default:                  return 0.0;
        }
    }

    void Engine::doStep()
    {
        ++steps;
        if (settings.hold)
            return;

        const double speed = speedFactor (settings.speed);

        // Every push is worked out from the outputs as they were before this step, so the order of the circuits
        // does not matter.
        for (int c = 0; c < kNumCircuits; ++c)
        {
            double p = 0.0;
            if (c < kNumControlled)
            {
                const double lean  = std::clamp (settings.lean[c], -1.0, 1.0);
                const double nudge = std::clamp (settings.nudge[c], -1.0, 1.0);
                p = softLimit (kPushPerKnob * (lean + nudge * nudgeSource (c, settings.nudgeFrom[c])));
            }
            pushes[(std::size_t) c] = p;
        }

        for (int c = 0; c < kNumCircuits; ++c)
        {
            auto& k = circuits[(std::size_t) c];
            const double du = speed / (double) kStepsPerSecond / secondsPerUnit (c);
            // Advance in pieces that end exactly on each Orbits trace point, so the trace stays smooth at high Speed.
            double left = du;
            while (left > 0.0)
            {
                double& acc = traceAcc[(std::size_t) c];
                const double piece = std::min (left, kTraceUnits - acc);
                k.advance (piece, pushes[(std::size_t) c]);
                left -= piece;
                acc += piece;
                if (acc >= kTraceUnits - 1.0e-12)
                {
                    acc = 0.0;
                    recordTrace (c);
                }
            }
            k.x += kJitter * randomBipolar (rng[(std::size_t) c]);
            if (! k.healthy())
            {
                // Never expected (the engine tests prove it); if it ever happened, the circuit starts afresh.
                ++rescueCount;
                auto& r = rng[(std::size_t) c];
                k.x = 1.0 + 5.0 * random01 (r); k.y = 1.0; k.z = 20.0;
                k.advance (5.0, 0.0);
            }
            units[(std::size_t) c] += du;
        }
        updateOutputs();
    }

    void Engine::render (int numSamples, const InputEvent* events, int numEvents, CcSink& sink)
    {
        int pos = 0, ev = 0;
        while (pos < numSamples)
        {
            while (ev < numEvents && events[ev].offset <= pos)
                modWheel = std::clamp (events[ev++].modWheel, 0.0, 1.0);

            while (sampleOfStep (steps) <= samples)
                doStep();

            if (samplesToTick == 0)
            {
                tick (pos, sink);
                samplesToTick = kTickSamples;
            }

            int n = std::min (samplesToTick, numSamples - pos);
            const uint64_t toStep = sampleOfStep (steps) - samples;
            if (toStep < (uint64_t) n) n = (int) toStep;
            if (ev < numEvents && events[ev].offset - pos < n) n = events[ev].offset - pos;
            if (n < 1) n = 1;
            pos += n;
            samples += (uint64_t) n;
            samplesToTick -= n;
        }
        for (; ev < numEvents; ++ev)
            modWheel = std::clamp (events[ev].modWheel, 0.0, 1.0);
    }

    void Engine::tick (int offset, CcSink& sink)
    {
        const double dt = (double) kTickSamples / sampleRate;
        for (int i = 0; i < kNumOutputs; ++i)
            cc.update (i, outputs[(std::size_t) i], dt, offset, sink);
        ++ticks;
    }

    void Engine::takePositionFrom (const Engine& o)
    {
        circuits = o.circuits;
        rng = o.rng;
        trace = o.trace;
        traceHead = o.traceHead;
        traceFill = o.traceFill;
        traceAcc = o.traceAcc;
        pushes = o.pushes;
        updateOutputs();
        cc.reset();
    }

    // ---- Saving and loading the chaos position ------------------------------------------------------------------
    // Text: "SD1", then per circuit x y z (exact, as hex floating point), its random state, the trace position and
    // fill, and the trace points (4 hex digits each).

    std::string Engine::saveState() const
    {
        std::ostringstream out;
        out << "SD1";
        char buf[64];
        for (int c = 0; c < kNumCircuits; ++c)
        {
            const auto& k = circuits[(std::size_t) c];
            for (double v : { k.x, k.y, k.z, traceAcc[(std::size_t) c] })
            {
                std::snprintf (buf, sizeof (buf), " %a", v);
                out << buf;
            }
            out << ' ' << rng[(std::size_t) c] << ' ' << traceHead[(std::size_t) c] << ' ' << traceFill[(std::size_t) c] << ' ';
            for (float v : trace[(std::size_t) c])
            {
                const int q = std::clamp ((int) std::lround (((double) v + 1.0) * 0.5 * 65535.0), 0, 65535);
                std::snprintf (buf, sizeof (buf), "%04x", q);
                out << buf;
            }
        }
        return out.str();
    }

    bool Engine::loadState (const std::string& text)
    {
        std::istringstream in (text);
        std::string word;
        if (! (in >> word) || word != "SD1")
            return false;

        std::array<Circuit, kNumCircuits> newCircuits {};
        std::array<double, kNumCircuits> newAcc {};
        std::array<uint32_t, kNumCircuits> newRng {};
        std::array<int, kNumCircuits> newHead {}, newFill {};
        std::array<std::array<float, kTracePoints * 2>, kNumCircuits> newTrace {};

        for (int c = 0; c < kNumCircuits; ++c)
        {
            double v[4];
            for (double& d : v)
            {
                if (! (in >> word)) return false;
                char* end = nullptr;
                d = std::strtod (word.c_str(), &end);
                if (end == word.c_str() || *end != 0 || ! std::isfinite (d)) return false;
            }
            unsigned long r = 0;
            int head = 0, fill = 0;
            std::string hex;
            if (! (in >> r >> head >> fill >> hex)) return false;
            if (head < 0 || head >= kTracePoints || fill < 0 || fill > kTracePoints || r == 0 || r > 0xFFFFFFFFul) return false;
            if (hex.size() != (std::size_t) (kTracePoints * 2 * 4)) return false;
            for (int p = 0; p < kTracePoints * 2; ++p)
            {
                char* end = nullptr;
                const std::string part = hex.substr ((std::size_t) (p * 4), 4);
                const long q = std::strtol (part.c_str(), &end, 16);
                if (*end != 0) return false;
                newTrace[(std::size_t) c][(std::size_t) p] = (float) ((double) q / 65535.0 * 2.0 - 1.0);
            }
            newCircuits[(std::size_t) c].x = v[0];
            newCircuits[(std::size_t) c].y = v[1];
            newCircuits[(std::size_t) c].z = v[2];
            if (! newCircuits[(std::size_t) c].healthy()) return false;
            newAcc[(std::size_t) c] = std::clamp (v[3], 0.0, kTraceUnits);
            newRng[(std::size_t) c] = (uint32_t) r;
            newHead[(std::size_t) c] = head;
            newFill[(std::size_t) c] = fill;
        }

        circuits = newCircuits;
        traceAcc = newAcc;
        rng = newRng;
        traceHead = newHead;
        traceFill = newFill;
        trace = newTrace;
        updateOutputs();
        cc.reset();
        return true;
    }
}
