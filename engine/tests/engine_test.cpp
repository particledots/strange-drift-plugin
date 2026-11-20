// Strange Drift engine self-test. Runs on the Linux checking computer before every Mac build.
//
// What it proves, without Logic:
//   - each circuit's trip time at Speed 1x is inside the module's documented range, and Speed scales it exactly;
//   - hours of running at 44.1, 48 and 96 kHz and any block size: always inside -1..+1, never a runaway, never a
//     standstill, both wings visited;
//   - every Lean / Nudge combination keeps moving (no standstill, no runaway), and Lean leans the right way;
//   - the same chaos path at every sample rate, the same MIDI at every block size, after save and reload, and New start;
//   - Hold, the mod wheel, blend, and the CC rules copied from Ogdoad (6 ms cap, coarse before fine, small steps);
//   - the CPU use and the MIDI rate.

#include "strangedrift/Engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

using namespace strangedrift;

namespace
{
    int checks = 0, failures = 0;

    void expect (bool ok, const std::string& what)
    {
        ++checks;
        if (! ok) { ++failures; std::printf ("FAIL: %s\n", what.c_str()); }
    }

    struct Msg { uint64_t sample; int cc, value; bool operator== (const Msg& o) const { return sample == o.sample && cc == o.cc && value == o.value; } };

    struct RecordSink : CcSink
    {
        std::vector<Msg>* out = nullptr;
        uint64_t base = 0;
        long count = 0;
        void cc (int offset, int number, int value) override
        {
            ++count;
            if (out) out->push_back ({ base + (uint64_t) offset, number, value });
        }
    };

    double knobForSpeed (double speed)
    {
        const double oct = std::log2 (speed);
        return oct < 0.0 ? 0.5 + oct / 6.0 : 0.5 + oct / 18.0;
    }

    // Runs an engine for a number of seconds in blocks of a given size (0 = random sizes 1..4096). After each block
    // `each` is called with the engine (for watching the outputs). Mod wheel events are given in absolute samples.
    void run (Engine& e, double sampleRate, double seconds, int block, std::vector<Msg>* out = nullptr,
              const std::function<void (const Engine&)>& each = nullptr,
              const std::vector<std::pair<uint64_t, double>>& wheel = {}, uint32_t blockSeed = 7)
    {
        RecordSink sink;
        sink.out = out;
        std::mt19937 rng (blockSeed);
        std::uniform_int_distribution<int> sizes (1, 4096);
        const uint64_t total = (uint64_t) std::llround (seconds * sampleRate);
        uint64_t pos = 0;
        std::size_t w = 0;
        std::vector<InputEvent> events;
        while (pos < total)
        {
            int n = block > 0 ? block : sizes (rng);
            n = (int) std::min<uint64_t> ((uint64_t) n, total - pos);
            events.clear();
            while (w < wheel.size() && wheel[w].first < pos + (uint64_t) n)
            {
                events.push_back ({ (int) (wheel[w].first >= pos ? wheel[w].first - pos : 0), wheel[w].second });
                ++w;
            }
            sink.base = e.samplesDone();
            e.render (n, events.data(), (int) events.size(), sink);
            if (each) each (e);
            pos += (uint64_t) n;
        }
    }

    // Watches one output: trip count (entries into the + side, with hysteresis), time on the + side, and the
    // smallest range seen in any window of a given length.
    struct Watch
    {
        int output = 0;
        double windowSeconds = 60.0;
        double sampleRate = 48000.0;
        int side = 0;
        long trips = 0;
        double firstTrip = -1.0, lastTrip = -1.0;
        double plusSamples = 0.0, allSamples = 0.0;
        double wMin = 1e9, wMax = -1e9, wStart = 0.0, minRange = 1e9;
        double lowest = 1e9, highest = -1e9;
        bool everOutside = false;

        void see (double t, double v, double dtSamples)
        {
            if (v < -1.0 || v > 1.0 || ! std::isfinite (v)) everOutside = true;
            lowest = std::min (lowest, v); highest = std::max (highest, v);
            allSamples += dtSamples;
            if (v > 0.0) plusSamples += dtSamples;
            const int ns = v > 0.15 ? 1 : (v < -0.15 ? -1 : side);
            if (ns == 1 && side == -1)
            {
                ++trips;
                if (firstTrip < 0.0) firstTrip = t;
                lastTrip = t;
            }
            side = ns;
            wMin = std::min (wMin, v); wMax = std::max (wMax, v);
            if (t - wStart >= windowSeconds)
            {
                minRange = std::min (minRange, wMax - wMin);
                wMin = 1e9; wMax = -1e9; wStart = t;
            }
        }
        double meanTrip() const { return trips > 1 ? (lastTrip - firstTrip) / (double) (trips - 1) : 0.0; }
        double plusShare() const { return allSamples > 0 ? plusSamples / allSamples : 0.0; }
    };

    std::string fmt (const char* f, double a, double b = 0.0, double c = 0.0)
    {
        char buf[256];
        std::snprintf (buf, sizeof (buf), f, a, b, c);
        return buf;
    }
}

int main()
{
    const auto started = std::chrono::steady_clock::now();

    // ---- Basic laws ---------------------------------------------------------------------------------------------
    expect (std::fabs (speedFactor (0.0) - 0.125) < 1e-12, "Speed fully anticlockwise is 1/8 x");
    expect (std::fabs (speedFactor (0.5) - 1.0) < 1e-12, "Speed centre is 1x");
    expect (std::fabs (speedFactor (1.0) - 512.0) < 1e-9, "Speed fully clockwise is 512x");
    expect (std::fabs (speedFactor (knobForSpeed (64.0)) - 64.0) < 1e-9, "Speed knob law round trip (64x)");
    {
        bool mono = true, under = true, ident = true;
        double prev = -1e9;
        for (double p = -40.0; p <= 40.0; p += 0.01)
        {
            const double s = softLimit (p);
            if (s < prev) mono = false;
            if (std::fabs (s) >= kPushCeiling) under = false;
            if (std::fabs (p) <= kKnee && s != p) ident = false;
            prev = s;
        }
        expect (mono && under && ident, "push soft limit: rises steadily, unchanged below 8, never reaches 11");
    }
    for (int i = 0; i < kNumOutputs; ++i)
        expect (coarseCc (i) >= 106 && coarseCc (i) <= 112 && fineCc (i) >= 113 && fineCc (i) <= 119, "CC numbers in 106-119");

    // ---- Trip time in circuit units (long direct run of one circuit, no push) -----------------------------------
    {
        Circuit k;
        k.advance (50.0, 0.0);
        int side = 1; long trips = 0; double first = -1, last = -1, t = 0;
        const double h = 0.002;
        for (long i = 0; i < 50000000L; ++i)        // 100,000 circuit units, about 28,000 trips
        {
            k.step (h, 0.0);
            t += h;
            const int ns = k.x > 3.0 ? 1 : (k.x < -3.0 ? -1 : side);
            if (ns == 1 && side == -1) { ++trips; if (first < 0) first = t; last = t; }
            side = ns;
        }
        const double mean = (last - first) / (double) (trips - 1);
        std::printf ("trip: %.4f circuit units on average (%ld trips)\n", mean, trips);
        expect (std::fabs (mean - kMeanTripUnits) < 0.04 * kMeanTripUnits, fmt ("mean trip %.3f units within 4%% of the constant used", mean));
    }

    // ---- Hours at 1x: trip times, bounded, never stalls, both wings, at three sample rates -----------------------
    const double rates[3] = { 44100.0, 48000.0, 96000.0 };
    double gotTrip[3] = { 0, 0, 0 };
    for (int r = 0; r < 3; ++r)
    {
        const double sr = rates[r];
        Engine e;
        e.prepare (sr);
        e.seed (100 + (uint64_t) r);
        const double hours = 10.0;
        Watch w[3];
        for (int c = 0; c < 3; ++c) { w[c].output = c * 2; w[c].sampleRate = sr; }
        w[0].windowSeconds = 120.0;          // ripple must move clearly in every 2 minutes
        w[1].windowSeconds = 480.0;          // tide in every 8 minutes
        w[2].windowSeconds = 2.0 * 3600.0;   // glacier in every 2 hours
        Watch blendW; blendW.output = kOutBlend; blendW.windowSeconds = 1e12;
        bool blendOk = true, yOk = true;
        double t = 0.0;
        uint64_t lastSamples = 0;
        run (e, sr, hours * 3600.0, 0, nullptr, [&] (const Engine& en)
        {
            const double d = (double) (en.samplesDone() - lastSamples);
            lastSamples = en.samplesDone();
            t = (double) en.samplesDone() / sr;
            for (int c = 0; c < 3; ++c) w[c].see (t, en.output (c * 2), d);
            for (int c = 0; c < 3; ++c) if (en.output (c * 2 + 1) < -1.0 || en.output (c * 2 + 1) > 1.0) yOk = false;
            blendW.see (t, en.output (kOutBlend), d);
            if (en.output (kOutBlend) != Engine::blendOf (en.output (kOutRippleY), en.output (kOutTideY), en.output (kOutGlacierY))) blendOk = false;
        }, {}, 11u + (uint32_t) r);

        const char* names[3] = { "ripple", "tide", "glacier" };
        const double lo[3] = { 15.0, 60.0, 30.0 * 60.0 }, hi[3] = { 30.0, 90.0, 40.0 * 60.0 };
        for (int c = 0; c < 3; ++c)
        {
            const std::string tag = std::string (names[c]) + fmt (" at %.1f kHz: ", sr / 1000.0);
            expect (! w[c].everOutside && yOk, tag + "always inside -1..+1");
            expect (w[c].minRange > (c == 2 ? 0.2 : 0.3), tag + fmt ("keeps moving in every window (smallest range %.2f)", w[c].minRange));
            expect (w[c].plusShare() > 0.2 && w[c].plusShare() < 0.8, tag + fmt ("visits both wings (%.0f%% on the + side)", 100.0 * w[c].plusShare()));
            if (c < 2)
            {
                expect (w[c].meanTrip() >= lo[c] && w[c].meanTrip() <= hi[c],
                        tag + fmt ("mean trip %.1f s (module: %.0f-%.0f s)", w[c].meanTrip(), lo[c], hi[c]));
                gotTrip[c] += w[c].meanTrip() / 3.0;
            }
            else
                expect (w[c].trips >= 10, tag + fmt ("%.0f trips in 10 hours", (double) w[c].trips));
        }
        expect (blendOk, fmt ("blend always equals tide + glacier - ripple (Z), halved, at %.1f kHz", sr / 1000.0));
        expect (blendW.lowest < -0.3 && blendW.highest > 0.3, fmt ("blend swings both ways (%.2f to %.2f)", blendW.lowest, blendW.highest));
        expect (e.rescues() == 0, "no circuit ever needed a restart");
        expect (std::llabs ((long long) e.stepsDone() - (long long) (hours * 3600.0 * 1000.0)) <= 1, fmt ("exactly 1000 steps a second at %.1f kHz", sr / 1000.0));
    }
    std::printf ("trip at 1x: ripple %.1f s, tide %.1f s (average of the three sample rates)\n", gotTrip[0], gotTrip[1]);

    // Glacier's trip time: Speed scales time exactly, so glacier's trip = (trip in circuit units) x its scale.
    {
        Engine e;
        e.prepare (48000.0);
        Settings s; s.speed = 0.5;
        e.setSettings (s);
        run (e, 48000.0, 600.0, 512);
        const double perSecond = e.unitsDone (2) / 600.0;
        const double trip = kMeanTripUnits / perSecond;
        expect (trip > 30.0 * 60.0 && trip < 40.0 * 60.0, fmt ("glacier mean trip %.1f min at 1x (module: 30-40 min)", trip / 60.0));
        std::printf ("trip at 1x: glacier %.1f min\n", trip / 60.0);
        for (double sp : { 0.125, 4.0, 64.0, 512.0 })
        {
            Engine f;
            f.prepare (48000.0);
            Settings t2; t2.speed = knobForSpeed (sp);
            f.setSettings (t2);
            run (f, 48000.0, 10.0, 256);
            bool exact = true;
            for (int c = 0; c < 3; ++c)
            {
                const double want = 10.0 * sp / (kTripSeconds[c] / kMeanTripUnits);
                if (std::fabs (f.unitsDone (c) - want) > 1e-9 * want) exact = false;
            }
            expect (exact, fmt ("Speed %.3fx moves every circuit exactly %.3f times as fast", sp, sp));
        }
        Engine g;
        g.prepare (48000.0);
        Settings t3; t3.speed = 1.0;
        g.setSettings (t3);
        Watch gw; gw.output = kOutGlacierX; gw.windowSeconds = 1e12;
        double t = 0.0;
        uint64_t last = 0;
        run (g, 48000.0, 120.0, 512, nullptr, [&] (const Engine& en)
        {
            const double d = (double) (en.samplesDone() - last); last = en.samplesDone();
            t = (double) en.samplesDone() / 48000.0;
            gw.see (t, en.output (kOutGlacierX), d);
        });
        expect (gw.trips >= 15 && gw.trips <= 50, fmt ("at 512x glacier makes a trip every few seconds (%.0f trips in 2 minutes)", (double) gw.trips));
    }

    // ---- Every Lean / Nudge combination: keeps moving, never runs away, both wings, leans the right way ---------
    {
        const double sp = 64.0;
        const double seconds = 200.0;              // at 64x: about 640 ripple trips and 170 tide trips
        int combos = 0, bad = 0;
        double plusAt[5] = { 0, 0, 0, 0, 0 };
        const double leans[5] = { -1.0, -0.5, 0.0, 0.5, 1.0 };
        std::vector<double> amounts { 0.0, -1.0, 1.0 };
        for (int li = 0; li < 5; ++li)
            for (double amt : amounts)
                for (int src = 0; src < kNumNudgeSources * 2; ++src)
                {
                    // src 0..5: both circuits from the same menu entry (2 and 3 = ripple and tide nudging each other);
                    // 6..11: the same with the two Nudge knobs turned opposite ways.
                    if (amt == 0.0 && src > 0) continue;
                    const bool opposite = src >= kNumNudgeSources;
                    Engine e;
                    e.prepare (48000.0);
                    e.seed (1000 + (uint64_t) combos);
                    Settings s;
                    s.speed = knobForSpeed (sp);
                    for (int c = 0; c < 2; ++c) { s.lean[c] = leans[li]; s.nudge[c] = (opposite && c == 1) ? -amt : amt; s.nudgeFrom[c] = src % kNumNudgeSources; }
                    e.setSettings (s);
                    std::vector<std::pair<uint64_t, double>> wheel;
                    if (src % kNumNudgeSources == (int) NudgeFrom::modWheel)          // the mod wheel held fully up, then moved about
                    {
                        wheel.push_back ({ 0, 1.0 });
                        for (int k = 1; k < 40; ++k) wheel.push_back ({ (uint64_t) (k * 5 * 48000), (k % 3) / 2.0 });
                        wheel.push_back ({ (uint64_t) (100 * 48000), 1.0 });
                    }
                    Watch w[2];
                    w[0].output = 0; w[0].windowSeconds = 20.0 * 20.0 / sp;     // about 20 trips
                    w[1].output = 2; w[1].windowSeconds = 20.0 * 75.0 / sp;
                    bool outside = false;
                    uint64_t last = 0;
                    run (e, 48000.0, seconds, 0, nullptr, [&] (const Engine& en)
                    {
                        const double d = (double) (en.samplesDone() - last); last = en.samplesDone();
                        const double t = (double) en.samplesDone() / 48000.0;
                        for (int c = 0; c < 2; ++c) w[c].see (t, en.output (c * 2), d);
                        for (int i = 0; i < kNumOutputs; ++i) if (! (en.output (i) >= -1.0 && en.output (i) <= 1.0)) outside = true;
                        for (int c = 0; c < 2; ++c) if (std::fabs (en.push (c)) >= kPushCeiling) outside = true;
                    }, wheel, (uint32_t) combos);
                    ++combos;
                    bool ok = ! outside && e.rescues() == 0;
                    for (int c = 0; c < 2; ++c)
                        ok = ok && w[c].minRange > 0.3 && w[c].plusShare() > 0.05 && w[c].plusShare() < 0.95 && w[c].trips > 20;
                    if (! ok)
                    {
                        ++bad;
                        std::printf ("  combo lean %.1f nudge %.0f from %d: ranges %.2f %.2f, + side %.2f %.2f, trips %ld %ld, rescues %d\n",
                                     leans[li], amt, src, w[0].minRange, w[1].minRange, w[0].plusShare(), w[1].plusShare(),
                                     w[0].trips, w[1].trips, e.rescues());
                    }
                    if (amt == 0.0) plusAt[li] = 0.5 * (w[0].plusShare() + w[1].plusShare());
                }
        expect (bad == 0, fmt ("all %.0f Lean / Nudge combinations keep moving, stay in range and visit both wings (%.0f did not)", combos, bad));
        expect (plusAt[0] < 0.42 && plusAt[1] < plusAt[2] && plusAt[2] < plusAt[3] && plusAt[4] > 0.58,
                fmt ("Lean leans: %.0f%% on the + side fully left, %.0f%% centred", 100 * plusAt[0], 100 * plusAt[2]) + fmt (", %.0f%% fully right", 100 * plusAt[4]));
        std::printf ("lean: + side share %.2f %.2f %.2f %.2f %.2f (lean -1 .. +1)\n", plusAt[0], plusAt[1], plusAt[2], plusAt[3], plusAt[4]);
    }

    // ---- Same chaos at every sample rate; same MIDI at every block size -----------------------------------------
    {
        Settings s;
        s.speed = knobForSpeed (16.0); s.lean[0] = 0.4; s.nudge[1] = -0.7; s.nudgeFrom[1] = (int) NudgeFrom::secondX;
        Circuit ref[3];
        for (int r = 0; r < 3; ++r)
        {
            Engine e;
            e.prepare (rates[r]);
            e.seed (77);
            e.setSettings (s);
            run (e, rates[r], 300.0, 0, nullptr, nullptr, {}, 3u + (uint32_t) r);
            if (r == 0) for (int c = 0; c < 3; ++c) ref[c] = e.circuit (c);
            else
            {
                bool same = true;
                for (int c = 0; c < 3; ++c)
                    same = same && e.circuit (c).x == ref[c].x && e.circuit (c).y == ref[c].y && e.circuit (c).z == ref[c].z;
                expect (same, fmt ("after 5 minutes at %.1f kHz the chaos is exactly where it is at 44.1 kHz", rates[r] / 1000.0));
            }
        }

        std::vector<std::pair<uint64_t, double>> wheel;
        for (int k = 0; k < 50; ++k) wheel.push_back ({ (uint64_t) (k * 23456 + 17), (k % 5) / 4.0 });
        Settings s2 = s;
        s2.nudgeFrom[0] = (int) NudgeFrom::modWheel; s2.nudge[0] = 0.8;
        std::vector<Msg> refMsgs;
        const int blocks[] = { 0, 1, 7, 64, 512, 4096 };
        for (int b : blocks)
        {
            Engine e;
            e.prepare (48000.0);
            e.seed (5);
            e.setSettings (s2);
            std::vector<Msg> msgs;
            run (e, 48000.0, 60.0, b, &msgs, nullptr, wheel, 99u);
            if (b == 0) refMsgs = msgs;
            else expect (msgs == refMsgs, "identical MIDI with blocks of " + std::to_string (b) + " samples as with random blocks");
        }
        expect (refMsgs.size() > 1000, "the comparison stream is not empty");
    }

    // ---- Save and reload; New start ------------------------------------------------------------------------------
    {
        Settings s; s.speed = knobForSpeed (32.0); s.nudge[0] = 0.5; s.nudgeFrom[0] = (int) NudgeFrom::firstY;
        Engine a;
        a.prepare (48000.0); a.seed (9); a.setSettings (s);
        run (a, 48000.0, 33.3, 0);
        const std::string saved = a.saveState();
        Engine b;
        b.prepare (48000.0); b.seed (12345); b.setSettings (s);
        expect (b.loadState (saved), "a saved position loads");
        expect (b.saveState() == saved, "the loaded position saves back to exactly the same text");
        bool traceSame = true;
        for (int c = 0; c < 3; ++c)
            for (int k = 0; k < Engine::kTracePoints; ++k)
                traceSame = traceSame && std::fabs (a.traceX (c, k) - b.traceX (c, k)) < 1e-4f && std::fabs (a.traceY (c, k) - b.traceY (c, k)) < 1e-4f;
        expect (traceSame && b.traceCount (0) == a.traceCount (0), "the Orbits trace comes back with the position");
        a.prepare (48000.0);
        b.prepare (48000.0);
        std::vector<Msg> ma, mb;
        run (a, 48000.0, 60.0, 0, &ma);
        run (b, 48000.0, 60.0, 333, &mb);
        expect (ma == mb && ! ma.empty(), "after reloading, the chaos continues exactly as if it had never stopped");

        Engine t1; t1.seed (444);
        t1.takePositionFrom (b);
        expect (t1.saveState() == b.saveState(), "taking over another engine's position copies it exactly");

        Engine c;
        c.prepare (48000.0); c.seed (9);
        const std::string before = c.saveState();
        for (const std::string bad : { std::string(), std::string ("SD1"), std::string ("hello"), saved.substr (0, saved.size() / 2),
                                       std::string ("SD1 nan nan nan 0 1 0 0 x") })
            expect (! c.loadState (bad) && c.saveState() == before, "a broken saved state is refused and changes nothing");

        Engine d1, d2, d3;
        d1.seed (1); d2.seed (2); d3.seed (1);
        expect (d1.circuit (0).x != d2.circuit (0).x && d1.circuit (2).z != d2.circuit (2).z, "New start: a different seed starts somewhere else");
        expect (d1.saveState() == d3.saveState(), "the same seed starts at the same place");
        int seedsBothWings = 0;
        for (uint64_t k = 0; k < 40; ++k) { Engine e; e.seed (k * 7919); if (e.output (0) > 0) ++seedsBothWings; }
        expect (seedsBothWings > 8 && seedsBothWings < 32, "New start lands in either wing");
    }

    // ---- Hold ----------------------------------------------------------------------------------------------------
    {
        Engine e;
        e.prepare (48000.0); e.seed (3);
        Settings s; s.speed = knobForSpeed (64.0);
        e.setSettings (s);
        run (e, 48000.0, 5.0, 256);
        s.hold = true; e.setSettings (s);
        run (e, 48000.0, 0.1, 256);
        const std::string frozen = e.saveState();
        std::vector<Msg> during;
        run (e, 48000.0, 10.0, 256, &during);
        expect (e.saveState() == frozen, "Hold freezes all three exactly");
        expect (during.empty(), "nothing is sent while held");
        s.hold = false; e.setSettings (s);
        std::vector<Msg> after;
        run (e, 48000.0, 1.0, 256, &after);
        expect (! after.empty(), "after Hold the circuits move on from where they were");
        Engine f;
        f.prepare (48000.0); f.loadState (frozen); f.setSettings (s);
        f.prepare (48000.0);
        e.prepare (48000.0);
        e.loadState (frozen);
        std::vector<Msg> x1, x2;
        run (e, 48000.0, 2.0, 100, &x1); run (f, 48000.0, 2.0, 100, &x2);
        expect (x1 == x2, "resuming after Hold continues the same path");
    }

    // ---- The mod wheel as a Nudge source --------------------------------------------------------------------------
    {
        Engine e;
        e.prepare (48000.0); e.seed (4);
        Settings s; s.nudge[0] = 1.0; s.nudgeFrom[0] = (int) NudgeFrom::modWheel;
        e.setSettings (s);
        run (e, 48000.0, 0.5, 128);
        expect (e.push (0) == 0.0, "mod wheel at rest: no push");
        run (e, 48000.0, 0.5, 128, nullptr, nullptr, { { 100, 1.0 } });
        expect (std::fabs (e.push (0) - softLimit (10.0)) < 1e-12, "mod wheel fully up pushes ripple fully (soft-limited)");
        expect (e.push (1) == 0.0 && e.push (2) == 0.0, "the mod wheel moves only the circuit it is chosen for");
    }

    // ---- CC rules (copied from Ogdoad) ----------------------------------------------------------------------------
    for (double sp : { 1.0, 512.0 })
    {
        Engine e;
        e.prepare (48000.0); e.seed (21);
        Settings s; s.speed = knobForSpeed (sp);
        e.setSettings (s);
        std::vector<Msg> msgs;
        run (e, 48000.0, sp > 1.0 ? 60.0 : 1800.0, 0, &msgs);
        bool onlyOurs = true, gapOk = true, coarseFirst = true;
        int lastFine[kNumOutputs]; uint64_t lastTime[kNumOutputs]; int coarse[kNumOutputs]; int maxJump[kNumOutputs];
        for (int i = 0; i < kNumOutputs; ++i) { lastFine[i] = -1; lastTime[i] = 0; coarse[i] = -1; maxJump[i] = 0; }
        int lastValue[kNumOutputs]; for (int& v : lastValue) v = -1;
        for (std::size_t m = 0; m < msgs.size(); ++m)
        {
            const auto& g = msgs[m];
            if (g.cc < 106 || g.cc > 119) { onlyOurs = false; continue; }
            if (g.cc <= 112)
            {
                const int i = g.cc - 106;
                coarse[i] = g.value;
                if (! (m + 1 < msgs.size() && msgs[m + 1].cc == g.cc + 7 && msgs[m + 1].sample == g.sample)) coarseFirst = false;
                continue;
            }
            const int i = g.cc - 113;
            if (lastFine[i] >= 0 && (double) (g.sample - lastTime[i]) < 0.006 * 48000.0 - 1.0) gapOk = false;
            const int v = coarse[i] * 128 + g.value;
            if (lastValue[i] >= 0) maxJump[i] = std::max (maxJump[i], std::abs (v - lastValue[i]));
            lastValue[i] = v; lastFine[i] = g.value; lastTime[i] = g.sample;
        }
        const std::string tag = fmt ("at %.0fx: ", sp);
        expect (onlyOurs, tag + "only CC 106-119 are sent");
        expect (gapOk, tag + "each output at most once every 6 ms");
        expect (coarseFirst, tag + "the coarse part always comes just before its fine part, at the same sample");
        bool finalOk = true;
        for (int i = 0; i < kNumOutputs; ++i) if (lastValue[i] != e.ccStream().lastSent (i)) finalOk = false;
        expect (finalOk, tag + "the receiver ends up with exactly the engine's values");
        if (sp == 1.0)
        {
            expect (maxJump[kOutGlacierX] <= 2 && maxJump[kOutGlacierY] <= 2,
                    fmt ("glacier moves in steps of at most 2 of 16,384 (largest %.0f and %.0f)", maxJump[kOutGlacierX], maxJump[kOutGlacierY]));
            expect (maxJump[kOutTideX] <= 40 && maxJump[kOutTideY] <= 40,
                    fmt ("tide moves in steps of at most 40 of 16,384 (largest %.0f and %.0f)", maxJump[kOutTideX], maxJump[kOutTideY]));
            std::printf ("smoothness at 1x, largest step per message (of 16,384): ripple %d %d, tide %d %d, glacier %d %d, blend %d\n",
                         maxJump[0], maxJump[1], maxJump[2], maxJump[3], maxJump[4], maxJump[5], maxJump[6]);
        }
        const double rate = (double) msgs.size() / (sp > 1.0 ? 60.0 : 1800.0);
        if (sp == 1.0) std::printf ("MIDI: %.0f messages a second at 1x", rate);
        else std::printf (", %.0f at 512x\n", rate);
    }

    // ---- CPU (worst case: 512x, both nudges on, 96 kHz, 64-sample blocks) ------------------------------------------
    {
        Engine e;
        e.prepare (96000.0); e.seed (8);
        Settings s; s.speed = 1.0;
        s.nudge[0] = 1.0; s.nudgeFrom[0] = (int) NudgeFrom::secondX;
        s.nudge[1] = -1.0; s.nudgeFrom[1] = (int) NudgeFrom::firstY;
        e.setSettings (s);
        const double seconds = 120.0;
        const auto t0 = std::chrono::steady_clock::now();
        run (e, 96000.0, seconds, 64);
        const double used = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double percent = 100.0 * used / seconds;
        std::printf ("CPU: %.4f%% of one core at 96 kHz, 64-sample blocks, worst-case settings\n", percent);
        expect (percent < 1.0, "CPU under 1% of one core (measured " + std::to_string (percent) + "%)");
    }

    const double took = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    std::printf ("engine tests: %d checks, %d failed (%.0f s)\n", checks, failures, took);
    if (failures == 0) std::printf ("engine tests: all %d checks passed\n", checks);
    return failures == 0 ? 0 : 1;
}
