// MzPCF - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Settings.h"

#include <atomic>

namespace pcf
{

constexpr int patterns = 32;
constexpr int steps    = 16;

// A pattern's level on a step, 0 to 1.
float level(int pattern, int step);

//
// The pattern-controlled filter. A state-variable filter whose cutoff is opened by an
// envelope, struck on every sixteenth of the host's timeline at that step's level in the
// pattern and dying away by DECAY - so the filter plays a rhythm of its own over whatever
// goes through it.
//
// It follows the host's transport, as ReBirth's PCF followed its own: stopped, nothing is
// struck and the filter settles back to FREQ.
//
class Pcf
{
public:
    void prepare(double sampleRate);
    void reset();

    void setSettings(const Settings& s) { target = s; }

    // Where the host's timeline is at the top of the next block. The clock is run forward
    // sample by sample from there, and put right again every block, so a loop or a tempo
    // change is followed within one block.
    void setHostClock(bool playing, double ppq, double bpm);

    // In place. right may be null, for a mono channel.
    void process(float* left, float* right, int n);

    // any thread: the step last struck, for the panel's lights; -1 stopped
    int step() const { return lastStruck.load(std::memory_order_relaxed); }

    // the envelope as it stands, 0 to 1 - for the checks
    float envelope() const { return shaped; }

private:
    void strike(long long sixteenth);

    double fs = 48000.0;
    Settings target;

    // the clock: where this block starts, in sixteenths, and how far one sample moves it
    bool playing = false;
    double position = 0.0, perSample = 0.0;
    long long current = 0;    // the sixteenth the last sample fell in (negative in a pre-roll)
    bool haveCurrent = false; // false until the first sample after starting
    std::atomic<int> lastStruck { -1 };

    // the envelope: struck, decaying, and rounded by a third of a millisecond's attack so
    // a strike opens the filter without a click
    float env = 0.0f, shaped = 0.0f, attackCoef = 0.0f;

    // the knobs, smoothed, so turning one does not zip
    float octaves = 0.0f, reach = 0.0f, damping = 1.0f, wet = 1.0f, gain = 1.0f;
    float smoothCoef = 0.0f;
    bool settled = false;

    struct Svf { float ic1 = 0.0f, ic2 = 0.0f; };
    Svf state[2];
};

} // namespace pcf
