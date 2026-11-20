// One slow chaos circuit.
//
// Our own design, not taken from any module, schematic or software: the textbook Lorenz system (E. Lorenz, 1963), the
// classic "butterfly" with two wings, i.e. two strange attractors the signal wanders around, switching from one to the
// other at unpredictable moments. It is used here because it can never run away: every path, from any starting point
// and with any of the bounded pushes used below, ends up inside the same bounded region.
//
//   dx/dt = sigma (y - x) + push          push = Lean + Nudge, through softLimit()
//   dy/dt = x (rho - z) - y
//   dz/dt = x y - beta z
//
// X is taken from x (which wing it is in), Y from z (how far out it is swinging): two different stages of the same
// circuit. Lean and Nudge add a push to the first line. A steady push makes the signal spend longer in one wing; too big
// a push would let it settle to a standstill in that wing, so the total push is soft-limited well below that point
// (it stalls at about 15; the limit is 11, and the long-run tests prove it keeps moving).
//
// Time is in "circuit units": one trip around both wings takes kMeanTripUnits on average. Each circuit maps circuit
// units to seconds with its own scale, so the three run at about 20 s, 75 s and 35 min per trip at Speed 1x.

#pragma once

#include <cmath>
#include <cstdint>

namespace strangedrift
{
    inline constexpr double kSigma = 10.0, kRho = 28.0, kBeta = 8.0 / 3.0;

    inline constexpr double kMeanTripUnits = 3.485;     // measured (engine test): mean time between entries into the + wing

    // Seconds per trip at Speed 1x, for ripple, tide and glacier (the module's documented ranges: 15-30 s, 60-90 s,
    // 30-40 min).
    inline constexpr double kTripSeconds[3] = { 20.0, 75.0, 35.0 * 60.0 };

    inline constexpr double kMaxStepUnits = 0.005;      // the largest single integration step, in circuit units

    // Lean and Nudge: knob +-1 gives a push of +-kPushPerKnob. The sum goes through softLimit (straight up to kKnee,
    // then bending smoothly towards kPushCeiling, which it never reaches).
    inline constexpr double kPushPerKnob = 10.0, kKnee = 8.0, kPushCeiling = 11.0;

    inline double softLimit (double p)
    {
        const double a = std::fabs (p);
        if (a <= kKnee) return p;
        const double room = kPushCeiling - kKnee;
        const double out = kKnee + room * std::tanh ((a - kKnee) / room);
        return p < 0.0 ? -out : out;
    }

    // Output scaling: X = x / 20, Y = (z - 24) / 24, both kept inside -1..+1.
    inline double clampUnit (double v) { return v < -1.0 ? -1.0 : (v > 1.0 ? 1.0 : v); }

    struct Circuit
    {
        double x = 1.0, y = 1.0, z = 20.0;

        void derivative (double px, double py, double pz, double push, double& dx, double& dy, double& dz) const
        {
            dx = kSigma * (py - px) + push;
            dy = px * (kRho - pz) - py;
            dz = px * py - kBeta * pz;
        }

        // One fourth-order Runge-Kutta step of h circuit units with a fixed push.
        void step (double h, double push)
        {
            double k1x, k1y, k1z, k2x, k2y, k2z, k3x, k3y, k3z, k4x, k4y, k4z;
            derivative (x, y, z, push, k1x, k1y, k1z);
            derivative (x + 0.5 * h * k1x, y + 0.5 * h * k1y, z + 0.5 * h * k1z, push, k2x, k2y, k2z);
            derivative (x + 0.5 * h * k2x, y + 0.5 * h * k2y, z + 0.5 * h * k2z, push, k3x, k3y, k3z);
            derivative (x + h * k3x, y + h * k3y, z + h * k3z, push, k4x, k4y, k4z);
            x += h / 6.0 * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);
            y += h / 6.0 * (k1y + 2.0 * k2y + 2.0 * k3y + k4y);
            z += h / 6.0 * (k1z + 2.0 * k2z + 2.0 * k3z + k4z);
        }

        // Advances by `units` circuit units, in equal steps no larger than kMaxStepUnits.
        void advance (double units, double push)
        {
            if (units <= 0.0) return;
            const int n = (int) std::ceil (units / kMaxStepUnits - 1.0e-9);
            const double h = units / (double) (n < 1 ? 1 : n);
            for (int i = 0; i < (n < 1 ? 1 : n); ++i) step (h, push);
        }

        bool healthy() const
        {
            return std::isfinite (x) && std::isfinite (y) && std::isfinite (z)
                && std::fabs (x) < 200.0 && std::fabs (y) < 300.0 && z > -100.0 && z < 300.0;
        }

        double outX() const { return clampUnit (x / 20.0); }
        double outY() const { return clampUnit ((z - 24.0) / 24.0); }
    };
}
