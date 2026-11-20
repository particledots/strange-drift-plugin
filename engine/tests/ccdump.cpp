// Writes the engine's real CC stream as text, for the mapper test (engine/tests/mapper_test.js).
// Usage: strangedrift_ccdump <seconds> <speed factor>
// Lines: "<sample> <cc> <value>", then "final <output> <14-bit value>" for each of the seven outputs.

#include "strangedrift/Engine.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace strangedrift;

struct PrintSink : CcSink
{
    unsigned long long base = 0;
    void cc (int offset, int number, int value) override { std::printf ("%llu %d %d\n", base + (unsigned long long) offset, number, value); }
};

int main (int argc, char** argv)
{
    const double seconds = argc > 1 ? std::atof (argv[1]) : 30.0;
    const double speed = argc > 2 ? std::atof (argv[2]) : 64.0;
    Engine e;
    e.prepare (48000.0);
    e.seed (2026);
    Settings s;
    const double oct = std::log2 (speed);
    s.speed = oct < 0.0 ? 0.5 + oct / 6.0 : 0.5 + oct / 18.0;
    s.nudge[0] = 0.6; s.nudgeFrom[0] = (int) NudgeFrom::secondX;
    e.setSettings (s);
    PrintSink sink;
    const long total = (long) (seconds * 48000.0);
    for (long pos = 0; pos < total; pos += 512)
    {
        sink.base = e.samplesDone();
        e.render (512, nullptr, 0, sink);
    }
    for (int i = 0; i < kNumOutputs; ++i) std::printf ("final %d %d\n", i, e.ccStream().lastSent (i));
    return 0;
}
