#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

// Samples of one slot: velocity layers (soft -> loud), each with its round robins.
// Built off the audio thread, then swapped in under the slot lock; never modified after.
struct SampleSet
{
    std::vector<std::vector<juce::AudioBuffer<float>>> layers;
    double sampleRate = 44100.0;

    int numHits() const
    {
        int n = 0;
        for (const auto& l : layers)
            n += (int) l.size();
        return n;
    }
};

struct SampleSlot
{
    std::unique_ptr<const SampleSet> samples;   // null = empty slot
    juce::String articulationName { "Empty" };
    juce::String micLabel { "Empty" };

    // Where the sound came from, saved with the session: "empty", "default" or "file"
    juce::String sourceType { "empty" };
    juce::File sourceFile;

    // Voice (audio thread only, under the slot lock)
    bool isPlaying = false;
    const juce::AudioBuffer<float>* voiceBuffer = nullptr;
    double playPosition = 0.0;
    float currentVelocity = 1.0f; // gain from the dynamics curve
    juce::int64 envelopeSamples = 0;
    std::vector<int> layerRoundRobin; // next round robin per velocity layer (sized when samples are installed)
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

    //==============================================================================
    // Slots
    struct SlotInfo
    {
        bool loaded = false;
        bool loading = false;
        juce::String micLabel { "Empty" };
        juce::String articulationName { "Empty" };
        juce::String error;      // last load error, if any
        int numLayers = 0;
        int numHits = 0;
    };

    SlotInfo getSlotInfo (int slotIndex) const;

    // .wav / .aif / .aiff / .flac / .tci
    static bool isSupportedFile (const juce::File& file);
    static juce::String supportedWildcard() { return "*.wav;*.aif;*.aiff;*.flac;*.tci"; }

    // Loads on a background thread; the slot keeps playing its old sound until the new one is ready.
    void loadFileIntoSlotAsync (int slotIndex, const juce::File& file);
    bool waitForPendingLoads (int timeoutMs);

    // Clears the slot and resets all of its parameters to their initial values ("Init").
    void initSlot (int slotIndex);

    void triggerAudition (int slotIndex, float velocity = 0.88f);

    //==============================================================================
    // Presets: .atpreset files hold every parameter plus the file loaded in each slot.
    static constexpr const char* presetExtension = ".atpreset";
    static juce::File getPresetsFolder(); // Documents/Aurora Trigger/Presets
    bool savePreset (const juce::File& file, juce::String& error);
    bool loadPreset (const juce::File& file, juce::String& error);
    juce::String getPresetName() const;
    void setPresetName (const juce::String& name);
    void flipAllPhases();
    void loadDefaultChilliSnareKit();

    //==============================================================================
    // Linked parameters (Link Tune + Curve Link): moving one slot's control moves all slots.
    // `suffix` is the per-slot parameter name: tune, att, sus, rel, dyn, vel, rmin, rmax.
    void applyLinkedSlotParam (const juce::String& suffix, int sourceSlot, float newValue);
    void applyLinkedTune (int sourceSlot, float newTuneValue) { applyLinkedSlotParam ("tune", sourceSlot, newTuneValue); }
    static juce::String linkParamIdFor (const juce::String& suffix);

    // Curves: input velocity (0..1) -> output velocity (0..127), and output velocity -> gain.
    // velCurve 50 = linear, above 50 boosts soft hits, below 50 favours hard hits.
    static float mapVelocity (float input01, float velCurve, float rangeMin, float rangeMax);
    static float dynamicsGain (float outputVelocity, float dynamicsPercent);

    //==============================================================================
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

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::ValueTree createStateTree();
    bool applyStateTree (juce::ValueTree tree);
    void updateLatencyFromMode (int modeIndex);
    void triggerAllActiveSlots (float velocity);
    void triggerSlot (int slotIndex, float velocity01);
    void synthesizeDemoSnareMic (int slotIndex);
    void installSamples (int slotIndex, std::unique_ptr<const SampleSet> set, const juce::String& art,
                         const juce::String& mic, const juce::String& type, const juce::File& file);
    void clearSlotSamples (int slotIndex);
    std::unique_ptr<const SampleSet> buildSampleSet (const juce::File& file, juce::String& art, juce::String& error);
    void setParamValue (const juce::String& paramId, float value);

    struct SlotParams
    {
        std::atomic<float>* vol = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* tune = nullptr;
        std::atomic<float>* solo = nullptr;
        std::atomic<float>* mute = nullptr;
        std::atomic<float>* phase = nullptr;
        std::atomic<float>* att = nullptr;
        std::atomic<float>* sus = nullptr;
        std::atomic<float>* rel = nullptr;
        std::atomic<float>* dyn = nullptr;
        std::atomic<float>* vel = nullptr;
        std::atomic<float>* rmin = nullptr;
        std::atomic<float>* rmax = nullptr;
    };
    std::array<SlotParams, numSlots> slotParams;

    juce::AudioFormatManager formatManager;
    juce::CriticalSection slotLock;
    std::array<SampleSlot, numSlots> slots;

    // Background loading
    juce::ThreadPool loaderPool { 1 };
    std::array<std::atomic<int>, numSlots> loadGeneration {};
    std::array<std::atomic<int>, numSlots> pendingLoads {};
    std::array<juce::String, numSlots> slotErrors; // guarded by slotLock

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

    juce::String presetName { "Chilli Snare" };
    juce::CriticalSection presetNameLock;

    int lastLatencyMode = -1;
    bool isSyncingLinkedParam = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuroraTriggerAudioProcessor)
};
