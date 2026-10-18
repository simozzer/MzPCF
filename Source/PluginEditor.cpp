// MzPCF - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "PluginEditor.h"

#include "PCF/Pcf.h"

namespace
{
    juce::RangedAudioParameter& parameter(MzPcfProcessor& p, const char* id)
    {
        auto* param = p.state.getParameter(id);
        jassert(param != nullptr);
        return *param;
    }

    void drawLegend(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r)
    {
        g.setColour(colours::light);
        g.setFont(fonts::bold(9.5f));
        g.drawText(text, r, juce::Justification::centred);
    }
}

//============================================================================== Knob

Knob::Knob(juce::AudioProcessorValueTreeState& state, const params::Spec& spec)
    : legend(spec.legend)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity(180);
    slider.setPopupDisplayEnabled(true, true, nullptr);
    slider.setTooltip(juce::String(spec.name).toUpperCase() + "\n" + spec.hint);
    slider.setColour(juce::Slider::rotarySliderFillColourId, colours::knobCap);
    slider.setColour(juce::Slider::thumbColourId, colours::pointer);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, colours::light);
    addAndMakeVisible(slider);

    // the attachment sets the range, and reads the value out as the host would
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, spec.id, slider);

    if (auto* p = state.getParameter(spec.id))
        slider.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop(12);
    const int side = juce::jmin(r.getWidth(), r.getHeight());
    slider.setBounds(r.withSizeKeepingCentre(side, side).withY(r.getY()));
}

void Knob::paint(juce::Graphics& g)
{
    drawLegend(g, legend, getLocalBounds().removeFromTop(12));
}

//============================================================================== LedNumber

LedNumber::LedNumber(juce::RangedAudioParameter& p, int d) : param(p), digits(d)
{
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
}

int LedNumber::value() const
{
    return juce::roundToInt(param.convertFrom0to1(param.getValue()));
}

void LedNumber::put(int v)
{
    const auto range = param.getNormalisableRange();
    v = juce::jlimit((int) range.start, (int) range.end, v);
    if (v != value()) param.setValueNotifyingHost(param.convertTo0to1((float) v));
    repaint();
}

void LedNumber::mouseDown(const juce::MouseEvent&)
{
    dragFrom = value();
    param.beginChangeGesture();
}

void LedNumber::mouseDrag(const juce::MouseEvent& e) { put(dragFrom - e.getDistanceFromDragStartY() / 4); }
void LedNumber::mouseUp(const juce::MouseEvent&)     { param.endChangeGesture(); }

void LedNumber::mouseDoubleClick(const juce::MouseEvent&)
{
    param.beginChangeGesture();
    put(juce::roundToInt(param.convertFrom0to1(param.getDefaultValue())));
    param.endChangeGesture();
}

void LedNumber::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    wheel += w.deltaY * 8.0f;
    const int whole = (int) wheel;
    if (whole == 0) return;
    wheel -= (float) whole;
    param.beginChangeGesture();
    put(value() + whole);
    param.endChangeGesture();
}

void LedNumber::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour(colours::display);
    g.fillRoundedRectangle(r, 2.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(r.reduced(0.5f), 2.0f, 1.0f);

    g.setFont(fonts::digits(r.getHeight() * 0.78f));
    g.setColour(colours::ledOff.withAlpha(0.4f));   // the unlit segments, there but not read
    g.drawText(juce::String::repeatedString("8", digits), r.reduced(4.0f, 0.0f), juce::Justification::centredRight);
    g.setColour(colours::ledRed);
    g.drawText(juce::String(value()), r.reduced(4.0f, 0.0f), juce::Justification::centredRight);
}

//============================================================================== Switch

Switch::Switch(juce::RangedAudioParameter& p, const juce::String& l) : param(p), legend(l) {}

void Switch::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    pressed = true;

    const int positions = param.getNumSteps();
    const int now = juce::roundToInt(param.getValue() * (float) (positions - 1));
    param.beginChangeGesture();
    param.setValueNotifyingHost((float) ((now + 1) % positions) / (float) (positions - 1));
    param.endChangeGesture();
    repaint();
}

void Switch::mouseUp(const juce::MouseEvent&)
{
    pressed = false;
    repaint();
}

void Switch::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    drawLegend(g, legend, area.removeFromTop(12));
    area.removeFromTop(2);

    auto r = area.withSizeKeepingCentre(juce::jmin(area.getWidth(), 46), 26).toFloat().reduced(1.0f);
    if (pressed) r = r.translated(0.0f, 1.0f);

    g.setGradientFill(juce::ColourGradient(colours::pad.brighter(pressed ? 0.0f : 0.15f), r.getX(), r.getY(),
                                           colours::pad.darker(0.3f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawRoundedRectangle(r, 3.0f, 1.0f);

    g.setColour(colours::light);
    g.setFont(fonts::bold(11.0f));
    g.drawText(param.getCurrentValueAsText(), r.toNearestInt(), juce::Justification::centred);
}

//============================================================================== PatternDisplay

PatternDisplay::PatternDisplay()
{
    setTooltip("The pattern: on each sixteenth the filter is opened by that much, and closes again by DEC. "
               "The step playing is lit while your DAW plays.");
}

void PatternDisplay::show(int p, int s)
{
    if (p == pattern && s == step) return;
    pattern = p;
    step = s;
    repaint();
}

void PatternDisplay::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour(colours::display);
    g.fillRoundedRectangle(r, 2.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(r.reduced(0.5f), 2.0f, 1.0f);

    const auto inner = r.reduced(5.0f);
    const float w = inner.getWidth() / (float) pcf::steps;
    for (int s = 0; s < pcf::steps; ++s)
    {
        const float level = pcf::level(pattern, s);
        const float h = juce::jmax(1.5f, inner.getHeight() * level);
        const juce::Rectangle<float> bar(inner.getX() + w * (float) s + 1.0f, inner.getBottom() - h, w - 2.0f, h);
        g.setColour(s == step ? colours::ledRed.brighter(0.3f) : colours::ledRed.withAlpha(s % 4 == 0 ? 0.75f : 0.5f));
        g.fillRect(bar);
    }
}

//============================================================================== the editor

MzPcfEditor::MzPcfEditor(MzPcfProcessor& p)
    : AudioProcessorEditor(p),
      processor(p),
      pattern(parameter(p, "pattern"), 2),
      mode(parameter(p, "mode"), "MODE")
{
    setLookAndFeel(&look);

    pattern.setTooltip(juce::String("PATTERN\n") + params::spec("pattern").hint);
    mode.setTooltip(juce::String("MODE\n") + params::spec("mode").hint);
    addAndMakeVisible(pattern);
    addAndMakeVisible(display);

    //
    // Where to come back to: the version this is, and the page the next one will be on.
    // Nothing here checks for updates by itself - the plugin never goes online unasked.
    // It takes no keyboard focus, or a host's space bar could open it rather than play.
    //
    projectLink.setButtonText("MzPCF " JucePlugin_VersionString "  -  github.com/simozzer/MzPCF");
    projectLink.setURL(juce::URL("https://github.com/simozzer/MzPCF"));
    projectLink.setFont(fonts::bold(10.5f), false, juce::Justification::centredRight);
    projectLink.setColour(juce::HyperlinkButton::textColourId, colours::light.withAlpha(0.6f));
    projectLink.setTooltip("The project's page: the latest version and what's new in it, the source, "
                           "and where to report a problem. Opens in your browser.");
    projectLink.setWantsKeyboardFocus(false);
    addAndMakeVisible(projectLink);
    addAndMakeVisible(mode);

    for (const char* id : { "freq", "q", "amount", "decay" })
        filterKnobs.push_back(std::make_unique<Knob>(p.state, params::spec(id)));
    for (const char* id : { "mix", "level" })
        outputKnobs.push_back(std::make_unique<Knob>(p.state, params::spec(id)));
    for (auto& k : filterKnobs) addAndMakeVisible(*k);
    for (auto& k : outputKnobs) addAndMakeVisible(*k);

    setSize(690, 170);
    timerCallback();   // opened showing the pattern, not after the first tick
    startTimerHz(30);
}

MzPcfEditor::~MzPcfEditor()
{
    setLookAndFeel(nullptr);
}

void MzPcfEditor::timerCallback()
{
    display.show(pattern.value() - 1, processor.step());
    pattern.repaint();
    mode.repaint();
}

void MzPcfEditor::resized()
{
    face = getLocalBounds().reduced(10);
    projectLink.setBounds(face.reduced(14, 8).removeFromTop(28).removeFromRight(300));   // the title's row

    auto r = face.reduced(10, 8);
    r.removeFromTop(30);   // the title

    outArea = r.removeFromRight(130);
    r.removeFromRight(8);
    pcfArea = r;

    // the PCF: the pattern number and its picture, the mode, then the four knobs
    auto row = pcfArea.reduced(8, 6);
    row.removeFromTop(18);   // the unit's name
    const int y = row.getY(), h = row.getHeight();

    pattern.setBounds(row.getX(), y + 16, 44, 32);
    display.setBounds(row.getX() + 52, y + 4, 150, h - 8);
    mode.setBounds(row.getX() + 210, y + 4, 50, 46);

    int x = row.getX() + 268;
    for (auto& k : filterKnobs)
    {
        k->setBounds(x, y, 54, h);
        x += 56;
    }

    auto out = outArea.reduced(8, 6);
    out.removeFromTop(18);
    for (auto& k : outputKnobs)
        k->setBounds(out.removeFromLeft(56).withHeight(out.getHeight()));
}

void MzPcfEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::rail);
    drawFace(g, face.toFloat(), colours::rail.brighter(0.15f));

    // the title
    auto title = face.reduced(14, 8).removeFromTop(28);
    g.setColour(colours::light);
    g.setFont(fonts::bold(22.0f));
    g.drawText("MzPCF", title, juce::Justification::centredLeft);
    g.setColour(colours::light.withAlpha(0.6f));
    g.setFont(fonts::bold(10.5f));
    g.drawText("PATTERN CONTROLLED FILTER", title.withTrimmedLeft(80), juce::Justification::centredLeft);

    // the two units, as the rack draws its effects
    for (auto [area, name] : { std::pair { pcfArea, "PCF" }, std::pair { outArea, "OUTPUT" } })
    {
        drawFace(g, area.toFloat(), colours::mixer);

        auto tag = area.reduced(8, 6).removeFromTop(15).withWidth(60);
        g.setColour(colours::mixerEdge);
        g.fillRoundedRectangle(tag.toFloat(), 2.0f);
        g.setColour(colours::light);
        g.setFont(fonts::bold(10.5f));
        g.drawText(name, tag, juce::Justification::centred);
    }

    g.setColour(colours::light.withAlpha(0.75f));
    g.setFont(fonts::bold(9.0f));
    g.drawText("PATTERN", pattern.getBounds().withY(pattern.getY() - 13).withHeight(12).withWidth(60),
               juce::Justification::centredLeft);
}
