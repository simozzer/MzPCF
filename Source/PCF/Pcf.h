// MzPCF - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Settings.h"
#include "StepClock.h"

#include <atomic>

namespace pcf
{

constexpr int patterns = 32;
constexpr int steps    = 16;

// A pattern's level on a step, 0 to 1.
float level(int pattern, int step);

//
// The envelope that plays the filter: struck to a step's level, dying away by DECAY, and
// rounded by a third of a millisecond's attack so a strike opens the filter without a click.
//
class Envelope
{
public:
    void prepare(double sampleRate);
    void reset() { env = shaped = 0.0f; }

    void setDecay(float decay);   // 0 to 1: 15 ms to 1.5 s
    void strike(float level) { env = level; }

    float next()
    {
        shaped += (env - shaped) * attackCoef;
        env *= decayCoef;
        return shaped;
    }

    float value() const { return shaped; }

private:
    double fs = 48000.0;
    float env = 0.0f, shaped = 0.0f;
    float attackCoef = 0.0f, decayCoef = 0.0f;
};

//
// The filter: a state-variable filter whose cutoff the envelope opens, by AMT octaves above
// FREQ at full level. Its knobs are smoothed here, so turning one does not zip. One envelope
// can play any number of these - the rack has one per channel.
//
class Filter
{
public:
    void prepare(double sampleRate);
    void reset();

    // In place. env: the envelope, a value a sample. right may be null, for a mono channel.
    void process(float* left, float* right, const float* env, int n, const Settings&);

private:
    double fs = 48000.0;
    float octaves = 0.0f, reach = 0.0f, damping = 1.0f, wet = 1.0f, gain = 1.0f;
    float smoothCoef = 0.0f;
    bool settled = false;

    struct Svf { float ic1 = 0.0f, ic2 = 0.0f; };
    Svf state[2];
};

//
// The whole PCF, as the plugin plays it: the envelope struck on every sixteenth of the
// host's timeline at that step's level in the pattern, playing the filter - so the filter
// plays a rhythm of its own over whatever goes through it.
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
    float envelope() const { return env.value(); }

private:
    void strike(long long sixteenth);

    Settings target;
    StepClock clock;          // in sixteenths
    std::atomic<int> lastStruck { -1 };

    Envelope env;
    Filter filter;
};

} // namespace pcf
