# MzPCF

**ReBirth's pattern-controlled filter, for any track in your DAW.** MzPCF is a VST3 effect
that emulates the PCF from Propellerhead's ReBirth RB-338: a resonant filter played by one of
32 step patterns, struck on every sixteenth of your project and closing again in between, so
it plays a rhythm of its own over whatever goes through it. Pumping pads, gated chords,
wah-wah basslines and drum loops that breathe.

![The MzPCF panel](docs/screenshot.png)

It is the PCF in [MzDebirth](https://github.com/simozzer/MzDebirth)'s effects rack, too: the
rack compiles its PCF from this repository, so the two are one and the same, and a pattern
number means the same in both.

The controls, as on ReBirth's panel:
- **PATTERN** - which of the 32 patterns plays the filter. Drag the number up or down, or
  use the wheel; the display draws the pattern and lights the step playing.
- **FREQ** - where the filter rests between strikes, 40 Hz to 10 kHz.
- **Q** - resonance, from gentle to a whistle as the pattern sweeps it.
- **AMT** - how far a full-level step opens the filter above FREQ, up to seven octaves.
- **DEC** - how fast it closes again, 15 ms to 1.5 s.
- **MODE** - low-pass, or band-pass for a thinner, wah-like sweep.

And two that ReBirth didn't need, for use as an insert:
- **MIX** - dry to fully filtered, for blending the effect in.
- **LEVEL** - -24 to +12 dB, to make up what the filter takes away.

The pattern follows your DAW's transport, to the sample: step 1 is the first sixteenth of
every bar, and it follows loops and tempo changes. With the transport stopped nothing is
struck, and the filter settles back to FREQ - as ReBirth's PCF did when ReBirth stopped.

MzPCF is an independent project. ReBirth is a trademark of Reason Studios.

## Installing

Download `MzPCF-<version>-win64-Setup.exe` from the
[releases page](https://github.com/simozzer/MzPCF/releases) and run it. It asks whether to
install for everyone or just you:

- **Everyone** (needs administrator rights) puts the VST3 in
  `C:\Program Files\Common Files\VST3`, which every DAW scans.
- **Just me** puts it in `%LOCALAPPDATA%\Programs\Common\VST3`. Some DAWs need that folder
  added to their plugin paths once; in Ableton Live it goes under
  *Preferences → Plug-Ins → VST3 Plug-In Custom Folder*.

Then rescan plugins in your DAW. The installer isn't code-signed, so Windows may warn about an
unknown publisher: choose *More info → Run anyway*. Uninstall from *Settings → Apps*.

To build it yourself instead, see [Building](#building).

### macOS (beta)

Download `MzPCF-<version>-macOS-beta.zip` from the
[releases page](https://github.com/simozzer/MzPCF/releases) and unzip it. It holds the VST3 and an Audio Unit (for Logic and GarageBand),
for Apple Silicon and Intel Macs on macOS 11 or later.

They aren't signed by Apple, which takes a paid developer account, so macOS won't open them
with a double-click. Install them from Terminal instead. Type `bash ` (with the space), drag
`install.sh` from the unzipped folder into the Terminal window, and press Return:

    bash ~/Downloads/MzPCF-<version>-macOS-beta/install.sh

It copies everything into your own Library folders, with no password needed, and clears the
"downloaded from the internet" flag that would stop your DAW loading the plugin. Then rescan
plugins in your DAW. `READ ME FIRST.txt` in the zip explains how to install by hand instead.

It's a beta: built and checked on GitHub's Macs, including Apple's own Audio Unit validator,
but not yet tried on a real one. If something doesn't work, please
[open an issue](https://github.com/simozzer/MzPCF/issues).

### Linux (beta)

Download `MzPCF-<version>-linux-x86_64-beta.tar.gz` from the
[releases page](https://github.com/simozzer/MzPCF/releases) and unpack it. It holds the VST3 and an LV2, for 64-bit x86 PCs on Ubuntu 22.04 or later, or another distribution as new.
Install it with one command in the unpacked folder:

    bash install.sh

It copies the plugins into `~/.vst3` and `~/.lv2`, where Linux hosts (Ardour, Bitwig,
Reaper, Carla, Qtractor, Zrythm) look for them, with no password needed. Then rescan plugins
in your DAW. `READ ME FIRST.txt` in the archive explains how to install by hand, and the
few libraries a minimal system might need.

It's a beta: built and checked on GitHub's Ubuntu machines, but not yet tried much in real
Linux hosts. If something doesn't work, please
[open an issue](https://github.com/simozzer/MzPCF/issues).

## Building

Needs Visual Studio 2022+ with *Desktop development with C++*, and JUCE checked out beside
this repository (`..\JUCE`, or pass `-DJUCE_PATH=`).

    .\build.ps1                 engine check, then the VST3
    .\build.ps1 -NoPlugin       engine check only
    .\build.ps1 -Wav out        and render demo phrases into out\ to listen to
    .\build.ps1 -Release        and the release downloads into dist\: Setup.exe and a zip

`-Release` needs [Inno Setup 6](https://jrsoftware.org/isdl.php). The installer's script is
`installer\MzPCF.iss`, and it takes its version from `CMakeLists.txt`.

## Releasing

GitHub builds the downloads for every release and attaches them, so each one has its
installer:

1. Bump the version in `CMakeLists.txt`.
2. Write the notes in `docs/releases/v<version>.md`, ending with the credit line
   (copy it from the last release's notes), then commit both.
3. Tag and push: `git tag -a v0.2.0 -m "MzPCF 0.2.0"`, then `git push origin main v0.2.0`.
4. The *release* workflow builds and checks the plugin, then makes a **draft** release with
   the notes and the two Windows downloads. A few minutes later its Mac job adds
   `MzPCF-<version>-macOS-beta.zip` to the same draft. Read it over on GitHub and publish it.

The Mac build is `.github/workflows/mac.yml`. It also runs on every push to `main`, as a
check, and keeps the zip on the run for two weeks.

The workflow stops if the tag and the version in `CMakeLists.txt` disagree. To rebuild an
existing release's downloads, run the workflow from the *Actions* tab with that tag.

## Layout

    Source/PCF/          the effect - plain C++, no JUCE
      Pcf.h/.cpp         the patterns, the envelope and the filter - which MzDebirth's rack
                         plays on its own clock - and the host clock that plays them here
      StepClock.h        the host's timeline as steps, to the sample; MzPCG's clock too
      Settings.h         the front panel, in panel units
    Source/              the JUCE plugin round it: parameters, look, editor
    Tests/PcfCheck.cpp   the engine's checks; gates the plugin build

## How it works

ReBirth's manual describes the PCF as a two-pole filter, low-pass or band-pass, with cutoff
and resonance; an envelope whose decay is set on the panel, whose attack comes from the
pattern, and whose depth is set by AMT; and preset patterns for pumping, sweeps and phrasing.
Beyond that, the curves and ranges here are chosen by ear rather than measured from ReBirth,
and the 32 patterns are MzDebirth's, not a transcription of ReBirth's.

**The clock.** At the top of every block the plugin reads where the host's timeline is, and
runs it forward sample by sample from there. A strike lands on the first sample of each
sixteenth. Playback started in the middle of a step waits for the next one, as a step
sequencer would; a loop back to the bar strikes step 1 where it lands. In a pre-roll, the
steps count in to step 1 on the bar.

**The envelope.** Each strike sets the envelope to that step's level - a step at 0 closes the
filter - and it decays exponentially by DEC. A third of a millisecond's rounding on the strike
keeps it from clicking.

**The filter.** A trapezoidal (zero-delay feedback) state-variable filter, after Zavalishin,
which stays in tune and stable however fast the pattern moves its cutoff. The cutoff is
FREQ raised by AMT × the envelope, in octaves, up to 45% of the sample rate. Band-pass is
scaled to the same level at its centre whatever the Q. Every knob is smoothed over 10 ms, so
automating one doesn't zip.

## Licence

Created by Simon Moscrop. Free software under the GNU AGPL v3; see [LICENSE](LICENSE).
