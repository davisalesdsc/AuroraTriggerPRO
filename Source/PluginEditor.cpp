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
    g.drawText (processor.activePresetName, static_cast<int> (bounds.getX() + 8), static_cast<int> (bounds.getY() + 4), 180, 16, juce::Justification::left);

    int readPos = processor.scopeWritePos.load();
    float stepX = bounds.getWidth() / static_cast<float> (AuroraTriggerAudioProcessor::scopeSize);

    for (int i = 0; i < AuroraTriggerAudioProcessor::scopeSize; ++i)
    {
        int idx = (readPos + i) % AuroraTriggerAudioProcessor::scopeSize;
        float val = processor.scopeData[idx];
        int marker = processor.scopeMarkers[idx];

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
// Main Editor
//==============================================================================
AuroraTriggerAudioProcessorEditor::AuroraTriggerAudioProcessorEditor (AuroraTriggerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      inputMeter (p.inputLevelDb), waveformScope (p), outputMeter (p.outputLevelDb)
{
    setLookAndFeel (&metallicLnf);
    setSize (1040, 640);

    // Left Navigation Column
    addAndMakeVisible (browserBtn);
    addAndMakeVisible (triggeringBtn);
    addAndMakeVisible (settingsBtn);
    browserBtn.onClick    = [this] { setActiveTab (0); };
    triggeringBtn.onClick = [this] { setActiveTab (1); };
    settingsBtn.onClick   = [this] { setActiveTab (2); };

    addAndMakeVisible (viewMixerBtn);
    addAndMakeVisible (viewCurvesBtn);
    viewMixerBtn.onClick = [this]
    {
        mixerVisible = ! mixerVisible;
        setSize (1040, mixerVisible ? 640 : 320);
        resized();
    };

    addAndMakeVisible (linkTuneBtn);
    linkTuneAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "linkTune", linkTuneBtn);

    addAndMakeVisible (flipPhasesBtn);
    flipPhasesBtn.onClick = [this] { audioProcessor.flipAllPhases(); };

    addAndMakeVisible (savePresetBtn);
    addAndMakeVisible (dragOnTrackBtn);
    addAndMakeVisible (clearBufferBtn);
    dragOnTrackBtn.onClick = [this] { dragCapturedMidiToDaw(); };
    clearBufferBtn.onClick = [this] { audioProcessor.clearMidiCapture(); };

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

    // Browser & Settings Views
    addChildComponent (loadSampleBtn);
    addChildComponent (auditionSlotBtn);
    addChildComponent (resetChilliKitBtn);
    addChildComponent (browserViewLabel);
    addChildComponent (settingsViewLabel);

    loadSampleBtn.onClick     = [this] { loadSampleViaChooser (selectedSlot); };
    auditionSlotBtn.onClick   = [this] { audioProcessor.triggerAudition (selectedSlot, 0.92f); };
    resetChilliKitBtn.onClick = [this] { audioProcessor.loadDefaultChilliSnareKit(); };

    browserViewLabel.setText ("SAMPLE BROWSER & MULTI-MIC KIT LOADER\n"
                              "Click any bottom channel pad to audition or load a custom .WAV / .AIFF file.",
                              juce::dontSendNotification);
    browserViewLabel.setJustificationType (juce::Justification::centred);

    // Bullet (U+2022) written as UTF-8 bytes so the file builds the same on every compiler
    settingsViewLabel.setText (juce::String (juce::CharPointer_UTF8 (
                                   "AURORA TRIGGER v0.2 - ENGINE SETTINGS\n"
                                   "\xe2\x80\xa2 Link Tune: Synchronizes pitch across all 8 multi-mic channels\n"
                                   "\xe2\x80\xa2 Independent Gate: Cleans original mic bleed even with Mix at 0%\n"
                                   "\xe2\x80\xa2 MIDI Capture: Records hits automatically for DRAG ON TRACK export")),
                               juce::dontSendNotification);
    settingsViewLabel.setJustificationType (juce::Justification::centred);

    // 8-Channel Multi-Channel Mixer
    juce::Colour purplePan (0xffa855f7);
    for (int i = 0; i < AuroraTriggerAudioProcessor::numSlots; ++i)
    {
        auto& st = strips[i];
        auto prefix = "slot" + juce::String (i) + "_";

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
            float val = static_cast<float> (strips[i].tuneSlider.getValue());
            strips[i].tuneReadout.setText (juce::String (val, 1), juce::dontSendNotification);
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
        addAndMakeVisible (st.artLabel);

        st.bottomPadBtn.onClick = [this, i]
        {
            selectedSlot = i;
            if (audioProcessor.slots[i].loaded)
                audioProcessor.triggerAudition (i, 0.90f);
            else
                loadSampleViaChooser (i);
        };
        addAndMakeVisible (st.bottomPadBtn);

        st.panAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + "pan", st.panSlider);
        st.tuneAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + "tune", st.tuneSlider);
        st.volAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, prefix + "vol", st.volSlider);
        st.soloAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, prefix + "solo", st.soloBtn);
        st.muteAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, prefix + "mute", st.muteBtn);
        st.phaseAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, prefix + "phase", st.phaseBtn);
    }

    setActiveTab (1);
    startTimerHz (30);
}

AuroraTriggerAudioProcessorEditor::~AuroraTriggerAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void AuroraTriggerAudioProcessorEditor::setActiveTab (int tabIndex)
{
    activeViewTab = tabIndex;

    browserBtn.setToggleState    (tabIndex == 0, juce::dontSendNotification);
    triggeringBtn.setToggleState (tabIndex == 1, juce::dontSendNotification);
    settingsBtn.setToggleState   (tabIndex == 2, juce::dontSendNotification);

    bool isTrig = (tabIndex == 1);
    gateOnBtn.setVisible (isTrig);
    gateAttSlider.setVisible (isTrig);
    gateHoldSlider.setVisible (isTrig);
    gateRelSlider.setVisible (isTrig);
    inVolSlider.setVisible (isTrig);
    inputMeter.setVisible (isTrig);
    lowcutSlider.setVisible (isTrig);
    highcutSlider.setVisible (isTrig);
    suppressSlider.setVisible (isTrig);
    waveformScope.setVisible (isTrig);
    sensSlider.setVisible (isTrig);
    retrigSlider.setVisible (isTrig);
    detailSlider.setVisible (isTrig);
    outputMeter.setVisible (isTrig);
    outVolSlider.setVisible (isTrig);
    mixSlider.setVisible (isTrig);

    bool isBrowser = (tabIndex == 0);
    loadSampleBtn.setVisible (isBrowser);
    auditionSlotBtn.setVisible (isBrowser);
    resetChilliKitBtn.setVisible (isBrowser);
    browserViewLabel.setVisible (isBrowser);

    bool isSettings = (tabIndex == 2);
    settingsViewLabel.setVisible (isSettings);

    repaint();
}

void AuroraTriggerAudioProcessorEditor::loadSampleViaChooser (int slotIndex)
{
    fileChooser = std::make_unique<juce::FileChooser> ("Select Drum Mic Sample (.wav / .aiff)",
                                                       juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                       "*.wav;*.aiff;*.flac");
    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (flags, [this, slotIndex] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
            audioProcessor.loadSampleIntoSlot (slotIndex, file);
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
        strips[i].artLabel.setText (audioProcessor.slots[i].articulationName, juce::dontSendNotification);
        strips[i].bottomPadBtn.setButtonText (audioProcessor.slots[i].micLabel);

        float panVal = static_cast<float> (strips[i].panSlider.getValue());
        if (std::abs (panVal) < 0.03f)
            strips[i].panReadout.setText ("<C>", juce::dontSendNotification);
        else if (panVal < 0.0f)
            strips[i].panReadout.setText ("L" + juce::String (static_cast<int> (-panVal * 100.0f)), juce::dontSendNotification);
        else
            strips[i].panReadout.setText ("R" + juce::String (static_cast<int> (panVal * 100.0f)), juce::dontSendNotification);
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

    // MIDI Capture Group Box
    g.setColour (juce::Colour (0xff9ea4b0));
    g.drawRoundedRectangle (12.0f, 546.0f, 124.0f, 82.0f, 4.0f, 1.0f);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("MIDI CAPTURE", 16, 539, 116, 14, juce::Justification::centred);

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

    if (mixerVisible)
    {
        int startX = 148;
        int stripW = 107;
        int gap = 4;

        for (int i = 0; i < AuroraTriggerAudioProcessor::numSlots; ++i)
        {
            int sx = startX + i * (stripW + gap);
            g.setColour (juce::Colour (0xff8a909c));
            g.fillRoundedRectangle (static_cast<float> (sx), 264.0f, static_cast<float> (stripW), 312.0f, 4.0f);
            g.setColour (juce::Colour (0xff2c3038));
            g.drawRoundedRectangle (static_cast<float> (sx), 264.0f, static_cast<float> (stripW), 312.0f, 4.0f, 1.0f);

            g.setColour (juce::Colour (0xff1a1d24));
            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            g.drawText ("PAN",  sx, 268, stripW, 14, juce::Justification::centred);
            g.drawText ("TUNE", sx, 336, stripW, 14, juce::Justification::centred);
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

    linkTuneBtn.setBounds    (lx, 434, lw, 26);
    flipPhasesBtn.setBounds  (lx, 468, lw, 26);
    savePresetBtn.setBounds  (lx, 502, lw, 26);

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

    // Browser & Settings
    browserViewLabel.setBounds  (220, 40, 740, 60);
    loadSampleBtn.setBounds     (290, 120, 200, 34);
    auditionSlotBtn.setBounds   (510, 120, 180, 34);
    resetChilliKitBtn.setBounds (710, 120, 180, 34);
    settingsViewLabel.setBounds (220, 50, 740, 150);

    // 8-Channel Mixer Strips + Bottom Pads
    if (mixerVisible)
    {
        int startX = 148;
        int stripW = 107;
        int gap = 4;

        for (int i = 0; i < AuroraTriggerAudioProcessor::numSlots; ++i)
        {
            int sx = startX + i * (stripW + gap);
            auto& st = strips[i];

            st.panSlider.setBounds   (sx + 35, 282, 36, 34);
            st.panReadout.setBounds  (sx + 24, 317, 58, 16);

            st.tuneSlider.setBounds  (sx + 35, 350, 36, 34);
            st.tuneReadout.setBounds (sx + 24, 385, 58, 16);

            st.volSlider.setBounds   (sx + 10, 408, 44, 144);
            st.soloBtn.setBounds     (sx + 62, 418, 28, 24);
            st.muteBtn.setBounds     (sx + 62, 450, 28, 24);
            st.phaseBtn.setBounds    (sx + 62, 482, 28, 24);

            st.artLabel.setBounds    (sx + 4, 554, stripW - 8, 18);
            st.bottomPadBtn.setBounds(sx, 582, stripW, 48);
        }
    }
}
