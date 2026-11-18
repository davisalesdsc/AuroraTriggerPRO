#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>

struct SampleSlot
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    juce::String articulationName { "Empty" };
    juce::String micLabel { "Empty" };
    bool loaded = false;

    bool isPlaying = false;
    double playPosition = 0.0;
    float currentVelocity = 1.0f;
};

class AuroraTriggerAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numSlots = 8;
    static constexpr int scopeSize = 512;

    AuroraTriggerAudioProcessor();
    ~AuroraTriggerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Sample & Kit Management
    bool loadSampleIntoSlot (int slotIndex, const juce::File& file);
    void clearSlot (int slotIndex);
    void triggerAudition (int slotIndex, float velocity = 0.88f);
    void flipAllPhases();
    void loadDefaultChilliSnareKit();

    // Link Tune Helper
    void applyLinkedTune (int sourceSlot, float newTuneValue);

    // MIDI Capture & Drag-to-Track
    void setMidiRecording (bool shouldRecord);
    bool isMidiRecording() const { return midiRecording.load(); }
    void clearMidiCapture();
    bool exportCapturedMidiToFile (const juce::File& targetFile);
    int getCapturedNoteCount() const { return capturedNoteCount.load(); }

    // UI Visualization & Metering
    std::array<float, scopeSize> scopeData {};
    std::array<int, scopeSize> scopeMarkers {}; // 0 = none, 1 = hit, 2 = suppressed bleed
    std::atomic<int> scopeWritePos { 0 };
    std::atomic<float> inputLevelDb { -60.0f };
    std::atomic<float> outputLevelDb { -60.0f };
    std::atomic<bool> gateOpenState { false };
    juce::String activePresetName { "Chilli Snare" };

    juce::AudioProcessorValueTreeState apvts;
    std::array<SampleSlot, numSlots> slots;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateLatencyFromMode (int modeIndex);
    void triggerAllActiveSlots (float velocity);
    void synthesizeDemoSnareMic (int slotIndex, const juce::String& artName, const juce::String& micName, float pitchHz, float noiseAmt, float roomDecay);

    juce::AudioFormatManager formatManager;
    juce::CriticalSection slotLock;

    juce::dsp::IIR::Filter<float> lowcutFilter;
    juce::dsp::IIR::Filter<float> highcutFilter;
    juce::dsp::IIR::Filter<float> bleedHighFilter;

    double currentSampleRate = 44100.0;
    float fastEnvelope = 0.0f;
    float slowEnvelope = 0.0f;
    float highFreqEnvelope = 0.0f;
    int retriggerCounter = 0;

    float gateGain = 0.0f;
    int gateHoldCounter = 0;

    int midiNoteOffCounter = 0;
    bool midiNoteActive = false;
    std::atomic<bool> midiRecording { true };
    std::atomic<int> capturedNoteCount { 0 };
    double totalSamplesElapsed = 0.0;
    juce::MidiMessageSequence capturedSequence;
    juce::CriticalSection midiLock;

    int lastLatencyMode = -1;
    bool isSyncingLinkedTune = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuroraTriggerAudioProcessor)
};
