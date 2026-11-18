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
    juce::Label* createSliderTextBox (juce::Slider& slider) override;
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

// Curves view: amplitude envelope of one slot (Attack / Sustain / Release)
class EnvelopeDisplay : public juce::Component
{
public:
    EnvelopeDisplay (AuroraTriggerAudioProcessor& p, int slot) : processor (p), slotIndex (slot) {}
    void paint (juce::Graphics& g) override;

private:
    AuroraTriggerAudioProcessor& processor;
    int slotIndex;
};

// Curves view: input velocity -> output velocity (orange) and resulting level (blue)
class VelocityCurveDisplay : public juce::Component
{
public:
    VelocityCurveDisplay (AuroraTriggerAudioProcessor& p, int slot) : processor (p), slotIndex (slot) {}
    void paint (juce::Graphics& g) override;

private:
    AuroraTriggerAudioProcessor& processor;
    int slotIndex;
};

// Bottom trigger pad: left click = audition / load, right click = menu,
// accepts files dragged from the browser or from Finder / Explorer.
class PadButton : public juce::TextButton,
                  public juce::DragAndDropTarget,
                  public juce::FileDragAndDropTarget
{
public:
    static constexpr const char* browserDragId = "AuroraBrowserFile";

    std::function<void()> onRightClick;
    std::function<void (const juce::File&)> onFileDropped;
    std::function<juce::File()> getBrowserSelection;

    void mouseDown (const juce::MouseEvent& e) override;
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDragEnter (const SourceDetails&) override { setDropHover (true); }
    void itemDragExit (const SourceDetails&) override { setDropHover (false); }
    void itemDropped (const SourceDetails& details) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { setDropHover (true); }
    void fileDragExit (const juce::StringArray&) override { setDropHover (false); }
    void filesDropped (const juce::StringArray& files, int, int) override;

    bool slotLoaded = false;

private:
    void setDropHover (bool h) { dropHover = h; repaint(); }
    bool dropHover = false;
};

class AuroraTriggerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          public juce::DragAndDropContainer,
                                          private juce::FileBrowserListener,
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
    void setLowerView (int view); // 0 = mixer, 1 = curves
    void selectSlot (int slotIndex);
    void loadSampleViaChooser (int slotIndex);
    void loadFileIntoSlot (int slotIndex, const juce::File& file);
    void showPadMenu (int slotIndex);
    void dragCapturedMidiToDaw();

    // Presets
    void savePresetDialog();
    void showPresetMenu();
    void loadPresetFile (const juce::File& file);
    void showError (const juce::String& title, const juce::String& message);

    // Browser
    void setBrowserRoot (const juce::File& folder);
    void chooseBrowserFolder();
    void selectionChanged() override {}
    void fileClicked (const juce::File&, const juce::MouseEvent&) override {}
    void fileDoubleClicked (const juce::File& file) override;
    void browserRootChanged (const juce::File&) override {}

    AuroraTriggerAudioProcessor& audioProcessor;
    AuroraMetallicLookAndFeel metallicLnf;

    int activeViewTab = 1; // 0 = Browser, 1 = Triggering, 2 = Settings
    int lowerView = 0;     // 0 = mixer, 1 = curves
    int selectedSlot = 0;

    // Left Navigation Column
    juce::TextButton browserBtn     { "Browser" };
    juce::TextButton triggeringBtn  { "Triggering" };
    juce::TextButton settingsBtn    { "Settings" };

    juce::TextButton viewMixerBtn   { "View Mixer" };
    juce::TextButton viewCurvesBtn  { "View Curves" };

    juce::ToggleButton linkTuneBtn  { "LINK TUNE" };
    juce::TextButton flipPhasesBtn  { "FLIP ALL PHASES" };
    juce::TextButton loadPresetBtn  { "LOAD PRESET" };
    juce::TextButton savePresetBtn  { "SAVE PRESET" };

    juce::TextButton dragOnTrackBtn { "DRAG ON TRACK" };
    juce::TextButton clearBufferBtn { "CLEAR BUFFER" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkTuneAtt;

    // Curve Link (shown in the curves view)
    struct CurveLinkRow
    {
        juce::String label;
        juce::TextButton button { "Off" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
    std::array<CurveLinkRow, 7> curveLinks;

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

    // Browser view: a folder tree that stays on screen
    juce::TimeSliceThread browserThread { "Aurora Trigger browser" };
    juce::WildcardFileFilter browserFilter { AuroraTriggerAudioProcessor::supportedWildcard() + ";*.atpreset", "*", "Drum samples, TCI and presets" };
    juce::DirectoryContentsList browserList { &browserFilter, browserThread };
    juce::FileTreeComponent browserTree { browserList };
    juce::TextButton chooseFolderBtn { "Choose Folder..." };
    juce::TextButton parentFolderBtn { "Up" };
    juce::Label browserPathLabel;
    juce::Label browserTargetLabel;
    juce::Label browserInfoLabel;
    juce::TextButton loadSelectedBtn { "Load Selected Into Slot" };
    juce::TextButton loadSampleBtn { "Load File..." };
    juce::TextButton auditionSlotBtn { "Audition Selected Slot" };
    juce::TextButton resetChilliKitBtn { "Reload Chilli Snare Kit" };
    std::unique_ptr<juce::PropertiesFile> userSettings;

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
        PadButton bottomPadBtn;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAtt, tuneAtt, volAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAtt, muteAtt, phaseAtt;
    };
    std::array<MixerStripUI, AuroraTriggerAudioProcessor::numSlots> strips;

    // Curves view strips (Trigger 2 style)
    struct CurveStripUI
    {
        CurveStripUI (AuroraTriggerAudioProcessor& p, int slot) : envelope (p, slot), velocityCurve (p, slot) {}

        EnvelopeDisplay envelope;
        VelocityCurveDisplay velocityCurve;
        juce::Slider attSlider, susSlider, relSlider, dynSlider, velSlider, rminSlider, rmaxSlider;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    };
    std::array<std::unique_ptr<CurveStripUI>, AuroraTriggerAudioProcessor::numSlots> curveStrips;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuroraTriggerAudioProcessorEditor)
};
