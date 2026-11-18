#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TciDecoder.h"
#include <cmath>

namespace
{
juce::String slotPrefix (int i) { return "slot" + juce::String (i) + "_"; }

// Initial ("Init") values of every per-slot parameter.
struct SlotDefault
{
    const char* suffix;
    float value;
};

constexpr SlotDefault slotInitValues[] = {
    { "vol", 0.0f }, { "pan", 0.0f }, { "tune", 0.0f },
    { "solo", 0.0f }, { "mute", 0.0f }, { "phase", 0.0f },
    { "att", 0.0f }, { "sus", 5.0f }, { "rel", 10.0f },
    { "dyn", 100.0f }, { "vel", 50.0f }, { "rmin", 0.0f }, { "rmax", 127.0f },
};

struct DemoMic
{
    const char* mic;
    float pitchHz, noiseAmt, roomDecay;
};

constexpr DemoMic demoKit[] = {
    { "ChiliSnare Z1 mic",   192.0f, 0.45f, 18.0f },
    { "ChiliSnare SSDR mic", 185.0f, 0.72f, 11.0f },
    { "ChiliSnare NRG mic",  178.0f, 0.88f, 6.5f },
};
constexpr int numDemoMics = (int) (sizeof (demoKit) / sizeof (demoKit[0]));

// Value text shown in the curve controls ("0 ms", "5.00 s", "100")
juce::AudioParameterFloatAttributes msAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return (v < 10.0f && v > 0.0f ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + " ms"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });
}

juce::AudioParameterFloatAttributes secondsAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " s"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });
}

juce::AudioParameterFloatAttributes integerAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)); })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });
}
} // namespace

AuroraTriggerAudioProcessor::AuroraTriggerAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();
    scopeData.fill (0.0f);
    scopeMarkers.fill (0);

    for (int i = 0; i < numSlots; ++i)
    {
        const auto p = slotPrefix (i);
        auto& sp = slotParams[(size_t) i];
        sp.vol   = apvts.getRawParameterValue (p + "vol");
        sp.pan   = apvts.getRawParameterValue (p + "pan");
        sp.tune  = apvts.getRawParameterValue (p + "tune");
        sp.solo  = apvts.getRawParameterValue (p + "solo");
        sp.mute  = apvts.getRawParameterValue (p + "mute");
        sp.phase = apvts.getRawParameterValue (p + "phase");
        sp.att   = apvts.getRawParameterValue (p + "att");
        sp.sus   = apvts.getRawParameterValue (p + "sus");
        sp.rel   = apvts.getRawParameterValue (p + "rel");
        sp.dyn   = apvts.getRawParameterValue (p + "dyn");
        sp.vel   = apvts.getRawParameterValue (p + "vel");
        sp.rmin  = apvts.getRawParameterValue (p + "rmin");
        sp.rmax  = apvts.getRawParameterValue (p + "rmax");
        loadGeneration[(size_t) i] = 0;
        pendingLoads[(size_t) i] = 0;
    }

    loadDefaultChilliSnareKit();
}

AuroraTriggerAudioProcessor::~AuroraTriggerAudioProcessor()
{
    // Loader jobs touch members declared after the pool: finish them first.
    loaderPool.removeAllJobs (true, 30000);
}

juce::String AuroraTriggerAudioProcessor::linkParamIdFor (const juce::String& suffix)
{
    if (suffix == "tune") return "linkTune";
    if (suffix == "att")  return "curveLinkAttack";
    if (suffix == "sus")  return "curveLinkSustain";
    if (suffix == "rel")  return "curveLinkRelease";
    if (suffix == "dyn")  return "curveLinkDynamics";
    if (suffix == "vel")  return "curveLinkVelocity";
    if (suffix == "rmin") return "curveLinkRangeMin";
    if (suffix == "rmax") return "curveLinkRangeMax";
    return {};
}

juce::AudioProcessorValueTreeState::ParameterLayout AuroraTriggerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Gate & Input
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("gateOn", "Gate On", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("gateAttack", "Attack", 0.1f, 20.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("gateHold", "Hold", 1.0f, 200.0f, 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("gateRelease", "Release", 10.0f, 1000.0f, 450.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("inputVol", "Input Volume", -24.0f, 24.0f, 0.0f));

    // Sidechain Filters & Suppress
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lowcut", "Lowcut",
        juce::NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.4f), 20.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("highcut", "Highcut",
        juce::NormalisableRange<float> (500.0f, 20000.0f, 1.0f, 0.4f), 20000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("suppress", "Suppress", 0.0f, 100.0f, 0.0f));

    // Detector & Output
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sensitivity", "Sensitivity", 0.0f, 100.0f, 51.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("retrigger", "Retrigger", 5.0f, 150.0f, 16.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("detail", "Detail", 1.0f, 100.0f, 14.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("outputVol", "Output Volume", -24.0f, 12.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix", 0.0f, 100.0f, 100.0f));

    // Global Utilities & Latency
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("linkTune", "Link Tune", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("latencyMode", "Latency Mode",
        juce::StringArray { "LIVE (4ms)", "LOW (12ms)", "NORMAL (23.2ms)" }, 2));
    params.push_back (std::make_unique<juce::AudioParameterInt>   ("midiNote", "MIDI Note", 24, 96, 38));

    // Curve Link: changing a curve control on one slot changes it on every slot
    params.push_back (std::make_unique<juce::AudioParameterBool> ("curveLinkAttack",   "Curve Link Attack", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("curveLinkSustain",  "Curve Link Sustain", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("curveLinkRelease",  "Curve Link Release", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("curveLinkDynamics", "Curve Link Dynamics", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("curveLinkVelocity", "Curve Link Velocity", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("curveLinkRangeMin", "Curve Link Range Min", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("curveLinkRangeMax", "Curve Link Range Max", false));

    // 8 slots: mixer strip + curves (Default Chilli Snare levels: 0.00 dB, -9.12 dB, -9.12 dB)
    for (int i = 0; i < numSlots; ++i)
    {
        const auto prefix = slotPrefix (i);
        const auto name = "Slot " + juce::String (i + 1) + " ";
        const float defaultVol = (i == 1 || i == 2) ? -9.12f : 0.0f;
        params.push_back (std::make_unique<juce::AudioParameterFloat> (prefix + "vol", name + "Vol", -60.0f, 12.0f, defaultVol));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (prefix + "pan", name + "Pan", -1.0f, 1.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (prefix + "tune", name + "Tune", -12.0f, 12.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterBool>  (prefix + "solo", name + "Solo", false));
        params.push_back (std::make_unique<juce::AudioParameterBool>  (prefix + "mute", name + "Mute", false));
        params.push_back (std::make_unique<juce::AudioParameterBool>  (prefix + "phase", name + "Phase", false));

        // Curves: amplitude envelope (Attack ms, Sustain s, Release s), dynamics, velocity curve, output range
        params.push_back (std::make_unique<juce::AudioParameterFloat> (prefix + "att", name + "Attack",
            juce::NormalisableRange<float> (0.0f, 500.0f, 0.1f, 0.4f), 0.0f, msAttributes()));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (prefix + "sus", name + "Sustain",
            juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f, 0.6f), 5.0f, secondsAttributes()));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (prefix + "rel", name + "Release",
            juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f, 0.6f), 10.0f, secondsAttributes()));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (prefix + "dyn", name + "Dynamics",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, integerAttributes()));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (prefix + "vel", name + "Velocity",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f, integerAttributes()));
        params.push_back (std::make_unique<juce::AudioParameterInt> (prefix + "rmin", name + "Range Min", 0, 127, 0));
        params.push_back (std::make_unique<juce::AudioParameterInt> (prefix + "rmax", name + "Range Max", 0, 127, 127));
    }

    return { params.begin(), params.end() };
}

//==============================================================================
// Curves
float AuroraTriggerAudioProcessor::mapVelocity (float input01, float velCurve, float rangeMin, float rangeMax)
{
    const float in = juce::jlimit (0.0f, 1.0f, input01);
    const float exponent = std::pow (4.0f, (50.0f - velCurve) / 50.0f); // 0.25 .. 1 .. 4
    const float shaped = std::pow (in, exponent);
    return juce::jlimit (0.0f, 127.0f, rangeMin + (rangeMax - rangeMin) * shaped);
}

float AuroraTriggerAudioProcessor::dynamicsGain (float outputVelocity, float dynamicsPercent)
{
    const float d = juce::jlimit (0.0f, 1.0f, dynamicsPercent / 100.0f);
    return (1.0f - d) + d * juce::jlimit (0.0f, 1.0f, outputVelocity / 127.0f);
}

//==============================================================================
// Slot contents
void AuroraTriggerAudioProcessor::installSamples (int slotIndex, std::unique_ptr<const SampleSet> set, const juce::String& art,
                                                  const juce::String& mic, const juce::String& type, const juce::File& file)
{
    std::unique_ptr<const SampleSet> old;
    {
        const juce::ScopedLock sl (slotLock);
        auto& slot = slots[(size_t) slotIndex];
        old = std::move (slot.samples);
        slot.layerRoundRobin.assign (set != nullptr ? set->layers.size() : 0, 0);
        slot.samples = std::move (set);
        slot.isPlaying = false;
        slot.voiceBuffer = nullptr;
        slot.playPosition = 0.0;
        slot.articulationName = art;
        slot.micLabel = mic;
        slot.sourceType = type;
        slot.sourceFile = file;
        slotErrors[(size_t) slotIndex].clear();
    }
    // `old` is released here, outside the lock and off the audio thread.
}

void AuroraTriggerAudioProcessor::clearSlotSamples (int slotIndex)
{
    installSamples (slotIndex, nullptr, "Empty", "Empty", "empty", {});
}

void AuroraTriggerAudioProcessor::synthesizeDemoSnareMic (int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= numDemoMics)
        return;

    const auto& m = demoKit[slotIndex];
    const double sr = 44100.0;
    const int lengthSamples = static_cast<int> (sr * 0.85);
    juce::AudioBuffer<float> buf (2, lengthSamples);
    juce::Random rng (slotIndex * 137 + 42);

    for (int i = 0; i < lengthSamples; ++i)
    {
        float t = static_cast<float> (i) / static_cast<float> (sr);
        float bodyEnv = std::exp (-t * 22.0f);
        float wireEnv = std::exp (-t * m.roomDecay);

        float pitchDrop = m.pitchHz * (1.0f + 0.8f * std::exp (-t * 95.0f));
        float body = std::sin (juce::MathConstants<float>::twoPi * pitchDrop * t) * bodyEnv;
        float noise = (rng.nextFloat() * 2.0f - 1.0f) * wireEnv * m.noiseAmt;

        float sampleL = (body * 0.65f + noise * 0.55f) * 0.85f;
        float sampleR = (body * 0.65f + ((rng.nextFloat() * 2.0f - 1.0f) * wireEnv * m.noiseAmt) * 0.55f) * 0.85f;

        buf.setSample (0, i, sampleL);
        buf.setSample (1, i, sampleR);
    }

    auto set = std::make_unique<SampleSet>();
    set->sampleRate = sr;
    set->layers.resize (1);
    set->layers[0].push_back (std::move (buf));
    installSamples (slotIndex, std::move (set), "Snare Full", m.mic, "default", {});
}

void AuroraTriggerAudioProcessor::loadDefaultChilliSnareKit()
{
    activePresetName = "Chilli Snare";
    for (int i = 0; i < numDemoMics; ++i)
    {
        ++loadGeneration[(size_t) i]; // a pending file load must not overwrite the kit
        synthesizeDemoSnareMic (i);
    }
}

bool AuroraTriggerAudioProcessor::isSupportedFile (const juce::File& file)
{
    return file.hasFileExtension ("wav;aif;aiff;flac;tci");
}

std::unique_ptr<const SampleSet> AuroraTriggerAudioProcessor::buildSampleSet (const juce::File& file, juce::String& art, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "File not found: " + file.getFullPathName();
        return nullptr;
    }

    auto set = std::make_unique<SampleSet>();

    if (tci::hasTciExtension (file))
    {
        auto inst = tci::decodeFile (file, error);
        if (! inst)
            return nullptr;

        set->sampleRate = inst->sampleRate;
        for (const auto& layer : inst->layers)
        {
            std::vector<juce::AudioBuffer<float>> rr;
            for (const int w : layer)
                rr.push_back (std::move (inst->waves[(size_t) w].audio));
            set->layers.push_back (std::move (rr));
        }

        art = "TCI " + juce::String ((int) set->layers.size()) + "v/" + juce::String (set->numHits()) + "h";
        if (inst->stereoWaves > 0)
            error = "Stereo TCI: the first ~2 ms of each stereo hit cannot be decoded yet and play silent.";
        if (inst->skippedWaves > 0)
            error = juce::String (error.isEmpty() ? "" : error + " ") + juce::String (inst->skippedWaves) + " hit(s) could not be decoded.";
        return set;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Unsupported or damaged audio file: " + file.getFileName();
        return nullptr;
    }
    if (reader->lengthInSamples <= 0 || reader->lengthInSamples > (juce::int64) reader->sampleRate * 60)
    {
        error = "Audio file is empty or longer than 60 s: " + file.getFileName();
        return nullptr;
    }

    juce::AudioBuffer<float> buf (static_cast<int> (juce::jlimit (1u, 2u, reader->numChannels)),
                                  static_cast<int> (reader->lengthInSamples));
    reader->read (&buf, 0, buf.getNumSamples(), 0, true, buf.getNumChannels() > 1);
    set->sampleRate = reader->sampleRate;
    set->layers.resize (1);
    set->layers[0].push_back (std::move (buf));
    art = "Sample";
    return set;
}

void AuroraTriggerAudioProcessor::loadFileIntoSlotAsync (int slotIndex, const juce::File& file)
{
    if (slotIndex < 0 || slotIndex >= numSlots)
        return;

    const int generation = ++loadGeneration[(size_t) slotIndex];
    ++pendingLoads[(size_t) slotIndex];

    loaderPool.addJob ([this, slotIndex, file, generation]
    {
        juce::String art, message;
        auto set = buildSampleSet (file, art, message);

        if (loadGeneration[(size_t) slotIndex].load() == generation)
        {
            if (set != nullptr)
            {
                installSamples (slotIndex, std::move (set), art, file.getFileNameWithoutExtension(), "file", file);
                const juce::ScopedLock sl (slotLock);
                slotErrors[(size_t) slotIndex] = message; // warnings for a usable file
            }
            else
            {
                const juce::ScopedLock sl (slotLock);
                slotErrors[(size_t) slotIndex] = message;
            }
        }
        --pendingLoads[(size_t) slotIndex];
    });
}

bool AuroraTriggerAudioProcessor::waitForPendingLoads (int timeoutMs)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (juce::Time::getMillisecondCounter() < end)
    {
        int pending = 0;
        for (const auto& p : pendingLoads)
            pending += p.load();
        if (pending == 0)
            return true;
        juce::Thread::sleep (5);
    }
    return false;
}

AuroraTriggerAudioProcessor::SlotInfo AuroraTriggerAudioProcessor::getSlotInfo (int slotIndex) const
{
    SlotInfo info;
    if (slotIndex < 0 || slotIndex >= numSlots)
        return info;

    info.loading = pendingLoads[(size_t) slotIndex].load() > 0;
    const juce::ScopedLock sl (slotLock);
    const auto& slot = slots[(size_t) slotIndex];
    info.loaded = slot.samples != nullptr;
    info.micLabel = slot.micLabel;
    info.articulationName = slot.articulationName;
    info.error = slotErrors[(size_t) slotIndex];
    if (slot.samples != nullptr)
    {
        info.numLayers = (int) slot.samples->layers.size();
        info.numHits = slot.samples->numHits();
    }
    return info;
}

void AuroraTriggerAudioProcessor::setParamValue (const juce::String& paramId, float value)
{
    if (auto* param = apvts.getParameter (paramId))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void AuroraTriggerAudioProcessor::initSlot (int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= numSlots)
        return;

    ++loadGeneration[(size_t) slotIndex]; // drop any load still in flight for this slot
    clearSlotSamples (slotIndex);

    // Reset this slot only, even when Link Tune / Curve Link are on.
    const bool wasSyncing = isSyncingLinkedParam;
    isSyncingLinkedParam = true;
    for (const auto& d : slotInitValues)
        setParamValue (slotPrefix (slotIndex) + d.suffix, d.value);
    isSyncingLinkedParam = wasSyncing;
}

void AuroraTriggerAudioProcessor::applyLinkedSlotParam (const juce::String& suffix, int sourceSlot, float newValue)
{
    if (isSyncingLinkedParam)
        return;

    auto* link = apvts.getRawParameterValue (linkParamIdFor (suffix));
    if (link == nullptr || link->load() < 0.5f)
        return;

    isSyncingLinkedParam = true;
    for (int i = 0; i < numSlots; ++i)
        if (i != sourceSlot)
            setParamValue (slotPrefix (i) + suffix, newValue);
    isSyncingLinkedParam = false;
}

void AuroraTriggerAudioProcessor::flipAllPhases()
{
    for (int i = 0; i < numSlots; ++i)
        if (auto* param = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (slotPrefix (i) + "phase")))
            param->setValueNotifyingHost (! param->get());
}

//==============================================================================
// Triggering (slot lock held)
void AuroraTriggerAudioProcessor::triggerSlot (int slotIndex, float velocity01)
{
    auto& slot = slots[(size_t) slotIndex];
    if (slot.samples == nullptr || slot.samples->layers.empty())
        return;

    const auto& sp = slotParams[(size_t) slotIndex];
    const float outVel = mapVelocity (velocity01, sp.vel->load(), sp.rmin->load(), sp.rmax->load());

    // Velocity layer from the curve output, then the next round robin of that layer
    const auto& layers = slot.samples->layers;
    const int numLayers = (int) layers.size();
    const int layer = juce::jlimit (0, numLayers - 1, (int) std::floor (outVel / 127.0f * (float) numLayers));
    const auto& rr = layers[(size_t) layer];
    if (rr.empty())
        return;

    auto& next = slot.layerRoundRobin[(size_t) layer];
    slot.voiceBuffer = &rr[(size_t) (next % (int) rr.size())];
    next = (next + 1) % (int) rr.size();
    slot.isPlaying = true;
    slot.playPosition = 0.0;
    slot.envelopeSamples = 0;
    slot.currentVelocity = dynamicsGain (outVel, sp.dyn->load());
}

void AuroraTriggerAudioProcessor::triggerAudition (int slotIndex, float velocity)
{
    if (slotIndex < 0 || slotIndex >= numSlots)
        return;
    const juce::ScopedLock sl (slotLock);
    triggerSlot (slotIndex, velocity);
}

void AuroraTriggerAudioProcessor::triggerAllActiveSlots (float velocity)
{
    for (int i = 0; i < numSlots; ++i)
        triggerSlot (i, velocity);
}

//==============================================================================
const juce::String AuroraTriggerAudioProcessor::getName() const { return JucePlugin_Name; }
bool AuroraTriggerAudioProcessor::acceptsMidi() const { return true; }
bool AuroraTriggerAudioProcessor::producesMidi() const { return true; }
bool AuroraTriggerAudioProcessor::isMidiEffect() const { return false; }
double AuroraTriggerAudioProcessor::getTailLengthSeconds() const { return 2.0; }
int AuroraTriggerAudioProcessor::getNumPrograms() { return 1; }
int AuroraTriggerAudioProcessor::getCurrentProgram() { return 0; }
void AuroraTriggerAudioProcessor::setCurrentProgram (int) {}
const juce::String AuroraTriggerAudioProcessor::getProgramName (int) { return {}; }
void AuroraTriggerAudioProcessor::changeProgramName (int, const juce::String&) {}

void AuroraTriggerAudioProcessor::updateLatencyFromMode (int modeIndex)
{
    if (modeIndex == lastLatencyMode)
        return;

    lastLatencyMode = modeIndex;
    int samplesDelay = 1023;
    if (modeIndex == 0)      samplesDelay = static_cast<int> (currentSampleRate * 0.004);
    else if (modeIndex == 1) samplesDelay = static_cast<int> (currentSampleRate * 0.012);
    else                     samplesDelay = static_cast<int> (currentSampleRate * 0.0232);

    setLatencySamples (samplesDelay);
}

void AuroraTriggerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    lowcutFilter.reset();
    highcutFilter.reset();
    bleedHighFilter.reset();

    *bleedHighFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 6500.0f);

    fastEnvelope = 0.0f;
    slowEnvelope = 0.0f;
    highFreqEnvelope = 0.0f;
    retriggerCounter = 0;
    gateGain = 0.0f;
    gateHoldCounter = 0;
    lastLatencyMode = -1;
    updateLatencyFromMode (static_cast<int> (apvts.getRawParameterValue ("latencyMode")->load()));
}

void AuroraTriggerAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AuroraTriggerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}
#endif

void AuroraTriggerAudioProcessor::setMidiRecording (bool shouldRecord)
{
    midiRecording.store (shouldRecord);
}

void AuroraTriggerAudioProcessor::clearMidiCapture()
{
    const juce::ScopedLock ml (midiLock);
    capturedSequence.clear();
    capturedNoteCount.store (0);
    totalSamplesElapsed = 0.0;
}

bool AuroraTriggerAudioProcessor::exportCapturedMidiToFile (const juce::File& targetFile)
{
    const juce::ScopedLock ml (midiLock);
    if (capturedSequence.getNumEvents() == 0)
        return false;

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (960);

    juce::MidiMessageSequence exportSeq (capturedSequence);
    exportSeq.updateMatchedPairs();
    midiFile.addTrack (exportSeq);

    juce::FileOutputStream stream (targetFile);
    if (stream.openedOk())
    {
        midiFile.writeTo (stream);
        return true;
    }
    return false;
}

void AuroraTriggerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    updateLatencyFromMode (static_cast<int> (apvts.getRawParameterValue ("latencyMode")->load()));

    const bool  gateEnabled  = apvts.getRawParameterValue ("gateOn")->load() > 0.5f;
    const float gateAttMs    = apvts.getRawParameterValue ("gateAttack")->load();
    const float gateHoldMs   = apvts.getRawParameterValue ("gateHold")->load();
    const float gateRelMs    = apvts.getRawParameterValue ("gateRelease")->load();
    const float inputGainLin = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("inputVol")->load());

    const float lowcutFreq   = apvts.getRawParameterValue ("lowcut")->load();
    const float highcutFreq  = apvts.getRawParameterValue ("highcut")->load();
    const float suppressPct  = apvts.getRawParameterValue ("suppress")->load() / 100.0f;

    const float sensitivity  = apvts.getRawParameterValue ("sensitivity")->load() / 100.0f;
    const float retriggerMs  = apvts.getRawParameterValue ("retrigger")->load();
    const float detailNorm   = apvts.getRawParameterValue ("detail")->load() / 100.0f;

    const float outputGain   = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outputVol")->load());
    const float wetMix       = apvts.getRawParameterValue ("mix")->load() / 100.0f;
    const float dryMix       = 1.0f - wetMix;
    const int   midiNoteNum  = static_cast<int> (apvts.getRawParameterValue ("midiNote")->load());

    *lowcutFilter.coefficients  = *juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, lowcutFreq);
    *highcutFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass  (currentSampleRate, highcutFreq);

    const juce::ScopedLock sl (slotLock);

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            triggerAllActiveSlots (msg.getFloatVelocity());
    }

    // Per-slot settings for this block (no string lookups in the sample loop)
    struct SlotBlock
    {
        bool audible;
        float gain, panL, panR, phase;
        double tuneRatio;
        double attS, susS, relS;
    };
    std::array<SlotBlock, numSlots> sb {};

    bool anySlotSoloed = false;
    for (int s = 0; s < numSlots; ++s)
        anySlotSoloed = anySlotSoloed || slotParams[(size_t) s].solo->load() > 0.5f;

    for (int s = 0; s < numSlots; ++s)
    {
        const auto& sp = slotParams[(size_t) s];
        auto& b = sb[(size_t) s];
        const bool muted = sp.mute->load() > 0.5f;
        const bool solo = sp.solo->load() > 0.5f;
        b.audible = ! (muted || (anySlotSoloed && ! solo));
        b.gain = juce::Decibels::decibelsToGain (sp.vol->load());
        const float pan = sp.pan->load();
        b.panL = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
        b.panR = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
        b.phase = sp.phase->load() > 0.5f ? -1.0f : 1.0f;
        const double setRate = slots[(size_t) s].samples != nullptr ? slots[(size_t) s].samples->sampleRate : currentSampleRate;
        b.tuneRatio = std::pow (2.0, sp.tune->load() / 12.0) * (setRate / currentSampleRate);
        b.attS = sp.att->load() * 0.001 * currentSampleRate;
        b.susS = sp.sus->load() * currentSampleRate;
        b.relS = sp.rel->load() * currentSampleRate;
    }

    const float fastCoeff = std::exp (-1.0f / (static_cast<float> (currentSampleRate) * 0.0015f));
    const float slowCoeff = std::exp (-1.0f / (static_cast<float> (currentSampleRate) * 0.045f));
    const float gateAttCoeff = std::exp (-1.0f / (static_cast<float> (currentSampleRate) * (gateAttMs * 0.001f)));
    const float gateRelCoeff = std::exp (-1.0f / (static_cast<float> (currentSampleRate) * (gateRelMs * 0.001f)));
    const int gateHoldSamples = static_cast<int> (currentSampleRate * (gateHoldMs * 0.001f));
    const int retriggerSamples = static_cast<int> (currentSampleRate * (retriggerMs * 0.001f));

    float blockInPeak = 0.0f;
    float blockOutPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float dryMono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            dryMono += buffer.getSample (ch, i);
        dryMono = (dryMono / static_cast<float> (juce::jmax (1, numChannels))) * inputGainLin;

        blockInPeak = juce::jmax (blockInPeak, std::abs (dryMono));

        float filteredSc = highcutFilter.processSample (lowcutFilter.processSample (dryMono));
        float highBleedSc = bleedHighFilter.processSample (dryMono);

        float rectSc = std::abs (filteredSc);
        float rectHigh = std::abs (highBleedSc);

        fastEnvelope = fastCoeff * fastEnvelope + (1.0f - fastCoeff) * rectSc;
        slowEnvelope = slowCoeff * slowEnvelope + (1.0f - slowCoeff) * rectSc;
        highFreqEnvelope = slowCoeff * highFreqEnvelope + (1.0f - slowCoeff) * rectHigh;

        int scopeMark = 0;
        if (retriggerCounter > 0)
            --retriggerCounter;

        float transientDelta = fastEnvelope - (slowEnvelope * (1.35f - sensitivity * 0.30f));

        if (retriggerCounter == 0 && fastEnvelope > detailNorm && transientDelta > (detailNorm * 0.25f))
        {
            float bleedRatio = (fastEnvelope > 0.0001f) ? (highFreqEnvelope / fastEnvelope) : 0.0f;
            float bleedLimit = 1.4f - (suppressPct * 1.05f);

            if (suppressPct > 0.05f && bleedRatio > bleedLimit)
            {
                scopeMark = 2;
                retriggerCounter = retriggerSamples / 2;
            }
            else
            {
                float hitVelocity = juce::jlimit (0.1f, 1.0f, fastEnvelope * (1.0f + sensitivity * 0.4f));
                triggerAllActiveSlots (hitVelocity);

                retriggerCounter = retriggerSamples;
                gateHoldCounter = gateHoldSamples;
                scopeMark = 1;

                int midiVel = juce::jlimit (1, 127, static_cast<int> (hitVelocity * 127.0f));
                midiMessages.addEvent (juce::MidiMessage::noteOn (1, midiNoteNum, static_cast<juce::uint8> (midiVel)), i);
                midiNoteActive = true;
                midiNoteOffCounter = static_cast<int> (currentSampleRate * 0.05);

                if (midiRecording.load())
                {
                    const juce::ScopedLock ml (midiLock);
                    double tickTime = ((totalSamplesElapsed + i) / currentSampleRate) * 1920.0;
                    capturedSequence.addEvent (juce::MidiMessage::noteOn (1, midiNoteNum, static_cast<juce::uint8> (midiVel)), tickTime);
                    capturedSequence.addEvent (juce::MidiMessage::noteOff (1, midiNoteNum), tickTime + 120.0);
                    capturedNoteCount.fetch_add (1);
                }
            }
        }

        if (midiNoteActive && --midiNoteOffCounter <= 0)
        {
            midiMessages.addEvent (juce::MidiMessage::noteOff (1, midiNoteNum), i);
            midiNoteActive = false;
        }

        float targetGate = 1.0f;
        if (gateEnabled)
        {
            if (gateHoldCounter > 0)
            {
                --gateHoldCounter;
                targetGate = 1.0f;
                gateOpenState.store (true);
            }
            else
            {
                targetGate = 0.0f;
                gateOpenState.store (false);
            }
            float coeff = (targetGate > gateGain) ? gateAttCoeff : gateRelCoeff;
            gateGain = coeff * gateGain + (1.0f - coeff) * targetGate;
        }
        else
        {
            gateGain = 1.0f;
            gateOpenState.store (true);
        }

        float wetLeft = 0.0f;
        float wetRight = 0.0f;

        for (int s = 0; s < numSlots; ++s)
        {
            auto& slot = slots[(size_t) s];
            if (! slot.isPlaying || slot.voiceBuffer == nullptr)
                continue;

            const auto& b = sb[(size_t) s];
            const auto& vb = *slot.voiceBuffer;
            const int len = vb.getNumSamples();

            // Amplitude envelope: linear attack, sustain, linear release
            const double t = (double) slot.envelopeSamples;
            float env;
            if (t < b.attS)                         env = (float) (t / b.attS);
            else if (t < b.attS + b.susS)           env = 1.0f;
            else if (t < b.attS + b.susS + b.relS)  env = (float) (1.0 - (t - b.attS - b.susS) / b.relS);
            else
            {
                slot.isPlaying = false;
                continue;
            }

            const int idx0 = static_cast<int> (slot.playPosition);
            if (idx0 >= len)
            {
                slot.isPlaying = false;
                continue;
            }

            if (b.audible)
            {
                const int idx1 = idx0 + 1;
                const float frac = static_cast<float> (slot.playPosition - idx0);

                float sampleL = vb.getSample (0, idx0);
                if (idx1 < len)
                    sampleL += frac * (vb.getSample (0, idx1) - sampleL);

                float sampleR = sampleL;
                if (vb.getNumChannels() > 1)
                {
                    sampleR = vb.getSample (1, idx0);
                    if (idx1 < len)
                        sampleR += frac * (vb.getSample (1, idx1) - sampleR);
                }

                const float g = slot.currentVelocity * b.gain * b.phase * env;
                wetLeft  += sampleL * g * b.panL;
                wetRight += sampleR * g * b.panR;
            }

            slot.playPosition += b.tuneRatio;
            ++slot.envelopeSamples;
            if (slot.playPosition >= len)
                slot.isPlaying = false;
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float drySample = buffer.getSample (ch, i) * inputGainLin * gateGain * dryMix;
            float wetSample = (ch == 0 || numChannels == 1 ? wetLeft : wetRight) * wetMix;
            float finalOut = (drySample + wetSample) * outputGain;

            buffer.setSample (ch, i, finalOut);
            blockOutPeak = juce::jmax (blockOutPeak, std::abs (finalOut));
        }

        if ((static_cast<int> (totalSamplesElapsed) + i) % 64 == 0)
        {
            int wPos = scopeWritePos.load();
            scopeData[(size_t) wPos] = juce::jlimit (-1.0f, 1.0f, filteredSc);
            scopeMarkers[(size_t) wPos] = scopeMark;
            scopeWritePos.store ((wPos + 1) % scopeSize);
        }
    }

    totalSamplesElapsed += numSamples;
    inputLevelDb.store (juce::Decibels::gainToDecibels (blockInPeak, -60.0f));
    outputLevelDb.store (juce::Decibels::gainToDecibels (blockOutPeak, -60.0f));
}

bool AuroraTriggerAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* AuroraTriggerAudioProcessor::createEditor()
{
    return new AuroraTriggerAudioProcessorEditor (*this);
}

//==============================================================================
// State: parameters + where each slot's sound came from
void AuroraTriggerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("SLOTS"), nullptr);

    juce::ValueTree slotsTree ("SLOTS");
    {
        const juce::ScopedLock sl (slotLock);
        for (int i = 0; i < numSlots; ++i)
        {
            juce::ValueTree st ("SLOT");
            st.setProperty ("index", i, nullptr);
            st.setProperty ("type", slots[(size_t) i].sourceType, nullptr);
            st.setProperty ("path", slots[(size_t) i].sourceFile.getFullPathName(), nullptr);
            slotsTree.appendChild (st, nullptr);
        }
    }
    state.appendChild (slotsTree, nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AuroraTriggerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xmlState);
    auto slotsTree = tree.getChildWithName ("SLOTS");
    tree.removeChild (slotsTree, nullptr);
    apvts.replaceState (tree);

    if (! slotsTree.isValid())
        return; // session from v0.2.0: keep the default kit

    for (const auto& st : slotsTree)
    {
        const int i = st.getProperty ("index", -1);
        if (i < 0 || i >= numSlots)
            continue;

        const auto type = st.getProperty ("type").toString();
        const juce::File file (st.getProperty ("path").toString());

        if (type == "file")
        {
            if (file.existsAsFile())
            {
                loadFileIntoSlotAsync (i, file);
            }
            else
            {
                ++loadGeneration[(size_t) i];
                installSamples (i, nullptr, "Missing file", "Missing: " + file.getFileNameWithoutExtension(), "file", file);
                const juce::ScopedLock sl (slotLock);
                slotErrors[(size_t) i] = "File not found: " + file.getFullPathName();
            }
        }
        else if (type == "default" && i < numDemoMics)
        {
            ++loadGeneration[(size_t) i];
            synthesizeDemoSnareMic (i);
        }
        else
        {
            ++loadGeneration[(size_t) i];
            clearSlotSamples (i);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AuroraTriggerAudioProcessor();
}
