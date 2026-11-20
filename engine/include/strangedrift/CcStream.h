// Turns slowly moving values into a smooth, thin stream of MIDI CC messages (copied from Ogdoad's CC sender).
//
// Each output ("lane") is sent as a 14-bit CC pair: a coarse part (MSB) and a fine part (LSB), 16,384 steps instead
// of 128. Strange Drift's pairs are listed in Outputs.h (coarse CC 106-112, fine CC 113-119).
//
// Rules, per lane:
//   - send only when the 14-bit value has changed;
//   - at most once every kMinIntervalSeconds (6 ms, about one Logic audio block), so Scripter is never flooded;
//   - the coarse part is sent only when it changed, and always immediately before the fine part, at the same sample;
//     the fine part is always sent, so a receiver acts when the fine part arrives.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace strangedrift
{
    struct CcSink
    {
        virtual ~CcSink() = default;
        virtual void cc (int sampleOffset, int ccNumber, int value) = 0;
    };

    inline constexpr int kMaxCcLanes = 16;
    inline constexpr double kMinIntervalSeconds = 0.006;

    // -1..+1 to 0..16383
    inline int toFourteenBit (double v)
    {
        double x = (v + 1.0) * 0.5 * 16383.0 + 0.5;
        if (x < 0.0) x = 0.0;
        if (x > 16383.0) x = 16383.0;
        return (int) x;
    }

    class CcStream
    {
    public:
        void setLane (int lane, int msbNumber, int lsbNumber);
        void reset();            // forget what was sent: the next update sends everything
        void setNumLanes (int n) { numLanes = n; }

        // Called once per engine tick. dtSeconds = time since the previous tick.
        void update (int lane, double value, double dtSeconds, int offset, CcSink& sink);

        int lastSent (int lane) const { return lanes[(std::size_t) lane].last; }

    private:
        struct Lane { int msbCc = -1, lsbCc = -1, last = -1, lastMsb = -1; double sinceSend = 1.0e9; };
        std::array<Lane, kMaxCcLanes> lanes {};
        int numLanes = 0;
    };
}
