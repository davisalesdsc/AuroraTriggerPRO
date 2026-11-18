#include "PluginEditor.h"

//==============================================================================
// Metallic Hardware LookAndFeel
//==============================================================================
AuroraMetallicLookAndFeel::AuroraMetallicLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff4a4d54));
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xffd5d8de));
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff38bdf8));
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xff1a1d24));
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void AuroraMetallicLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                  float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                                  juce::Slider& slider)
{
    auto radius = static_cast<float> (juce::jmin (width / 2, height / 2)) - 3.0f;
    auto centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
    auto centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Outer Shadow & Metallic Bezel
    g.setColour (juce::Colour (0xff181a1f));
    g.fillEllipse (rx - 1.5f, ry - 1.5f, rw + 3.0f, rw + 3.0f);

    juce::ColourGradient grad (juce::Colour (0xffdce0e6), centreX, ry,
                               juce::Colour (0xff7c828d), centreX, ry + rw, false);
    g.setGradientFill (grad);
    g.fillEllipse (rx, ry, rw, rw);

    // Inner Cap Accent (Purple for Pan, Brass/Copper for Tune/Detector, Steel for Gate)
    auto accentCol = slider.findColour (juce::Slider::rotarySliderFillColourId);
    g.setColour (accentCol);
    float innerR = radius * 0.72f;
    g.fillEllipse (centreX - innerR, centreY - innerR, innerR * 2.0f, innerR * 2.0f);

    // Indicator Line
    juce::Path p;
    p.addRectangle (-1.2f, -radius + 2.0f, 2.4f, radius * 0.65f);
    p.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));
    g.setColour (juce::Colour (0xff111317));
    g.fillPath (p);
}

void AuroraMetallicLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                                  float sliderPos, float, float,
                                                  const juce::Slider::SliderStyle style, juce::Slider&)
{
    if (style == juce::Slider::LinearVertical)
    {
        float trackX = static_cast<float> (x) + static_cast<float> (width) * 0.5f - 2.5f;
        g.setColour (juce::Colour (0xff181b20));
        g.fillRoundedRectangle (trackX, static_cast<float> (y), 5.0f, static_cast<float> (height), 2.0f);

        // Tick marks on left of fader
        g.setColour (juce::Colour (0xff2c3038));
        for (int t = 0; t <= 10; ++t)
        {
            float ty = static_cast<float> (y) + (static_cast<float> (height) * t / 10.0f);
            g.drawHorizontalLine (static_cast<int> (ty), trackX - 7.0f, trackX - 2.0f);
        }

        // Metallic Fader Cap
        float capW = 24.0f;
        float capH = 14.0f;
        float capX = static_cast<float> (x) + (static_cast<float> (width) - capW) * 0.5f;
        float capY = juce::jlimit (static_cast<float> (y), static_cast<float> (y + height) - capH, sliderPos - capH * 0.5f);

        g.setColour (juce::Colour (0xff14161a));
        g.fillRoundedRectangle (capX - 1.0f, capY - 1.0f, capW + 2.0f, capH + 2.0f, 2.5f);
        g.setColour (juce::Colour (0xffd8dce3));
        g.fillRoundedRectangle (capX, capY, capW, capH, 2.0f);
        g.setColour (juce::Colour (0xff38bdf8));
        g.drawHorizontalLine (static_cast<int> (capY + capH * 0.5f), capX + 2.0f, capX + capW - 2.0f);
    }
}

juce::Label* AuroraMetallicLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (juce::FontOptions (11.0f));
    return l;
}

//==============================================================================
// Segmented LED Meter Component (0 to -48 dB)
//==============================================================================
void LedMeterComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff14171c));
    g.fillRoundedRectangle (bounds, 3.0f);

    float db = levelDb.load();
    float norm = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);

    const int segments = 12;
    float segH = (bounds.getHeight() - 6.0f) / static_cast<float> (segments);

    for (int i = 0; i < segments; ++i)
    {
        float segNorm = static_cast<float> (i + 1) / static_cast<float> (segments);
        float sy = bounds.getBottom() - 3.0f - (i + 1) * segH;

        juce::Colour ledCol = juce::Colour (0xff22d3ee);
        if (i >= 10)     ledCol = juce::Colour (0xffef4444);
        else if (i >= 8) ledCol = juce::Colour (0xfff59e0b);

        g.setColour (norm >= segNorm ? ledCol : ledCol.withAlpha (0.15f));
        g.fillRect (bounds.getX() + 3.0f, sy + 1.0f, bounds.getWidth() - 6.0f, segH - 2.0f);
    }
}

//==============================================================================
// Blue Waveform Scope with Peak Markers & Preset Header
//==============================================================================
WaveformScopeComponent::WaveformScopeComponent (AuroraTriggerAudioProcessor& p) : processor (p) {}

void WaveformScopeComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0e1217));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (juce::Colour (0xff272b33));
    g.drawRoundedRectangle (bounds, 4.0f, 1.5f);

    // Subtle Grid
    g.setColour (juce::Colour (0xff1b222c));
    for (int gx = 1; gx < 8; ++gx)
        g.drawVerticalLine (static_cast<int> (bounds.getX() + bounds.getWidth() * gx / 8.0f), bounds.getY(), bounds.getBottom());

    float midY = bounds.getCentreY();
    g.drawHorizontalLine (static_cast<int> (midY), bounds.getX(), bounds.getRight());

    // Preset Header ("Chilli Snare")
    g.setColour (juce::Colour (0xffcbd5e1));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (processor.getPresetName(), static_cast<int> (bounds.getX() + 8), static_cast<int> (bounds.getY() + 4), 180, 16, juce::Justification::left);

    int readPos = processor.scopeWritePos.load();
    float stepX = bounds.getWidth() / static_cast<float> (AuroraTriggerAudioProcessor::scopeSize);

    for (int i = 0; i < AuroraTriggerAudioProcessor::scopeSize; ++i)
    {
        int idx = (readPos + i) % AuroraTriggerAudioProcessor::scopeSize;
        float val = processor.scopeData[(size_t) idx];
        int marker = processor.scopeMarkers[(size_t) idx];

        float xPos = bounds.getX() + i * stepX;
        float barH = std::abs (val) * (bounds.getHeight() * 0.44f);

        g.setColour (juce::Colour (0xff38bdf8).withAlpha (0.85f));
        g.drawVerticalLine (static_cast<int> (xPos), midY - barH, midY + barH);

        if (marker == 1)
        {
            g.setColour (juce::Colour (0xfff97316));
            g.drawVerticalLine (static_cast<int> (xPos), bounds.getY() + 18.0f, bounds.getBottom() - 6.0f);
            g.fillEllipse (xPos - 2.5f, midY - barH - 3.0f, 5.0f, 5.0f);
        }
        else if (marker == 2)
        {
            g.setColour (juce::Colour (0xffef4444));
            g.fillEllipse (xPos - 2.5f, bounds.getBottom() - 8.0f, 5.0f, 5.0f);
        }
    }

    float detailPct = processor.apvts.getRawParameterValue ("detail")->load() / 100.0f;
    float threshY = midY - (detailPct * (bounds.getHeight() * 0.44f));
    g.setColour (juce::Colour (0xfff43f5e).withAlpha (0.8f));
    g.drawHorizontalLine (static_cast<int> (threshY), bounds.getX() + 2.0f, bounds.getRight() - 2.0f);
}

void WaveformScopeComponent::updateDetailFromMouseY (int y)
{
    float midY = static_cast<float> (getHeight()) * 0.5f;
    float pct = juce::jlimit (1.0f, 100.0f, ((midY - static_cast<float> (y)) / (midY * 0.88f)) * 100.0f);
    if (auto* param = processor.apvts.getParameter ("detail"))
        param->setValueNotifyingHost (param->convertTo0to1 (pct));
    repaint();
}

void WaveformScopeComponent::mouseDown (const juce::MouseEvent& e) { updateDetailFromMouseY (e.y); }
void WaveformScopeComponent::mouseDrag (const juce::MouseEvent& e) { updateDetailFromMouseY (e.y); }


//==============================================================================
// Curves view displays
//==============================================================================
namespace
{
const juce::Colour curveOrange (0xfff97316);
const juce::Colour curveBlue   (0xff38bdf8);
const juce::Colour displayBg   (0xff1a2029);

float rawParam (AuroraTriggerAudioProcessor& p, int slot, const char* suffix)
{
    if (auto* v = p.apvts.getRawParameterValue ("slot" + juce::String (slot) + "_" + suffix))
        return v->load();
    return 0.0f;
}

void drawDisplayFrame (juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour (displayBg);
    g.fillRoundedRectangle (b, 3.0f);
    g.setColour (juce::Colour (0xff2c3038));
    g.drawRoundedRectangle (b, 3.0f, 1.0f);
}
} // namespace

void EnvelopeDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawDisplayFrame (g, b);
    auto area = b.reduced (6.0f, 7.0f);

    const float att = rawParam (processor, slotIndex, "att");   // ms
    const float sus = rawParam (processor, slotIndex, "sus");   // s
    const float rel = rawParam (processor, slotIndex, "rel");   // s

    // Attack gets up to a quarter of the width (sqrt scale so short attacks stay visible);
    // sustain and release share the rest in proportion.
    const float attW = std::sqrt (juce::jlimit (0.0f, 1.0f, att / 500.0f)) * area.getWidth() * 0.25f;
    const float rest = area.getWidth() - attW;
    const float total = sus + rel;
    const float susW = total > 0.0f ? rest * (sus / total) : 0.0f;
    const float relW = total > 0.0f ? rest * (rel / total) : 0.0f;

    const float x0 = area.getX();
    const float top = area.getY();
    const float bottom = area.getBottom();

    juce::Path p;
    p.startNewSubPath (x0, bottom);
    p.lineTo (x0 + attW, top);
    p.lineTo (x0 + attW + susW, top);
    p.lineTo (x0 + attW + susW + relW, bottom);

    juce::Path fill (p);
    fill.lineTo (x0, bottom);
    fill.closeSubPath();
    g.setColour (curveOrange.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (curveOrange);
    g.strokePath (p, juce::PathStrokeType (1.6f));
}

void VelocityCurveDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawDisplayFrame (g, b);
    auto area = b.reduced (5.0f);

    const float vel  = rawParam (processor, slotIndex, "vel");
    const float dyn  = rawParam (processor, slotIndex, "dyn");
    const float rmin = rawParam (processor, slotIndex, "rmin");
    const float rmax = rawParam (processor, slotIndex, "rmax");

    juce::Path outVel, level;
    const int steps = 48;
    for (int i = 0; i <= steps; ++i)
    {
        const float x01 = (float) i / (float) steps;
        const float v = AuroraTriggerAudioProcessor::mapVelocity (x01, vel, rmin, rmax);
        const float lv = AuroraTriggerAudioProcessor::dynamicsGain (v, dyn);
        const float x = area.getX() + x01 * area.getWidth();
        const float yv = area.getBottom() - (v / 127.0f) * area.getHeight();
        const float yl = area.getBottom() - lv * area.getHeight();
        if (i == 0) { outVel.startNewSubPath (x, yv); level.startNewSubPath (x, yl); }
        else        { outVel.lineTo (x, yv);          level.lineTo (x, yl); }
    }

    g.setColour (curveBlue.withAlpha (0.9f));
    g.strokePath (level, juce::PathStrokeType (1.4f));
    g.setColour (curveOrange);
    g.strokePath (outVel, juce::PathStrokeType (1.6f));
}

//==============================================================================
// Trigger pad
//==============================================================================
void PadButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onRightClick)
            onRightClick();
        return; // no click: the right button only opens the menu
    }
    juce::TextButton::mouseDown (e);
}

void PadButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (1.5f);

    juce::Colour top = slotLoaded ? juce::Colour (0xffe4e7ec) : juce::Colour (0xff3b3f48);
    juce::Colour bot = slotLoaded ? juce::Colour (0xffb7bcc6) : juce::Colour (0xff2a2d34);
    if (dropHover)
    {
        top = juce::Colour (0xff7dd3fc);
        bot = juce::Colour (0xff0ea5e9);
    }
    if (down)        { top = top.darker (0.15f); bot = bot.darker (0.15f); }
    else if (highlighted) { top = top.brighter (0.06f); bot = bot.brighter (0.06f); }

    g.setColour (juce::Colour (0xff111317));
    g.fillRoundedRectangle (b.expanded (1.0f), 4.0f);
    g.setGradientFill (juce::ColourGradient (top, b.getX(), b.getY(), bot, b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, 3.5f);

    if (getToggleState()) // selected slot
    {
        g.setColour (juce::Colour (0xff38bdf8));
        g.drawRoundedRectangle (b.reduced (1.0f), 3.5f, 2.0f);
    }

    g.setColour (slotLoaded || dropHover ? juce::Colour (0xff15181e) : juce::Colour (0xff8c929c));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawFittedText (getButtonText(), getLocalBounds().reduced (6, 4), juce::Justification::centred, 3, 0.85f);
}

bool PadButton::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString() == browserDragId;
}

void PadButton::itemDropped (const SourceDetails&)
{
    setDropHover (false);
    if (getBrowserSelection && onFileDropped)
    {
        const auto f = getBrowserSelection();
        if (AuroraTriggerAudioProcessor::isSupportedFile (f))
            onFileDropped (f);
    }
}

bool PadButton::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (AuroraTriggerAudioProcessor::isSupportedFile (juce::File (f)))
            return true;
    return false;
}

void PadButton::filesDropped (const juce::StringArray& files, int, int)
{
    setDropHover (false);
    for (const auto& f : files)
    {
        const juce::File file (f);
        if (AuroraTriggerAudioProcessor::isSupportedFile (file))
        {
            if (onFileDropped)
                onFileDropped (file);
            return;
        }
    }
}

//==============================================================================
// Main Editor
//==============================================================================
namespace
{
constexpr int stripStartX = 148;
constexpr int stripW = 107;
constexpr int stripGap = 4;
constexpr int stripTop = 264;
constexpr int stripH = 312;

int stripX (int i) { return stripStartX + i * (stripW + stripGap); }

struct CurveLinkDef
{
    const char* label;
    const char* paramId;
};

constexpr CurveLinkDef curveLinkDefs[] = {
    { "Attack",    "curveLinkAttack" },
    { "Sustain",   "curveLinkSustain" },
    { "Release",   "curveLinkRelease" },
    { "Dynamics",  "curveLinkDynamics" },
    { "Velocity",  "curveLinkVelocity" },
    { "Range Min", "curveLinkRangeMin" },
    { "Range Max", "curveLinkRangeMax" },
};

constexpr int curveLinkTop = 354;
constexpr int curveLinkRowH = 25;
} // namespace

AuroraTriggerAudioProcessorEditor::AuroraTriggerAudioProcessorEditor (AuroraTriggerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      inputMeter (p.inputLevelDb), waveformScope (p), outputMeter (p.outputLevelDb)
{
    setLookAndFeel (&metallicLnf);

    // Left Navigation Column
    addAndMakeVisible (browserBtn);
    addAndMakeVisible (triggeringBtn);
    addAndMakeVisible (settingsBtn);
    browserBtn.onClick    = [this] { setActiveTab (0); };
    triggeringBtn.onClick = [this] { setActiveTab (1); };
    settingsBtn.onClick   = [this] { setActiveTab (2); };

    addAndMakeVisible (viewMixerBtn);
    addAndMakeVisible (viewCurvesBtn);
    viewMixerBtn.onClick  = [this] { setLowerView (0); };
    viewCurvesBtn.onClick = [this] { setLowerView (1); };

    addAndMakeVisible (linkTuneBtn);
    linkTuneAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "linkTune", linkTuneBtn);

    addAndMakeVisible (flipPhasesBtn);
    flipPhasesBtn.onClick = [this] { audioProcessor.flipAllPhases(); };

    addAndMakeVisible (loadPresetBtn);
    addAndMakeVisible (savePresetBtn);
    loadPresetBtn.onClick = [this] { showPresetMenu(); };
    savePresetBtn.onClick = [this] { savePresetDialog(); };
    addAndMakeVisible (dragOnTrackBtn);
    addAndMakeVisible (clearBufferBtn);
    dragOnTrackBtn.onClick = [this] { dragCapturedMidiToDaw(); };
    clearBufferBtn.onClick = [this] { audioProcessor.clearMidiCapture(); };

    // Curve Link toggles
    for (size_t r = 0; r < curveLinks.size(); ++r)
    {
        auto& row = curveLinks[r];
        row.label = curveLinkDefs[r].label;
        row.button.setClickingTogglesState (true);
        row.button.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2d34));
        row.button.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffcbd5e1));
        addChildComponent (row.button);
        row.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, curveLinkDefs[r].paramId, row.button);
    }

    // Top Triggering Controls
    auto setupKnob = [this] (juce::Slider& s, const juce::String& suffix, juce::Colour capCol)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
        s.setTextValueSuffix (suffix);
        s.setColour (juce::Slider::rotarySliderFillColourId, capCol);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1c1f26));
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        addAndMakeVisible (s);
    };

    juce::Colour steelCap  (0xff9ca3af);
    juce::Colour copperCap (0xffc09a86);
    juce::Colour blueCap   (0xff7aa7d9);

    addAndMakeVisible (gateOnBtn);
    setupKnob (gateAttSlider,  " ms", copperCap);
    setupKnob (gateHoldSlider, " ms", copperCap);
    setupKnob (gateRelSlider,  " ms", copperCap);
    setupKnob (inVolSlider,    " dB", steelCap);
    addAndMakeVisible (inputMeter);

    setupKnob (lowcutSlider,   " hz", steelCap);
    setupKnob (highcutSlider,  " hz", steelCap);
    setupKnob (suppressSlider, "%",   copperCap);
    addAndMakeVisible (waveformScope);

    setupKnob (sensSlider,     "",    steelCap);
    setupKnob (retrigSlider,   " ms", copperCap);
    setupKnob (detailSlider,   "",    copperCap);
    addAndMakeVisible (outputMeter);
    setupKnob (outVolSlider,   " dB", steelCap);
    setupKnob (mixSlider,      "%",   steelCap);

    gateOnAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "gateOn", gateOnBtn);
    gateAttAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "gateAttack", gateAttSlider);
    gateHoldAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "gateHold", gateHoldSlider);
    gateRelAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "gateRelease", gateRelSlider);
    inVolAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "inputVol", inVolSlider);

    lowcutAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "lowcut", lowcutSlider);
    highcutAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "highcut", highcutSlider);
    suppressAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "suppress", suppressSlider);

    sensAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "sensitivity", sensSlider);
    retrigAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "retrigger", retrigSlider);
    detailAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "detail", detailSlider);
    outVolAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "outputVol", outVolSlider);
    mixAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "mix", mixSlider);

    //==========================================================================
    // Browser: folder tree stays on screen, expand sub-folders in place
    juce::PropertiesFile::Options opts;
    opts.applicationName = "AuroraTrigger";
    opts.folderName = "Aurora Audio";
    opts.filenameSuffix = ".settings";
    opts.osxLibrarySubFolder = "Application Support";
    userSettings = std::make_unique<juce::PropertiesFile> (opts);

    browserThread.startThread();
    browserTree.setDragAndDropDescription (PadButton::browserDragId);
    browserTree.addListener (this);
    browserTree.setColour (juce::TreeView::backgroundColourId, juce::Colour (0xff14171c));
    browserTree.setColour (juce::DirectoryContentsDisplayComponent::textColourId, juce::Colour (0xffe2e8f0));
    browserTree.setColour (juce::DirectoryContentsDisplayComponent::highlightColourId, juce::Colour (0xff0ea5e9));
    browserTree.setColour (juce::DirectoryContentsDisplayComponent::highlightedTextColourId, juce::Colours::white);
    browserTree.setItemHeight (20);
    addChildComponent (browserTree);

    for (auto* l : { &browserPathLabel, &browserTargetLabel, &browserInfoLabel })
    {
        l->setColour (juce::Label::textColourId, juce::Colour (0xffeceef2));
        l->setFont (juce::FontOptions (12.0f));
        addChildComponent (*l);
    }
    browserPathLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1c1f26));
    browserPathLabel.setMinimumHorizontalScale (0.6f);
    browserTargetLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    browserTargetLabel.setJustificationType (juce::Justification::topLeft);
    browserInfoLabel.setFont (juce::FontOptions (11.0f));
    browserInfoLabel.setJustificationType (juce::Justification::topLeft);

    for (auto* b : { &chooseFolderBtn, &parentFolderBtn, &loadSelectedBtn, &loadSampleBtn, &auditionSlotBtn, &resetChilliKitBtn })
        addChildComponent (*b);

    chooseFolderBtn.onClick   = [this] { chooseBrowserFolder(); };
    parentFolderBtn.onClick   = [this] { setBrowserRoot (browserList.getDirectory().getParentDirectory()); };
    loadSelectedBtn.onClick   = [this]
    {
        const auto f = browserTree.getSelectedFile();
        if (AuroraTriggerAudioProcessor::isSupportedFile (f))
            loadFileIntoSlot (selectedSlot, f);
    };
    loadSampleBtn.onClick     = [this] { loadSampleViaChooser (selectedSlot); };
    auditionSlotBtn.onClick   = [this] { audioProcessor.triggerAudition (selectedSlot, 0.92f); };
    resetChilliKitBtn.onClick = [this] { audioProcessor.loadDefaultChilliSnareKit(); };

    auto startFolder = juce::File (userSettings->getValue ("browserRoot"));
    if (! startFolder.isDirectory())
        startFolder = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    if (! startFolder.isDirectory())
        startFolder = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    setBrowserRoot (startFolder);

    // Bullet (U+2022) written as UTF-8 bytes so the file builds the same on every compiler
    settingsViewLabel.setText (juce::String (juce::CharPointer_UTF8 (
                                   "AURORA TRIGGER v" JucePlugin_VersionString " - ENGINE SETTINGS\n"
                                   "\xe2\x80\xa2 Link Tune: Synchronizes pitch across all 8 multi-mic channels\n"
                                   "\xe2\x80\xa2 Independent Gate: Cleans original mic bleed even with Mix at 0%\n"
                                   "\xe2\x80\xa2 MIDI Capture: Records hits automatically for DRAG ON TRACK export\n"
                                   "\xe2\x80\xa2 TCI instruments: velocity layers and round robins are picked per hit")),
                               juce::dontSendNotification);
    settingsViewLabel.setJustificationType (juce::Justification::centred);
    addChildComponent (settingsViewLabel);

    //==========================================================================
    // 8-Channel Mixer + Curves strips + pads
    juce::Colour purplePan (0xffa855f7);
    for (int i = 0; i < AuroraTriggerAudioProcessor::numSlots; ++i)
    {
        auto& st = strips[(size_t) i];
        const auto prefix = "slot" + juce::String (i) + "_";

        st.panSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        st.panSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        st.panSlider.setColour (juce::Slider::rotarySliderFillColourId, purplePan);
        addAndMakeVisible (st.panSlider);

        st.panReadout.setText ("<C>", juce::dontSendNotification);
        st.panReadout.setJustificationType (juce::Justification::centred);
        st.panReadout.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1a1d24));
        addAndMakeVisible (st.panReadout);

        st.tuneSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        st.tuneSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        st.tuneSlider.setColour (juce::Slider::rotarySliderFillColourId, copperCap);
        st.tuneSlider.onValueChange = [this, i]
        {
            float val = static_cast<float> (strips[(size_t) i].tuneSlider.getValue());
            strips[(size_t) i].tuneReadout.setText (juce::String (val, 1), juce::dontSendNotification);
            audioProcessor.applyLinkedTune (i, val);
        };
        addAndMakeVisible (st.tuneSlider);

        st.tuneReadout.setText ("0", juce::dontSendNotification);
        st.tuneReadout.setJustificationType (juce::Justification::centred);
        st.tuneReadout.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1a1d24));
        addAndMakeVisible (st.tuneReadout);

        st.volSlider.setSliderStyle (juce::Slider::LinearVertical);
        st.volSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
        st.volSlider.setTextValueSuffix (" dB");
        st.volSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1a1d24));
        addAndMakeVisible (st.volSlider);

        st.soloBtn.setClickingTogglesState (true);
        st.muteBtn.setClickingTogglesState (true);
        st.phaseBtn.setClickingTogglesState (true);
        addAndMakeVisible (st.soloBtn);
        addAndMakeVisible (st.muteBtn);
        addAndMakeVisible (st.phaseBtn);

        st.artLabel.setJustificationType (juce::Justification::centred);
        st.artLabel.setColour (juce::Label::textColourId, juce::Colour (0xff1e222b));
        st.artLabel.setFont (juce::FontOptions (11.0f));
        addAndMakeVisible (st.artLabel);

        // Pad: left click audition (or load when empty), right click menu, drop files
        auto& pad = st.bottomPadBtn;
        pad.setButtonText ("Empty");
        pad.onClick = [this, i]
        {
            selectSlot (i);
            if (audioProcessor.getSlotInfo (i).loaded)
                audioProcessor.triggerAudition (i, 0.90f);
            else
                loadSampleViaChooser (i);
        };
        pad.onRightClick = [this, i]
        {
            selectSlot (i);
            showPadMenu (i);
        };
        pad.onFileDropped = [this, i] (const juce::File& f) { loadFileIntoSlot (i, f); };
        pad.getBrowserSelection = [this] { return browserTree.getSelectedFile(); };
        addAndMakeVisible (pad);

        st.panAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + "pan", st.panSlider);
        st.tuneAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + "tune", st.tuneSlider);
        st.volAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + "vol", st.volSlider);
        st.soloAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, prefix + "solo", st.soloBtn);
        st.muteAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, prefix + "mute", st.muteBtn);
        st.phaseAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, prefix + "phase", st.phaseBtn);

        // Curves strip
        auto cs = std::make_unique<CurveStripUI> (audioProcessor, i);
        addChildComponent (cs->envelope);
        addChildComponent (cs->velocityCurve);

        auto setupCurveKnob = [&] (juce::Slider& s, const char* suffix, juce::Colour cap, bool valueBelow)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            if (valueBelow)
                s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 15);
            else
                s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 17);
            s.setColour (juce::Slider::rotarySliderFillColourId, cap);
            s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1c1f26));
            s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
            s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff2c3038));
            addChildComponent (s);
            cs->attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + suffix, s));
            const juce::String sfx (suffix);
            s.onValueChange = [this, i, sfx, &s] { audioProcessor.applyLinkedSlotParam (sfx, i, (float) s.getValue()); };
        };

        setupCurveKnob (cs->attSlider,  "att",  copperCap, false);
        setupCurveKnob (cs->susSlider,  "sus",  copperCap, false);
        setupCurveKnob (cs->relSlider,  "rel",  copperCap, false);
        setupCurveKnob (cs->dynSlider,  "dyn",  copperCap, false);
        setupCurveKnob (cs->velSlider,  "vel",  blueCap,   false);
        setupCurveKnob (cs->rminSlider, "rmin", blueCap,   true);
        setupCurveKnob (cs->rmaxSlider, "rmax", blueCap,   true);
        curveStrips[(size_t) i] = std::move (cs);
    }

    selectSlot (0);
    setActiveTab (1);
    setLowerView (0);

    // Last: setSize() calls resized(), which needs every component above to exist
    setSize (1040, 640);
    startTimerHz (30);
}

AuroraTriggerAudioProcessorEditor::~AuroraTriggerAudioProcessorEditor()
{
    stopTimer();
    browserTree.removeListener (this);
    browserThread.stopThread (2000);
    setLookAndFeel (nullptr);
}

void AuroraTriggerAudioProcessorEditor::setActiveTab (int tabIndex)
{
    activeViewTab = tabIndex;

    browserBtn.setToggleState    (tabIndex == 0, juce::dontSendNotification);
    triggeringBtn.setToggleState (tabIndex == 1, juce::dontSendNotification);
    settingsBtn.setToggleState   (tabIndex == 2, juce::dontSendNotification);

    const bool isTrig = (tabIndex == 1);
    for (auto* c : std::initializer_list<juce::Component*> { &gateOnBtn, &gateAttSlider, &gateHoldSlider, &gateRelSlider, &inVolSlider,
                                                             &inputMeter, &lowcutSlider, &highcutSlider, &suppressSlider, &waveformScope,
                                                             &sensSlider, &retrigSlider, &detailSlider, &outputMeter, &outVolSlider, &mixSlider })
        c->setVisible (isTrig);

    const bool isBrowser = (tabIndex == 0);
    for (auto* c : std::initializer_list<juce::Component*> { &browserTree, &chooseFolderBtn, &parentFolderBtn, &browserPathLabel,
                                                             &browserTargetLabel, &browserInfoLabel, &loadSelectedBtn, &loadSampleBtn,
                                                             &auditionSlotBtn, &resetChilliKitBtn })
        c->setVisible (isBrowser);

    settingsViewLabel.setVisible (tabIndex == 2);
    repaint();
}

void AuroraTriggerAudioProcessorEditor::setLowerView (int view)
{
    lowerView = view;
    viewMixerBtn.setToggleState (view == 0, juce::dontSendNotification);
    viewCurvesBtn.setToggleState (view == 1, juce::dontSendNotification);

    const bool mixer = (view == 0);
    for (auto& st : strips)
        for (auto* c : std::initializer_list<juce::Component*> { &st.panSlider, &st.panReadout, &st.tuneSlider, &st.tuneReadout, &st.volSlider,
                                                                 &st.soloBtn, &st.muteBtn, &st.phaseBtn, &st.artLabel })
            c->setVisible (mixer);

    for (auto& cs : curveStrips)
        for (auto* c : std::initializer_list<juce::Component*> { &cs->envelope, &cs->velocityCurve, &cs->attSlider, &cs->susSlider, &cs->relSlider,
                                                                 &cs->dynSlider, &cs->velSlider, &cs->rminSlider, &cs->rmaxSlider })
            c->setVisible (! mixer);

    linkTuneBtn.setVisible (mixer);
    flipPhasesBtn.setVisible (mixer);
    loadPresetBtn.setVisible (mixer);
    savePresetBtn.setVisible (mixer);
    for (auto& row : curveLinks)
        row.button.setVisible (! mixer);

    repaint();
}

void AuroraTriggerAudioProcessorEditor::selectSlot (int slotIndex)
{
    selectedSlot = juce::jlimit (0, AuroraTriggerAudioProcessor::numSlots - 1, slotIndex);
    for (int i = 0; i < AuroraTriggerAudioProcessor::numSlots; ++i)
        strips[(size_t) i].bottomPadBtn.setToggleState (i == selectedSlot, juce::dontSendNotification);
}

void AuroraTriggerAudioProcessorEditor::loadFileIntoSlot (int slotIndex, const juce::File& file)
{
    if (! AuroraTriggerAudioProcessor::isSupportedFile (file))
        return;
    selectSlot (slotIndex);
    audioProcessor.loadFileIntoSlotAsync (slotIndex, file);
}

void AuroraTriggerAudioProcessorEditor::showPadMenu (int slotIndex)
{
    const auto info = audioProcessor.getSlotInfo (slotIndex);
    const auto browserFile = browserTree.getSelectedFile();
    const bool browserFileOk = AuroraTriggerAudioProcessor::isSupportedFile (browserFile);

    juce::PopupMenu menu;
    menu.addSectionHeader ("Slot " + juce::String (slotIndex + 1) + ": " + info.micLabel);
    menu.addItem (1, "Load Sample / TCI...");
    menu.addItem (2, browserFileOk ? "Load \"" + browserFile.getFileName() + "\" from Browser" : juce::String ("Load from Browser (select a file first)"), browserFileOk);
    menu.addItem (3, "Audition", info.loaded);
    menu.addSeparator();
    menu.addItem (4, "Clear Slot (Init)");

    juce::Component::SafePointer<AuroraTriggerAudioProcessorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&strips[(size_t) slotIndex].bottomPadBtn),
                        [safe, slotIndex, browserFile] (int result)
    {
        if (safe == nullptr)
            return;
        switch (result)
        {
            case 1: safe->loadSampleViaChooser (slotIndex); break;
            case 2: safe->loadFileIntoSlot (slotIndex, browserFile); break;
            case 3: safe->audioProcessor.triggerAudition (slotIndex, 0.90f); break;
            case 4: safe->audioProcessor.initSlot (slotIndex); break;
            default: break;
        }
    });
}

void AuroraTriggerAudioProcessorEditor::loadSampleViaChooser (int slotIndex)
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load into Slot " + juce::String (slotIndex + 1) + " (.wav / .aiff / .flac / .tci)",
                                                       browserList.getDirectory(),
                                                       AuroraTriggerAudioProcessor::supportedWildcard());
    const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    juce::Component::SafePointer<AuroraTriggerAudioProcessorEditor> safe (this);
    fileChooser->launchAsync (chooserFlags, [safe, slotIndex] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (safe != nullptr && file.existsAsFile())
            safe->loadFileIntoSlot (slotIndex, file);
    });
}

void AuroraTriggerAudioProcessorEditor::setBrowserRoot (const juce::File& folder)
{
    auto dir = folder.isDirectory() ? folder : juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    browserList.setDirectory (dir, true, true);
    browserPathLabel.setText (dir.getFullPathName(), juce::dontSendNotification);
    if (userSettings != nullptr)
    {
        userSettings->setValue ("browserRoot", dir.getFullPathName());
        userSettings->saveIfNeeded();
    }
}

void AuroraTriggerAudioProcessorEditor::chooseBrowserFolder()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Choose a sample folder", browserList.getDirectory());
    const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;

    juce::Component::SafePointer<AuroraTriggerAudioProcessorEditor> safe (this);
    fileChooser->launchAsync (chooserFlags, [safe] (const juce::FileChooser& fc)
    {
        const auto dir = fc.getResult();
        if (safe != nullptr && dir.isDirectory())
            safe->setBrowserRoot (dir);
    });
}

void AuroraTriggerAudioProcessorEditor::fileDoubleClicked (const juce::File& file)
{
    // Folders just open in place (the tree keeps the list on screen); files load.
    if (! file.existsAsFile())
        return;
    if (file.hasFileExtension (AuroraTriggerAudioProcessor::presetExtension))
        loadPresetFile (file);
    else
        loadFileIntoSlot (selectedSlot, file);
}

//==============================================================================
// Presets
void AuroraTriggerAudioProcessorEditor::showError (const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::WarningIcon)
                                      .withTitle (title)
                                      .withMessage (message)
                                      .withButton ("OK")
                                      .withAssociatedComponent (this),
                                  nullptr);
}

void AuroraTriggerAudioProcessorEditor::savePresetDialog()
{
    const auto folder = AuroraTriggerAudioProcessor::getPresetsFolder();
    folder.createDirectory();
    const auto suggested = folder.getChildFile (juce::File::createLegalFileName (audioProcessor.getPresetName()))
                               .withFileExtension (AuroraTriggerAudioProcessor::presetExtension);

    fileChooser = std::make_unique<juce::FileChooser> ("Save Aurora Trigger Preset", suggested, "*.atpreset");
    const auto chooserFlags = juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting;

    juce::Component::SafePointer<AuroraTriggerAudioProcessorEditor> safe (this);
    fileChooser->launchAsync (chooserFlags, [safe] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (safe == nullptr || file == juce::File())
            return;

        juce::String error;
        if (! safe->audioProcessor.savePreset (file, error))
        {
            safe->showError ("Preset not saved", error);
            return;
        }

        safe->savePresetBtn.setButtonText ("SAVED!");
        juce::Timer::callAfterDelay (1500, [safe]
        {
            if (safe != nullptr)
                safe->savePresetBtn.setButtonText ("SAVE PRESET");
        });
        safe->waveformScope.repaint();
    });
}

void AuroraTriggerAudioProcessorEditor::loadPresetFile (const juce::File& file)
{
    juce::String error;
    if (! audioProcessor.loadPreset (file, error))
        showError ("Preset not loaded", error);
    waveformScope.repaint();
}

void AuroraTriggerAudioProcessorEditor::showPresetMenu()
{
    const auto folder = AuroraTriggerAudioProcessor::getPresetsFolder();
    auto files = folder.findChildFiles (juce::File::findFiles, true, "*.atpreset");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFullPathName().compareNatural (b.getFullPathName()) < 0;
    });

    const auto current = audioProcessor.getPresetName();
    juce::PopupMenu menu;
    menu.addSectionHeader ("Presets");
    if (files.isEmpty())
        menu.addItem (-1, "(no presets saved yet)", false);
    for (int i = 0; i < files.size(); ++i)
    {
        const auto name = files[i].getRelativePathFrom (folder).upToLastOccurrenceOf (".", false, false);
        menu.addItem (100 + i, name, true, files[i].getFileNameWithoutExtension() == current);
    }
    menu.addSeparator();
    menu.addItem (1, "Load From File...");
    menu.addItem (2, "Open Presets Folder");

    juce::Component::SafePointer<AuroraTriggerAudioProcessorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&loadPresetBtn),
                        [safe, files, folder] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        if (result >= 100 && result - 100 < files.size())
        {
            safe->loadPresetFile (files[result - 100]);
        }
        else if (result == 1)
        {
            safe->fileChooser = std::make_unique<juce::FileChooser> ("Load Aurora Trigger Preset", folder, "*.atpreset");
            safe->fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                            [safe] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (safe != nullptr && file.existsAsFile())
                    safe->loadPresetFile (file);
            });
        }
        else if (result == 2)
        {
            folder.createDirectory();
            folder.startAsProcess();
        }
    });
}

void AuroraTriggerAudioProcessorEditor::dragCapturedMidiToDaw()
{
    auto tempMid = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("AuroraTrigger_Capture.mid");
    if (audioProcessor.exportCapturedMidiToFile (tempMid))
        performExternalDragDropOfFiles ({ tempMid.getFullPathName() }, false);
}

void AuroraTriggerAudioProcessorEditor::timerCallback()
{
    if (activeViewTab == 1)
    {
        waveformScope.repaint();
        inputMeter.repaint();
        outputMeter.repaint();
    }

    for (int i = 0; i < AuroraTriggerAudioProcessor::numSlots; ++i)
    {
        const auto info = audioProcessor.getSlotInfo (i);
        auto& st = strips[(size_t) i];

        st.artLabel.setText (info.articulationName, juce::dontSendNotification);
        st.bottomPadBtn.setButtonText (info.loading ? juce::String ("Loading...") : info.micLabel);
        if (st.bottomPadBtn.slotLoaded != info.loaded)
        {
            st.bottomPadBtn.slotLoaded = info.loaded;
            st.bottomPadBtn.repaint();
        }

        float panVal = static_cast<float> (st.panSlider.getValue());
        if (std::abs (panVal) < 0.03f)
            st.panReadout.setText ("<C>", juce::dontSendNotification);
        else if (panVal < 0.0f)
            st.panReadout.setText ("L" + juce::String (static_cast<int> (-panVal * 100.0f)), juce::dontSendNotification);
        else
            st.panReadout.setText ("R" + juce::String (static_cast<int> (panVal * 100.0f)), juce::dontSendNotification);

        if (lowerView == 1)
        {
            curveStrips[(size_t) i]->envelope.repaint();
            curveStrips[(size_t) i]->velocityCurve.repaint();
        }
    }

    for (auto& row : curveLinks)
        row.button.setButtonText (row.button.getToggleState() ? "On" : "Off");

    if (activeViewTab == 0)
    {
        const auto info = audioProcessor.getSlotInfo (selectedSlot);
        browserTargetLabel.setText ("Target: Slot " + juce::String (selectedSlot + 1) + "\n" + info.micLabel, juce::dontSendNotification);

        juce::String text;
        if (info.loading)
            text = "Loading...";
        else if (info.loaded)
            text = juce::String (info.numLayers) + (info.numLayers == 1 ? " velocity layer, " : " velocity layers, ")
                 + juce::String (info.numHits) + (info.numHits == 1 ? " hit" : " hits");
        if (info.error.isNotEmpty())
            text << (text.isEmpty() ? "" : "\n") << info.error;
        text << (text.isEmpty() ? "" : "\n")
             << "Double-click a file or drag it onto a pad. Right-click a pad to clear it.";
        browserInfoLabel.setText (text, juce::dontSendNotification);
    }
}

void AuroraTriggerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff484c54));

    // Left Sidebar Metallic Panel
    g.setColour (juce::Colour (0xff6e737d));
    g.fillRect (6, 6, 136, getHeight() - 12);
    g.setColour (juce::Colour (0xff2c3038));
    g.drawRect (6, 6, 136, getHeight() - 12, 1);

    // Group boxes in the sidebar
    auto groupBox = [&g] (float y, float h, const juce::String& title)
    {
        g.setColour (juce::Colour (0xff9ea4b0));
        g.drawRoundedRectangle (12.0f, y, 124.0f, h, 4.0f, 1.0f);
        g.setColour (juce::Colour (0xff6e737d));
        g.fillRect (30, (int) y - 7, 88, 14);
        g.setColour (juce::Colour (0xffeceef2));
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (title, 16, (int) y - 7, 116, 14, juce::Justification::centred);
    };

    groupBox (546.0f, 82.0f, "MIDI CAPTURE");

    if (lowerView == 1)
    {
        groupBox ((float) curveLinkTop - 8.0f, (float) (curveLinks.size() * curveLinkRowH) + 12.0f, "CURVE LINK");
        g.setColour (juce::Colour (0xfff1f5f9));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        for (size_t r = 0; r < curveLinks.size(); ++r)
            g.drawText (curveLinks[r].label, 18, curveLinkTop + (int) r * curveLinkRowH, 78, 22, juce::Justification::centredLeft);
    }

    if (activeViewTab == 1)
    {
        // Top Section Backgrounds
        g.setColour (juce::Colour (0xff585c66));
        g.fillRoundedRectangle (148.0f, 6.0f, 146.0f, 250.0f, 4.0f); // Gate + Input
        g.fillRoundedRectangle (300.0f, 6.0f, 520.0f, 250.0f, 4.0f); // Filters + Scope
        g.fillRoundedRectangle (826.0f, 6.0f, 208.0f, 250.0f, 4.0f); // Sensitivity + Output

        g.setColour (juce::Colour (0xffeceef2));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));

        g.drawText ("GATE",     152, 10, 64, 14, juce::Justification::centred);
        g.drawText ("ATTACK",   152, 54, 64, 14, juce::Justification::centred);
        g.drawText ("HOLD",     152, 122, 64, 14, juce::Justification::centred);
        g.drawText ("RELEASE",  152, 190, 64, 14, juce::Justification::centred);
        g.drawText ("INPUT",    224, 10, 64, 14, juce::Justification::centred);
        g.drawText ("VOLUME",   224, 122, 64, 14, juce::Justification::centred);

        g.drawText ("LOWCUT",   306, 12, 86, 14, juce::Justification::centred);
        g.drawText ("HIGHCUT",  306, 86, 86, 14, juce::Justification::centred);
        g.drawText ("SUPPRESS", 306, 178, 86, 14, juce::Justification::centred);

        g.drawText ("SENSITIVITY", 834, 24, 114, 14, juce::Justification::centred);
        g.drawText ("RETRIGGER",   834, 98, 114, 14, juce::Justification::centred);
        g.drawText ("DETAIL",      834, 172, 114, 14, juce::Justification::centred);
        g.drawText ("OUTPUT",      956, 10, 72, 14, juce::Justification::centred);
        g.drawText ("VOLUME",      956, 122, 72, 14, juce::Justification::centred);
        g.drawText ("MIX",         956, 188, 72, 14, juce::Justification::centred);
    }
    else if (activeViewTab == 0)
    {
        g.setColour (juce::Colour (0xff585c66));
        g.fillRoundedRectangle (148.0f, 6.0f, 886.0f, 250.0f, 4.0f);
        g.setColour (juce::Colour (0xffeceef2));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("BROWSER  (.wav  .aiff  .flac  .tci)", 158, 10, 400, 14, juce::Justification::centredLeft);
    }

    // Lower strips
    for (int i = 0; i < AuroraTriggerAudioProcessor::numSlots; ++i)
    {
        const int sx = stripX (i);
        g.setColour (juce::Colour (0xff8a909c));
        g.fillRoundedRectangle (static_cast<float> (sx), (float) stripTop, static_cast<float> (stripW), (float) stripH, 4.0f);
        g.setColour (juce::Colour (0xff2c3038));
        g.drawRoundedRectangle (static_cast<float> (sx), (float) stripTop, static_cast<float> (stripW), (float) stripH, 4.0f, 1.0f);

        g.setColour (juce::Colour (0xff1a1d24));
        if (lowerView == 0)
        {
            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            g.drawText ("PAN",  sx, 268, stripW, 14, juce::Justification::centred);
            g.drawText ("TUNE", sx, 336, stripW, 14, juce::Justification::centred);
        }
        else
        {
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText ("A",   sx + 3, 330, 12, 22, juce::Justification::centred);
            g.drawText ("S",   sx + 3, 354, 12, 22, juce::Justification::centred);
            g.drawText ("R",   sx + 3, 378, 12, 22, juce::Justification::centred);
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText ("DYN", sx + 2, 466, 26, 22, juce::Justification::centredLeft);
            g.drawText ("VEL", sx + 2, 490, 26, 22, juce::Justification::centredLeft);
            g.drawText ("RANGE", sx, 513, stripW, 12, juce::Justification::centred);
        }
    }
}

void AuroraTriggerAudioProcessorEditor::resized()
{
    int lx = 14;
    int lw = 120;

    browserBtn.setBounds     (lx, 74, lw, 28);
    triggeringBtn.setBounds  (lx, 110, lw, 28);
    settingsBtn.setBounds    (lx, 146, lw, 28);

    viewMixerBtn.setBounds   (lx, 268, lw, 28);
    viewCurvesBtn.setBounds  (lx, 304, lw, 28);

    linkTuneBtn.setBounds    (lx, 400, lw, 26);
    flipPhasesBtn.setBounds  (lx, 434, lw, 26);
    loadPresetBtn.setBounds  (lx, 468, lw, 26);
    savePresetBtn.setBounds  (lx, 502, lw, 26);

    for (size_t r = 0; r < curveLinks.size(); ++r)
        curveLinks[r].button.setBounds (96, curveLinkTop + (int) r * curveLinkRowH + 2, 34, 18);

    dragOnTrackBtn.setBounds (lx + 4, 560, lw - 8, 26);
    clearBufferBtn.setBounds (lx + 4, 592, lw - 8, 26);

    // Top Triggering Controls
    gateOnBtn.setBounds      (162, 26, 44, 22);
    gateAttSlider.setBounds  (156, 68, 56, 52);
    gateHoldSlider.setBounds (156, 136, 56, 52);
    gateRelSlider.setBounds  (156, 204, 56, 52);

    inputMeter.setBounds     (238, 26, 36, 90);
    inVolSlider.setBounds    (228, 136, 56, 64);

    lowcutSlider.setBounds   (320, 28, 58, 54);
    highcutSlider.setBounds  (320, 102, 58, 54);
    suppressSlider.setBounds (320, 194, 58, 54);
    waveformScope.setBounds  (402, 14, 410, 234);

    sensSlider.setBounds     (840, 40, 56, 52);
    retrigSlider.setBounds   (840, 114, 56, 52);
    detailSlider.setBounds   (840, 188, 56, 52);

    outputMeter.setBounds    (974, 26, 36, 90);
    outVolSlider.setBounds   (964, 136, 56, 48);
    mixSlider.setBounds      (964, 202, 56, 48);

    // Browser
    chooseFolderBtn.setBounds    (156, 28, 128, 24);
    parentFolderBtn.setBounds    (290, 28, 48, 24);
    browserPathLabel.setBounds   (344, 28, 434, 24);
    browserTree.setBounds        (156, 58, 622, 192);
    browserTargetLabel.setBounds (790, 26, 236, 36);
    loadSelectedBtn.setBounds    (790, 64, 236, 26);
    loadSampleBtn.setBounds      (790, 94, 236, 26);
    auditionSlotBtn.setBounds    (790, 124, 236, 26);
    resetChilliKitBtn.setBounds  (790, 154, 236, 26);
    browserInfoLabel.setBounds   (790, 184, 236, 66);

    settingsViewLabel.setBounds (220, 50, 740, 150);

    for (int i = 0; i < AuroraTriggerAudioProcessor::numSlots; ++i)
    {
        const int sx = stripX (i);
        auto& st = strips[(size_t) i];

        // Mixer view
        st.panSlider.setBounds   (sx + 35, 282, 36, 34);
        st.panReadout.setBounds  (sx + 24, 317, 58, 16);

        st.tuneSlider.setBounds  (sx + 35, 350, 36, 34);
        st.tuneReadout.setBounds (sx + 24, 385, 58, 16);

        st.volSlider.setBounds   (sx + 2, 408, 60, 144);
        st.soloBtn.setBounds     (sx + 62, 418, 28, 24);
        st.muteBtn.setBounds     (sx + 62, 450, 28, 24);
        st.phaseBtn.setBounds    (sx + 62, 482, 28, 24);

        st.artLabel.setBounds    (sx + 4, 554, stripW - 8, 18);
        st.bottomPadBtn.setBounds(sx, 582, stripW, 48);

        // Curves view
        if (curveStrips[(size_t) i] == nullptr)
            continue;
        auto& cs = *curveStrips[(size_t) i];
        cs.envelope.setBounds      (sx + 4, 268, stripW - 8, 58);
        cs.attSlider.setBounds     (sx + 16, 330, stripW - 20, 22);
        cs.susSlider.setBounds     (sx + 16, 354, stripW - 20, 22);
        cs.relSlider.setBounds     (sx + 16, 378, stripW - 20, 22);
        cs.velocityCurve.setBounds (sx + 4, 404, stripW - 8, 58);
        cs.dynSlider.setBounds     (sx + 28, 466, stripW - 32, 22);
        cs.velSlider.setBounds     (sx + 28, 490, stripW - 32, 22);
        cs.rminSlider.setBounds    (sx + 6, 526, 46, 46);
        cs.rmaxSlider.setBounds    (sx + 55, 526, 46, 46);
    }
}
