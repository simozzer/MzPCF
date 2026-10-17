// MzPCF - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <cmath>

namespace pcf
{

//
// The host's timeline, as steps: told where the timeline is at the top of each block, it
// says on which samples of the block a step begins. The clock is run forward sample by
// sample from there, and put right again every block, so a loop or a tempo change is
// followed within one block.
//
// A step lands on its first sample. Arriving in the middle of one - starting playback there,
// or a jump - waits for the next, as a step sequencer would; arriving on its first sample,
// as a loop back to the bar does, strikes it. Steps are counted from the top of the song,
// so step 0 of a pattern is always the first of a bar.
//
class StepClock
{
public:
    void prepare(double sampleRate) { fs = sampleRate > 0.0 ? sampleRate : 48000.0; reset(); }
    void reset() { haveCurrent = false; }

    // where the timeline is at the top of the next block; perQuarter steps to the beat
    void set(bool isPlaying, double ppq, double bpm, int perQuarter = 4)
    {
        playing = isPlaying && bpm > 0.0 && std::isfinite(ppq) && perQuarter > 0;
        if (! playing) { haveCurrent = false; return; }

        if (perQuarter != stepsPerQuarter) haveCurrent = false;   // a new grid: start it afresh
        stepsPerQuarter = perQuarter;
        position = ppq * perQuarter;
        perSample = bpm / 60.0 * perQuarter / fs;
    }

    bool isPlaying() const { return playing; }
    double samplesPerStep() const { return perSample > 0.0 ? 1.0 / perSample : fs; }

    // Whether a step begins on sample i of this block, and which.
    bool stepAt(int i, long long& step)
    {
        if (! playing) return false;

        const double at = position + (double) i * perSample;
        const long long s = (long long) std::floor(at + 1.0e-9);   // due on this sample, give or take rounding
        if (haveCurrent && s == current) return false;

        const bool next = haveCurrent && s == current + 1;
        const bool onTheStep = at - (double) s < perSample;
        current = s;
        haveCurrent = true;
        step = s;
        return next || onTheStep;
    }

private:
    double fs = 48000.0;
    bool playing = false;
    int stepsPerQuarter = 4;
    double position = 0.0, perSample = 0.0;
    long long current = 0;    // the step the last sample fell in (negative in a pre-roll)
    bool haveCurrent = false; // false until the first sample after starting
};

} // namespace pcf
