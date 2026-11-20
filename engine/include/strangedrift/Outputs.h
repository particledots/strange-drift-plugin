// Strange Drift's outputs and their MIDI CC numbers. These numbers are fixed from the first build: saved projects and
// the mapper rely on them.
//
// Seven 14-bit outputs are sent, all on one MIDI channel (15 by default):
//
//   output      ripple X  ripple Y  tide X  tide Y  glacier X  glacier Y  blend
//   coarse CC     106       107      108     109      110        111       112
//   fine CC       113       114      115     116      117        118       119
//
// CC 106-119 have no standard job in MIDI and are not used by Ogdoad (CC 12-31, 44-63, 102-105 on channel 16).
// The pairs are not the usual "fine = coarse + 32"; the mapper has its own table.
//
// Z (each circuit's Y turned upside down) and blend + / blend - (blend above or below zero) are worked out by the
// mapper from Y and blend, so they cost no MIDI.

#pragma once

namespace strangedrift
{
    inline constexpr int kNumCircuits = 3;          // 0 ripple (fastest), 1 tide, 2 glacier (slowest, no controls)
    inline constexpr int kNumControlled = 2;        // ripple and tide have Lean and Nudge
    inline constexpr int kNumOutputs = 7;           // X and Y of each circuit, then blend

    inline constexpr int kOutRippleX = 0, kOutRippleY = 1, kOutTideX = 2, kOutTideY = 3,
                         kOutGlacierX = 4, kOutGlacierY = 5, kOutBlend = 6;

    inline constexpr int kFirstCoarseCc = 106;      // output i: coarse CC 106 + i
    inline constexpr int kFirstFineCc   = 113;      // output i: fine CC 113 + i
    inline constexpr int coarseCc (int output) { return kFirstCoarseCc + output; }
    inline constexpr int fineCc (int output)   { return kFirstFineCc + output; }

    inline constexpr int kDefaultChannel = 15;

    // Nudge from: what feeds each Nudge knob. The order is fixed (saved projects store the index).
    // A circuit is never nudged by itself (its own outputs pushing back on it can make it settle and stop), so the
    // menu lists the other two: for ripple "first" = tide and "second" = glacier, for tide "first" = ripple and
    // "second" = glacier.
    enum class NudgeFrom : int { off = 0, modWheel, firstX, firstY, secondX, secondY };
    inline constexpr int kNumNudgeSources = 6;

    // The circuit that "first" and "second" mean for a controlled circuit (0 ripple, 1 tide).
    inline constexpr int nudgeCircuit (int controlled, bool second) { return second ? 2 : (controlled == 0 ? 1 : 0); }
}
