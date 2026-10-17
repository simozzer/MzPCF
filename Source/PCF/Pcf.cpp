// MzPCF - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Pcf.h"

#include <algorithm>
#include <cmath>

namespace pcf
{

namespace
{
    constexpr float pi = 3.14159265358979f;

    // the one-pole coefficient that gets 63% of the way in `seconds`
    float follow(float seconds, double rate)
    {
        return 1.0f - (float) std::exp(-1.0 / (seconds * rate));
    }
}

//============================================================================== patterns

//
// Thirty-two rhythms for the filter, a hex digit a step, 0 to f: the plain ones first - every
// beat, every off-beat, every step - then ramps, then broken ones. The same as the rack's
// (MzDebirth), so a pattern number means the same in both.
//
float level(int pattern, int step)
{
    static const char* const table[patterns] = {
        "f000f000f000f000", "f0f0f0f0f0f0f0f0", "ffffffffffffffff", "0f0f0f0f0f0f0f0f",
        "00f000f000f000f0", "f8f8f8f8f8f8f8f8", "ff00ff00ff00ff00", "f00ff00ff00ff00f",
        "fc84fc84fc84fc84", "48cf48cf48cf48cf", "fedcba9876543210", "0123456789abcdef",
        "fdb97531fdb97531", "13579bdf13579bdf", "f0e0d0c0b0a09080", "fa50fa50fa50fa50",
        "f00f00f00f00f0f0", "f0080f008f00f080", "f000000080000000", "f0a0f0a0f0a0f05f",
        "f0c0f080f0c0f040", "8f8f8f8f8f8f8f8f", "f000f000f0f0f0ff", "fff0fff0fff0fff0",
        "f08cf08cf08cf08c", "0f800f800f800f80", "f9e2f9e2f9e2f9e2", "f0f00ff0f0f00f0f",
        "f00000f000f00000", "f000f0f0f000f00f", "c0f0c0f0c0f0c0ff", "f0f0f0f0ffffffff",
    };

    const char* p = table[std::clamp(pattern, 0, patterns - 1)];
    const char c = p[((step % steps) + steps) % steps];
    const int v = c >= 'a' ? c - 'a' + 10 : c - '0';
    return (float) v / 15.0f;
}

//============================================================================== Pcf

void Pcf::prepare(double sampleRate)
{
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    attackCoef = follow(0.0003f, fs);
    smoothCoef = follow(0.010f, fs);
    reset();
}

void Pcf::reset()
{
    for (auto& s : state) s = {};
    env = shaped = 0.0f;
    haveCurrent = false;
    lastStruck = -1;
    settled = false;
}

void Pcf::setHostClock(bool isPlaying, double ppq, double bpm)
{
    playing = isPlaying && bpm > 0.0 && std::isfinite(ppq);

    if (playing)
    {
        position = ppq * 4.0;
        perSample = bpm / 60.0 * 4.0 / fs;
    }
    else
    {
        haveCurrent = false;
        lastStruck = -1;
    }
}

void Pcf::strike(long long sixteenth)
{
    const int s = (int) (((sixteenth % steps) + steps) % steps);
    env = level(target.pattern, s);
    lastStruck.store(s, std::memory_order_relaxed);
}

void Pcf::process(float* left, float* right, int n)
{
    const Settings& s = target;

    // where the knobs are heading
    const float octT   = std::clamp(s.freq, 0.0f, 1.0f) * 8.0f;     // 40 Hz to 10 kHz
    const float reachT = std::clamp(s.amount, 0.0f, 1.0f) * 7.0f;   // octaves
    const float qv     = 0.5f + s.q * s.q * 19.5f;
    const float dampT  = 1.0f / qv;
    const float wetT   = std::clamp(s.mix, 0.0f, 1.0f);
    const float gainT  = std::pow(10.0f, std::clamp(s.level, -24.0f, 12.0f) / 20.0f);
    const bool bandPass = s.mode == Mode::BP;

    if (! settled)
    {
        octaves = octT; reach = reachT; damping = dampT; wet = wetT; gain = gainT;
        settled = true;
    }

    const float seconds = 0.015f * std::pow(100.0f, std::clamp(s.decay, 0.0f, 1.0f));   // 15 ms to 1.5 s
    const float decayCoef = (float) std::exp(-1.0 / (seconds * fs));
    const float ceiling = (float) fs * 0.45f;
    const float piOverFs = pi / (float) fs;

    float* sides[2] = { left, right };

    for (int i = 0; i < n; ++i)
    {
        // A strike lands on the first sample of each sixteenth. Arriving in the middle of
        // one - starting playback there, or a jump - waits for the next, as a step sequencer
        // would; arriving on its first sample, as a loop back to the bar does, strikes it.
        if (playing)
        {
            const double at = position + (double) i * perSample;
            const long long sixteenth = (long long) std::floor(at + 1.0e-9);   // due on this sample, give or take rounding

            if (! haveCurrent || sixteenth != current)
            {
                const bool next = haveCurrent && sixteenth == current + 1;
                const bool onTheStep = at - (double) sixteenth < perSample;
                current = sixteenth;
                haveCurrent = true;
                if (next || onTheStep) strike(sixteenth);
            }
        }

        shaped += (env - shaped) * attackCoef;
        env *= decayCoef;

        octaves += (octT - octaves) * smoothCoef;
        reach   += (reachT - reach) * smoothCoef;
        damping += (dampT - damping) * smoothCoef;
        wet     += (wetT - wet) * smoothCoef;
        gain    += (gainT - gain) * smoothCoef;

        const float fc = std::min(ceiling, 40.0f * std::exp2(octaves + reach * shaped));
        const float g = std::tan(piOverFs * fc);
        const float k = damping;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        // Zavalishin's trapezoidal SVF, which keeps its tuning and stays stable however fast
        // the cutoff moves
        for (int c = 0; c < 2; ++c)
        {
            if (sides[c] == nullptr) continue;
            Svf& st = state[c];
            const float x = sides[c][i];
            const float v3 = x - st.ic2;
            const float v1 = a1 * st.ic1 + a2 * v3;
            const float v2 = st.ic2 + a2 * st.ic1 + a3 * v3;
            st.ic1 = 2.0f * v1 - st.ic1;
            st.ic2 = 2.0f * v2 - st.ic2;

            // the band output scaled to unity at its peak, whatever the Q, then lifted to sit
            // level with the low-pass
            const float y = bandPass ? v1 * k * 1.5f : v2;
            sides[c][i] = (x + (y - x) * wet) * gain;
        }
    }
}

} // namespace pcf
