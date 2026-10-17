// MzPCF - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

namespace pcf
{

namespace Mode { enum { LP, BP }; }

//
// The front panel, in panel units. The PCF's own five controls run 0 to 1 as on the rack
// (MzDebirth); MIX and LEVEL are not on ReBirth's panel - a DAW insert wants them.
//
struct Settings
{
    int   pattern = 0;       // 0 to 31
    float freq    = 0.35f;   // where the filter rests, 40 Hz to 10 kHz
    float q       = 0.4f;    // resonance, Q 0.5 to 20
    float amount  = 0.6f;    // how far a full-level step opens it, up to seven octaves
    float decay   = 0.35f;   // how fast it closes again, 15 ms to 1.5 s
    int   mode    = Mode::LP;

    float mix     = 1.0f;    // 0 dry to 1 all through the filter
    float level   = 0.0f;    // output, dB, -24 to +12
};

} // namespace pcf
