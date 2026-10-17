// MzPCF - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Params.h"

namespace params
{

using S = pcf::Settings;

const std::vector<Spec>& specs()
{
    static const std::vector<Spec> table = {
        { "pattern", "Pattern",   "PATTERN", "Which of the 32 patterns plays the filter: on each sixteenth it is opened by that step's level. Drag up or down, or use the wheel.",
          nullptr, &S::pattern, {}, 1.0f, 32.0f },
        { "freq",    "Frequency", "FREQ",    "Where the filter rests between strikes, 40 Hz to 10 kHz.",
          &S::freq },
        { "q",       "Resonance", "Q",       "Resonance: a peak at the cutoff, up to a whistle as the pattern sweeps it.",
          &S::q },
        { "amount",  "Amount",    "AMT",     "How far a full-level step opens the filter above FREQ, up to seven octaves.",
          &S::amount },
        { "decay",   "Decay",     "DEC",     "How fast the filter closes again after each strike, 15 ms to 1.5 s.",
          &S::decay },
        { "mode",    "Mode",      "MODE",    "Low-pass (LP), or band-pass (BP) for a thinner, wah-like sweep.",
          nullptr, &S::mode, { "LP", "BP" } },
        { "mix",     "Mix",       "MIX",     "How much of the sound goes through the filter: 0% is dry, 100% all filtered. Not on the original.",
          &S::mix, nullptr, {}, 0.0f, 1.0f, "%" },
        { "level",   "Level",     "LEVEL",   "Output level, -24 to +12 dB, to make up what the filter takes away. Not on the original.",
          &S::level, nullptr, {}, -24.0f, 12.0f, "dB" },
    };

    return table;
}

const Spec& spec(const juce::String& id)
{
    for (const auto& p : specs())
        if (id == p.id) return p;
    jassertfalse;
    return specs().front();
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const S defaults;

    for (const auto& p : specs())
    {
        const juce::ParameterID id { p.id, 1 };

        if (p.f != nullptr)
        {
            const juce::String unit(p.unit);
            auto text = [unit] (float v, int)
            {
                if (unit == "dB") return (v > 0.0f ? "+" : "") + juce::String(v, 1) + " dB";
                if (unit == "%")  return juce::String(juce::roundToInt(v * 100.0f)) + "%";
                return juce::String(juce::roundToInt(v * 100.0f));
            };

            layout.add(std::make_unique<juce::AudioParameterFloat>(
                id, p.name, juce::NormalisableRange<float>(p.min, p.max, 0.0f), defaults.*(p.f),
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(text)));
        }
        else if (! p.choices.isEmpty())
        {
            layout.add(std::make_unique<juce::AudioParameterChoice>(id, p.name, p.choices, defaults.*(p.i)));
        }
        else
        {
            // a number, shown from min - the pattern, 1 to 32, held as 0 to 31
            layout.add(std::make_unique<juce::AudioParameterInt>(
                id, p.name, (int) p.min, (int) p.max, defaults.*(p.i) + (int) p.min));
        }
    }

    return layout;
}

Reader::Reader(juce::AudioProcessorValueTreeState& state)
{
    for (const auto& p : specs())
    {
        auto* v = state.getRawParameterValue(p.id);
        jassert(v != nullptr);
        values.push_back(v);
    }
}

pcf::Settings Reader::read() const
{
    S s;
    const auto& table = specs();

    for (size_t n = 0; n < table.size(); ++n)
    {
        const float v = values[n]->load(std::memory_order_relaxed);
        const auto& p = table[n];

        if (p.f != nullptr)                s.*(p.f) = v;
        else if (! p.choices.isEmpty())    s.*(p.i) = juce::roundToInt(v);
        else                               s.*(p.i) = juce::roundToInt(v) - (int) p.min;
    }

    return s;
}

} // namespace params
