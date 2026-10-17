// MzPCF - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

//
// The engine, checked on its own: the patterns, strikes landing on the host's sixteenths
// whatever the block size, starting and looping and stopping, the filter's response at rest,
// MIX and LEVEL, and that no setting of any control at any rate makes it blow up. Links no
// JUCE.
//
//     PcfCheck                 run the checks
//     PcfCheck --wav <dir>     and also write a few demo phrases there to listen to
//

#define _CRT_SECURE_NO_WARNINGS   // fopen, for the demo renders

#include "Pcf.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace pcf;

namespace
{

int failures = 0;

void check(bool ok, const char* what, const std::string& detail)
{
    std::printf("  %s  %-48s %s\n", ok ? "ok  " : "FAIL", what, detail.c_str());
    if (! ok) ++failures;
}

std::string fmt(const char* f, double a, double b = 0)
{
    char buf[128];
    std::snprintf(buf, sizeof buf, f, a, b);
    return buf;
}

constexpr double pi = 3.14159265358979323846;

//
// A host: plays from `ppq` at `bpm`, handing the engine its position at the top of each
// block, as the plugin does.
//
struct Host
{
    double rate = 48000.0, bpm = 120.0, ppq = 0.0;
    bool playing = true;
    int block = 512;
    long long played = 0;   // samples since ppq: a host counts them rather than adding up beats

    // calls perSample(i) after each sample, with the engine as it stands
    template <class F>
    void run(Pcf& p, float* l, float* r, int n, F&& perSample)
    {
        for (int at = 0; at < n; )
        {
            const int len = std::min(block, n - at);
            p.setHostClock(playing, ppq + (double) played * bpm / 60.0 / rate, bpm);
            if (block == 1)
            {
                p.process(l + at, r ? r + at : nullptr, 1);
                perSample(at);
            }
            else
            {
                p.process(l + at, r ? r + at : nullptr, len);
            }
            if (playing) played += len;
            at += len;
        }
    }

    void run(Pcf& p, float* l, float* r, int n) { run(p, l, r, n, [] (int) {}); }
};

std::vector<float> noise(size_t n, unsigned seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(-0.5f, 0.5f);
    std::vector<float> v(n);
    for (auto& x : v) x = u(rng);
    return v;
}

// Gain in dB of a sine at f through the filter, at rest, measured over the second half.
double gainAt(const Settings& s, double f, double rate)
{
    Pcf p;
    p.prepare(rate);
    p.setSettings(s);
    p.setHostClock(false, 0, 120);

    const int n = (int) (rate * 0.5);
    std::vector<float> x(n);
    for (int i = 0; i < n; ++i) x[i] = (float) (0.25 * std::sin(2.0 * pi * f * i / rate));
    std::vector<float> y = x;
    p.process(y.data(), nullptr, n);

    double ex = 0, ey = 0;
    for (int i = n / 2; i < n; ++i) { ex += (double) x[i] * x[i]; ey += (double) y[i] * y[i]; }
    return 10.0 * std::log10(ey / ex);
}

// The samples at which a new step was struck, and which step.
struct Strike { int at, step; };

std::vector<Strike> strikes(Host h, const Settings& s, int n)
{
    Pcf p;
    p.prepare(h.rate);
    p.setSettings(s);
    h.block = 1;

    std::vector<float> x(n, 0.0f);
    std::vector<Strike> out;
    int last = -1;
    h.run(p, x.data(), nullptr, n, [&] (int i)
    {
        const int st = p.step();
        if (st != last && st >= 0) out.push_back({ i, st });
        last = st;
    });
    return out;
}

//============================================================================== the checks

void patternsCheck()
{
    std::printf("patterns\n");

    bool inRange = true, allReachFull = true, allDistinct = true;
    for (int p = 0; p < patterns; ++p)
    {
        float most = 0;
        for (int s = 0; s < steps; ++s)
        {
            const float v = level(p, s);
            inRange = inRange && v >= 0.0f && v <= 1.0f;
            most = std::max(most, v);
        }
        allReachFull = allReachFull && most == 1.0f;

        for (int q = 0; q < p; ++q)
        {
            bool same = true;
            for (int s = 0; s < steps; ++s) same = same && level(p, s) == level(q, s);
            allDistinct = allDistinct && ! same;
        }
    }

    check(inRange, "every level is 0 to 1", "");
    check(allReachFull, "every pattern has a full-level step", "");
    check(allDistinct, "no two patterns are the same", "");
    check(level(0, 0) == 1.0f && level(0, 1) == 0.0f && level(0, 4) == 1.0f, "pattern 1 is every beat", "");
    check(level(0, -1) == level(0, 15) && level(0, 16) == level(0, 0), "steps wrap", "");
}

void clockCheck()
{
    std::printf("the clock\n");

    Settings s;
    s.pattern = 2;   // every step
    s.decay = 0.0f;

    // 120 bpm at 48 kHz: a sixteenth is exactly 6000 samples
    {
        Host h;
        const auto got = strikes(h, s, 6000 * 16 + 10);
        bool onGrid = got.size() == 17;
        for (size_t k = 0; k < got.size() && onGrid; ++k)
            onGrid = got[k].at == (int) k * 6000 && got[k].step == (int) (k % 16);
        check(onGrid, "120 bpm, 48 kHz: a strike every 6000 samples", fmt("%.0f strikes", (double) got.size()));
    }

    // 133 bpm at 44.1 kHz: a sixteenth is 4973.68 samples, so each lands on the sample after
    {
        Host h;
        h.rate = 44100.0;
        h.bpm = 133.0;
        const double spb = 44100.0 * 60.0 / 133.0 / 4.0;
        const auto got = strikes(h, s, (int) (spb * 32));
        int worst = 0;
        for (size_t k = 0; k < got.size(); ++k)
            worst = std::max(worst, std::abs(got[k].at - (int) std::ceil((double) k * spb)));
        check(got.size() >= 32 && worst <= 1, "133 bpm, 44.1 kHz: on the grid to a sample",
              fmt("%.0f strikes, worst %.0f samples off", (double) got.size(), worst));
    }

    // started in the middle of a step: waits for the next
    {
        Host h;
        h.ppq = 0.1;
        const auto got = strikes(h, s, 12000);
        check(! got.empty() && got[0].at == 3600 && got[0].step == 1, "started mid-step, waits for the next",
              got.empty() ? "none" : fmt("first at %.0f, step %.0f", got[0].at, got[0].step + 1));
    }

    // a pre-roll, before the bar: steps count back from 16
    {
        Host h;
        h.ppq = -0.5;   // two sixteenths before the bar
        const auto got = strikes(h, s, 12001);
        check(got.size() == 3 && got[0].step == 14 && got[2].step == 0 && got[2].at == 12000,
              "a pre-roll counts in to step 1 on the bar", fmt("%.0f strikes", (double) got.size()));
    }

    // a loop: the host jumps back to the bar, and step 1 is struck where it lands
    {
        Pcf p;
        p.prepare(48000.0);
        p.setSettings(s);
        std::vector<float> x(512, 0.0f);

        p.setHostClock(true, 3.5, 120.0);
        p.process(x.data(), nullptr, 512);
        const int before = p.step();
        p.setHostClock(true, 0.0, 120.0);
        p.process(x.data(), nullptr, 1);
        check(before == 14 && p.step() == 0, "a loop back to the bar strikes step 1", fmt("step %.0f then %.0f", before + 1, p.step() + 1));
    }

    // stopped: nothing is struck, and the filter settles back to rest
    {
        Pcf p;
        p.prepare(48000.0);
        Settings t = s;
        t.decay = 1.0f;   // the longest
        p.setSettings(t);
        std::vector<float> x(48000 * 12, 0.0f);

        p.setHostClock(true, 0.0, 120.0);
        p.process(x.data(), nullptr, 4800);
        const float struck = p.envelope();
        p.setHostClock(false, 0.0, 120.0);
        p.process(x.data(), nullptr, (int) x.size());
        check(struck > 0.9f && p.step() == -1 && p.envelope() < 1.0e-3f, "stopped, the filter comes back to rest",
              fmt("%.4f after 12 s", p.envelope()));
    }

    // a step's level is what it is struck to: on pattern 1 only the beats open the filter
    {
        Host h;
        Settings t;
        t.pattern = 0;
        t.decay = 0.0f;
        Pcf p;
        p.prepare(h.rate);
        p.setSettings(t);
        h.block = 1;
        std::vector<float> x(6000 * 16, 0.0f), peaks(16, 0.0f);
        // the first few milliseconds of each step, where its strike would peak
        h.run(p, x.data(), nullptr, (int) x.size(), [&] (int i)
        {
            if (i % 6000 >= 50 && i % 6000 < 600) peaks[i / 6000] = std::max(peaks[i / 6000], p.envelope());
        });
        bool ok = true;
        for (int st = 0; st < 16; ++st)
            ok = ok && (st % 4 == 0 ? peaks[st] > 0.9f : peaks[st] < 0.01f);
        check(ok, "pattern 1 opens on the beats only", fmt("beat %.2f, off-beat %.3f", peaks[0], peaks[1]));
    }

    // the block size makes no difference
    {
        Settings t;
        t.pattern = 8;
        t.q = 0.8f;
        t.amount = 0.9f;
        const auto in = noise(44100 * 3, 7);
        std::vector<std::vector<float>> outs;
        for (int block : { 1, 37, 512, 4096 })
        {
            Host h;
            h.rate = 44100.0;
            h.bpm = 133.0;
            h.block = block;
            Pcf p;
            p.prepare(h.rate);
            p.setSettings(t);
            auto l = in, r = in;
            h.run(p, l.data(), r.data(), (int) l.size());
            outs.push_back(l);
        }
        double worst = 0;
        for (size_t o = 1; o < outs.size(); ++o)
            for (size_t i = 0; i < in.size(); ++i)
                worst = std::max(worst, (double) std::fabs(outs[o][i] - outs[0][i]));
        check(worst < 1.0e-4, "the same at block sizes 1, 37, 512 and 4096", fmt("worst difference %.2g", worst));
    }
}

void filterCheck()
{
    std::printf("the filter, at rest\n");

    Settings s;
    s.freq = 0.5f;   // 40 Hz * 2^4 = 640 Hz
    s.q = 0.0f;

    const double pass = gainAt(s, 80.0, 48000.0);
    const double stop = gainAt(s, 6400.0, 48000.0);
    check(std::fabs(pass) < 1.0, "LP passes an octave below", fmt("%+.1f dB at 80 Hz", pass));
    check(stop < -35.0, "LP: 12 dB an octave above", fmt("%+.1f dB at 6.4 kHz", stop));

    s.q = 1.0f;
    const double peak = gainAt(s, 640.0, 48000.0);
    check(peak > 24.0 && peak < 28.0, "LP at full Q peaks at Q 20", fmt("%+.1f dB at 640 Hz", peak));

    s.mode = Mode::BP;
    for (float q : { 0.2f, 0.6f, 1.0f })
    {
        s.q = q;
        const double centre = gainAt(s, 640.0, 48000.0);
        check(std::fabs(centre - 3.52) < 0.5, "BP sits level at its centre, whatever Q",
              fmt("Q knob %.1f: %+.1f dB", q, centre));
    }
    s.q = 0.6f;
    const double below = gainAt(s, 64.0, 48000.0);
    check(below < -15.0, "BP cuts a few octaves below", fmt("%+.1f dB at 64 Hz", below));

    s.mode = Mode::LP;
    s.freq = 1.0f;
    s.q = 0.0f;
    check(std::fabs(gainAt(s, 1000.0, 96000.0)) < 1.0, "fully open passes the middle", "");
}

void mixLevelCheck()
{
    std::printf("MIX and LEVEL\n");

    const auto in = noise(48000, 3);
    Settings s;
    s.mix = 0.0f;
    s.freq = 0.0f;

    Pcf p;
    p.prepare(48000.0);
    p.setSettings(s);
    auto y = in;
    p.process(y.data(), nullptr, (int) y.size());
    double worst = 0;
    for (size_t i = 0; i < y.size(); ++i) worst = std::max(worst, (double) std::fabs(y[i] - in[i]));
    check(worst < 1.0e-6, "MIX at dry passes the input untouched", fmt("worst %.2g", worst));

    s.level = 6.0f;
    p.prepare(48000.0);
    p.setSettings(s);
    y = in;
    p.process(y.data(), nullptr, (int) y.size());
    const double ratio = y[1000] / in[1000];
    check(std::fabs(ratio - 1.9953) < 1.0e-3, "LEVEL +6 dB", fmt("x%.4f", ratio));
}

void stabilityCheck()
{
    std::printf("stability\n");

    std::mt19937 rng(101);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);

    bool finite = true;
    double loudest = 0;

    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        Pcf p;
        p.prepare(rate);
        Host h;
        h.rate = rate;
        h.bpm = 40.0 + 260.0 * u(rng);
        h.block = 256;

        const int chunk = (int) (rate * 0.25);
        for (int round = 0; round < 40; ++round)
        {
            Settings s;
            s.pattern = (int) (u(rng) * patterns) % patterns;
            s.freq = u(rng); s.q = round % 4 == 0 ? 1.0f : u(rng);
            s.amount = round % 3 == 0 ? 1.0f : u(rng);
            s.decay = u(rng); s.mode = u(rng) < 0.5f ? Mode::LP : Mode::BP;
            s.mix = u(rng); s.level = -24.0f + 36.0f * u(rng);
            p.setSettings(s);
            h.playing = round % 7 != 3;

            auto l = noise((size_t) chunk, (unsigned) round), r = noise((size_t) chunk, (unsigned) round + 99);
            h.run(p, l.data(), r.data(), chunk);
            for (int i = 0; i < chunk; ++i)
            {
                finite = finite && std::isfinite(l[i]) && std::isfinite(r[i]);
                loudest = std::max(loudest, (double) std::max(std::fabs(l[i]), std::fabs(r[i])));
            }
        }
    }

    check(finite, "no NaN or infinity, any setting, any rate", "");
    check(loudest < 200.0, "bounded, even at full Q and LEVEL", fmt("peak %.1f from noise at 0.5", loudest));
}

void speedCheck()
{
    std::printf("speed\n");

    Pcf p;
    p.prepare(48000.0);
    Settings s;
    s.pattern = 2;
    p.setSettings(s);
    Host h;
    auto l = noise(48000 * 10, 1), r = noise(48000 * 10, 2);

    const auto t0 = std::chrono::steady_clock::now();
    h.run(p, l.data(), r.data(), (int) l.size());
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    check(10.0 / secs > 50.0, "stereo at 48 kHz, faster than 50x real time", fmt("%.0fx", 10.0 / secs));
}

//============================================================================== the demo renders

void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int rate)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) { std::printf("  could not write %s\n", path.c_str()); return; }

    const uint32_t frames = (uint32_t) l.size(), bytes = frames * 4;
    auto u32 = [f] (uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [f] (uint16_t v) { std::fwrite(&v, 2, 1, f); };

    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(2); u32((uint32_t) rate); u32((uint32_t) rate * 4); u16(4); u16(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    for (uint32_t i = 0; i < frames; ++i)
        for (float v : { l[i], r[i] })
        {
            const int16_t s = (int16_t) std::lrint(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
            std::fwrite(&s, 2, 1, f);
        }
    std::fclose(f);
    std::printf("  wrote %s\n", path.c_str());
}

// A held minor chord of detuned saws, the plainest thing to hear a filter play.
std::vector<float> chord(int n, int rate, double detune)
{
    const double notes[] = { 65.41, 130.81, 155.56, 196.00 };   // C2, C3, Eb3, G3
    std::vector<float> v((size_t) n, 0.0f);
    for (double f : notes)
    {
        const double inc = f * detune / rate;
        double ph = f / 1000.0;
        for (int i = 0; i < n; ++i)
        {
            v[(size_t) i] += (float) (0.12 * (2.0 * ph - 1.0));
            ph += inc;
            if (ph >= 1.0) ph -= 1.0;
        }
    }
    return v;
}

void demos(const std::string& dir)
{
    std::printf("demo renders\n");

    const int rate = 48000;
    const double bpm = 125.0;
    const int twoBars = (int) (rate * 60.0 / bpm * 8.0);

    struct Demo { const char* name; std::vector<int> patternsPlayed; int mode; float q; };
    const Demo list[] = {
        { "pcf-lp-tour", { 0, 2, 8, 10, 17, 26 }, Mode::LP, 0.55f },
        { "pcf-bp-tour", { 1, 5, 9, 13, 20, 29 }, Mode::BP, 0.45f },
    };

    for (const auto& d : list)
    {
        const int n = twoBars * (int) d.patternsPlayed.size();
        auto l = chord(n, rate, 1.0), r = chord(n, rate, 1.004);

        Pcf p;
        p.prepare(rate);
        Host h;
        h.rate = rate;
        h.bpm = bpm;

        for (size_t k = 0; k < d.patternsPlayed.size(); ++k)
        {
            Settings s;
            s.pattern = d.patternsPlayed[k];
            s.mode = d.mode;
            s.q = d.q;
            s.freq = 0.25f;
            s.amount = 0.7f;
            s.decay = 0.4f;
            p.setSettings(s);
            h.run(p, l.data() + k * twoBars, r.data() + k * twoBars, twoBars);
        }

        writeWav(dir + "/" + d.name + ".wav", l, r, rate);
    }
}

} // namespace

int main(int argc, char** argv)
{
    patternsCheck();
    clockCheck();
    filterCheck();
    mixLevelCheck();
    stabilityCheck();
    speedCheck();

    if (argc >= 3 && std::strcmp(argv[1], "--wav") == 0)
        demos(argv[2]);

    std::printf("\n%s\n", failures == 0 ? "all checks passed" : (std::to_string(failures) + " FAILED").c_str());
    return failures == 0 ? 0 : 1;
}
