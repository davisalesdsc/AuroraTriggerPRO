#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class AuroraMetallicLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AuroraMetallicLookAndFeel();
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override;
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override;
};

class LedMeterComponent : public juce::Component
{
public:
    LedMeterComponent (std::atomic<float>& levelSource) : levelDb (levelSource) {}
    void paint (juce::Graphics& g) override;

private:
    std::atomic<float>& levelDb;
};

class WaveformScopeComponent : public juce::Component
{
public:
    WaveformScopeComponent (AuroraTriggerAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    void updateDetailFromMouseY (int y);
    AuroraTriggerAudioProcessor& processor;
};

class AuroraTriggerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          public juce::DragAndDropContainer,
                                          private juce::Timer
{
public:
    AuroraTriggerAudioProcessorEditor (AuroraTriggerAudioProcessor&);
    ~AuroraTriggerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setActiveTab (int tabIndex);
    void loadSampleViaChooser (int slotIndex);
    void dragCapturedMidiToDaw();

    AuroraTriggerAudioProcessor& audioProcessor;
    AuroraMetallicLookAndFeel metallicLnf;

    int activeViewTab = 1; // 0 = Browser, 1 = Triggering, 2 = Settings
    bool mixerVisible = true;
    int selectedSlot = 0;

    // Left Navigation Column (Exact Layout Order)
    juce::TextButton browserBtn     { "Browser" };
    juce::TextButton triggeringBtn  { "Triggering" };
    juce::TextButton settingsBtn    { "Settings" };

    juce::TextButton viewMixerBtn   { "View Mixer" };
    juce::TextButton viewCurvesBtn  { "View Curves" };

    juce::ToggleButton linkTuneBtn  { "LINK TUNE" };
    juce::TextButton flipPhasesBtn  { "FLIP ALL PHASES" };
    juce::TextButton savePresetBtn  { "SAVE PRESET" };

    juce::TextButton dragOnTrackBtn { "DRAG ON TRACK" };
    juce::TextButton clearBufferBtn { "CLEAR BUFFER" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkTuneAtt;

    // Top Triggering Controls
    juce::ToggleButton gateOnBtn { "On" };
    juce::Slider gateAttSlider, gateHoldSlider, gateRelSlider;
    juce::Slider inVolSlider;
    LedMeterComponent inputMeter;

    juce::Slider lowcutSlider, highcutSlider, suppressSlider;
    WaveformScopeComponent waveformScope;

    juce::Slider sensSlider, retrigSlider, detailSlider;
    LedMeterComponent outputMeter;
    juce::Slider outVolSlider, mixSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gateOnAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttAtt, gateHoldAtt, gateRelAtt, inVolAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowcutAtt, highcutAtt, suppressAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensAtt, retrigAtt, detailAtt, outVolAtt, mixAtt;

    // Browser & Settings Views
    juce::TextButton loadSampleBtn { "Load .WAV / .AIFF into Slot" };
    juce::TextButton auditionSlotBtn { "Audition Selected Slot" };
    juce::TextButton resetChilliKitBtn { "Reload Chilli Snare Kit" };
    juce::Label browserViewLabel;
    juce::Label settingsViewLabel;

    // 8-Channel Mixer Strips with Bottom Trigger Pads
    struct MixerStripUI
    {
        juce::Slider panSlider;
        juce::Label  panReadout;
        juce::Slider tuneSlider;
        juce::Label  tuneReadout;
        juce::Slider volSlider;
        juce::TextButton soloBtn  { "S" };
        juce::TextButton muteBtn  { "M" };
        juce::TextButton phaseBtn { juce::String (juce::CharPointer_UTF8 ("\xc3\x98")) }; // O with stroke (phase)
        juce::Label  artLabel;
        juce::TextButton bottomPadBtn { "Empty" };

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAtt, tuneAtt, volAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAtt, muteAtt, phaseAtt;
    };
    std::array<MixerStripUI, AuroraTriggerAudioProcessor::numSlots> strips;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuroraTriggerAudioProcessorEditor)
};
