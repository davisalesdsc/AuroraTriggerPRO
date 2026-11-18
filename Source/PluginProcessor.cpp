#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

AuroraTriggerAudioProcessor::AuroraTriggerAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();
    scopeData.fill (0.0f);
    scopeMarkers.fill (0);
    loadDefaultChilliSnareKit();
}

AuroraTriggerAudioProcessor::~AuroraTriggerAudioProcessor() = default;

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

    // 8-Channel Mixer Strips (Default Chilli Snare levels: 0.00 dB, -9.12 dB, -9.12 dB)
    for (int i = 0; i < numSlots; ++i)
    {
        auto prefix = "slot" + juce::String (i) + "_";
        float defaultVol = (i == 1 || i == 2) ? -9.12f : 0.0f;
        params.push_back (std::make_unique<juce::AudioParameterFloat> (prefix + "vol", "Slot " + juce::String (i + 1) + " Vol", -60.0f, 12.0f, defaultVol));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (prefix + "pan", "Slot " + juce::String (i + 1) + " Pan", -1.0f, 1.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (prefix + "tune", "Slot " + juce::String (i + 1) + " Tune", -12.0f, 12.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterBool>  (prefix + "solo", "Slot " + juce::String (i + 1) + " Solo", false));
        params.push_back (std::make_unique<juce::AudioParameterBool>  (prefix + "mute", "Slot " + juce::String (i + 1) + " Mute", false));
        params.push_back (std::make_unique<juce::AudioParameterBool>  (prefix + "phase", "Slot " + juce::String (i + 1) + " Phase", false));
    }

    return { params.begin(), params.end() };
}

void AuroraTriggerAudioProcessor::synthesizeDemoSnareMic (int slotIndex, const juce::String& artName, const juce::String& micName,
                                                         float pitchHz, float noiseAmt, float roomDecay)
{
    const double sr = 44100.0;
    const int lengthSamples = static_cast<int> (sr * 0.85);
    juce::AudioBuffer<float> buf (2, lengthSamples);
    juce::Random rng (slotIndex * 137 + 42);

    for (int i = 0; i < lengthSamples; ++i)
    {
        float t = static_cast<float> (i) / static_cast<float> (sr);
        float bodyEnv = std::exp (-t * 22.0f);
        float wireEnv = std::exp (-t * roomDecay);

        float pitchDrop = pitchHz * (1.0f + 0.8f * std::exp (-t * 95.0f));
        float body = std::sin (juce::MathConstants<float>::twoPi * pitchDrop * t) * bodyEnv;
        float noise = (rng.nextFloat() * 2.0f - 1.0f) * wireEnv * noiseAmt;

        float sampleL = (body * 0.65f + noise * 0.55f) * 0.85f;
        float sampleR = (body * 0.65f + ((rng.nextFloat() * 2.0f - 1.0f) * wireEnv * noiseAmt) * 0.55f) * 0.85f;

        buf.setSample (0, i, sampleL);
        buf.setSample (1, i, sampleR);
    }

    const juce::ScopedLock sl (slotLock);
    slots[slotIndex].buffer = std::move (buf);
    slots[slotIndex].sampleRate = sr;
    slots[slotIndex].articulationName = artName;
    slots[slotIndex].micLabel = micName;
    slots[slotIndex].loaded = true;
}

void AuroraTriggerAudioProcessor::loadDefaultChilliSnareKit()
{
    activePresetName = "Chilli Snare";
    synthesizeDemoSnareMic (0, "Snare Full", "ChiliSnare Z1 mic",   192.0f, 0.45f, 18.0f);
    synthesizeDemoSnareMic (1, "Snare Full", "ChiliSnare SSDR mic", 185.0f, 0.72f, 11.0f);
    synthesizeDemoSnareMic (2, "Snare Full", "ChiliSnare NRG mic",  178.0f, 0.88f, 6.5f);
}

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

bool AuroraTriggerAudioProcessor::loadSampleIntoSlot (int slotIndex, const juce::File& file)
{
    if (slotIndex < 0 || slotIndex >= numSlots)
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return false;

    juce::AudioBuffer<float> newBuffer (static_cast<int> (reader->numChannels),
                                        static_cast<int> (reader->lengthInSamples));
    reader->read (&newBuffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);

    const juce::ScopedLock sl (slotLock);
    slots[slotIndex].buffer = std::move (newBuffer);
    slots[slotIndex].sampleRate = reader->sampleRate;
    slots[slotIndex].articulationName = "Custom Hit";
    slots[slotIndex].micLabel = file.getFileNameWithoutExtension();
    slots[slotIndex].loaded = true;
    slots[slotIndex].isPlaying = false;
    slots[slotIndex].playPosition = 0.0;
    return true;
}

void AuroraTriggerAudioProcessor::clearSlot (int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= numSlots)
        return;

    const juce::ScopedLock sl (slotLock);
    slots[slotIndex].loaded = false;
    slots[slotIndex].isPlaying = false;
    slots[slotIndex].articulationName = "Empty";
    slots[slotIndex].micLabel = "Empty";
    slots[slotIndex].buffer.setSize (0, 0);
}

void AuroraTriggerAudioProcessor::triggerAudition (int slotIndex, float velocity)
{
    if (slotIndex < 0 || slotIndex >= numSlots)
        return;

    const juce::ScopedLock sl (slotLock);
    if (slots[slotIndex].loaded)
    {
        slots[slotIndex].isPlaying = true;
        slots[slotIndex].playPosition = 0.0;
        slots[slotIndex].currentVelocity = velocity;
    }
}

void AuroraTriggerAudioProcessor::triggerAllActiveSlots (float velocity)
{
    const juce::ScopedLock sl (slotLock);
    for (int i = 0; i < numSlots; ++i)
    {
        if (slots[i].loaded)
        {
            slots[i].isPlaying = true;
            slots[i].playPosition = 0.0;
            slots[i].currentVelocity = velocity;
        }
    }
}

void AuroraTriggerAudioProcessor::flipAllPhases()
{
    for (int i = 0; i < numSlots; ++i)
    {
        auto paramId = "slot" + juce::String (i) + "_phase";
        if (auto* param = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (paramId)))
            param->setValueNotifyingHost (! param->get());
    }
}

void AuroraTriggerAudioProcessor::applyLinkedTune (int sourceSlot, float newTuneValue)
{
    if (isSyncingLinkedTune)
        return;

    bool isLinked = apvts.getRawParameterValue ("linkTune")->load() > 0.5f;
    if (! isLinked)
        return;

    isSyncingLinkedTune = true;
    for (int i = 0; i < numSlots; ++i)
    {
        if (i == sourceSlot)
            continue;

        auto paramId = "slot" + juce::String (i) + "_tune";
        if (auto* param = apvts.getParameter (paramId))
            param->setValueNotifyingHost (param->convertTo0to1 (newTuneValue));
    }
    isSyncingLinkedTune = false;
}

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

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            triggerAllActiveSlots (msg.getFloatVelocity());
    }

    bool anySlotSoloed = false;
    for (int s = 0; s < numSlots; ++s)
    {
        if (apvts.getRawParameterValue ("slot" + juce::String (s) + "_solo")->load() > 0.5f)
        {
            anySlotSoloed = true;
            break;
        }
    }

    const float fastCoeff = std::exp (-1.0f / (static_cast<float> (currentSampleRate) * 0.0015f));
    const float slowCoeff = std::exp (-1.0f / (static_cast<float> (currentSampleRate) * 0.045f));
    const float gateAttCoeff = std::exp (-1.0f / (static_cast<float> (currentSampleRate) * (gateAttMs * 0.001f)));
    const float gateRelCoeff = std::exp (-1.0f / (static_cast<float> (currentSampleRate) * (gateRelMs * 0.001f)));
    const int gateHoldSamples = static_cast<int> (currentSampleRate * (gateHoldMs * 0.001f));
    const int retriggerSamples = static_cast<int> (currentSampleRate * (retriggerMs * 0.001f));

    float blockInPeak = 0.0f;
    float blockOutPeak = 0.0f;

    const juce::ScopedLock sl (slotLock);

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
            auto& slot = slots[s];
            if (! slot.loaded || ! slot.isPlaying)
                continue;

            auto prefix = "slot" + juce::String (s) + "_";
            bool isMuted = apvts.getRawParameterValue (prefix + "mute")->load() > 0.5f;
            bool isSolo  = apvts.getRawParameterValue (prefix + "solo")->load() > 0.5f;

            if (isMuted || (anySlotSoloed && ! isSolo))
            {
                slot.playPosition += 1.0;
                if (slot.playPosition >= slot.buffer.getNumSamples())
                    slot.isPlaying = false;
                continue;
            }

            float slotGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue (prefix + "vol")->load());
            float slotPan  = apvts.getRawParameterValue (prefix + "pan")->load();
            float slotTune = apvts.getRawParameterValue (prefix + "tune")->load();
            bool phaseInv  = apvts.getRawParameterValue (prefix + "phase")->load() > 0.5f;

            double pitchRatio = std::pow (2.0, slotTune / 12.0) * (slot.sampleRate / currentSampleRate);
            int idx0 = static_cast<int> (slot.playPosition);
            int idx1 = idx0 + 1;
            float frac = static_cast<float> (slot.playPosition - idx0);

            if (idx0 >= slot.buffer.getNumSamples())
            {
                slot.isPlaying = false;
                continue;
            }

            float sampleL = slot.buffer.getSample (0, idx0);
            if (idx1 < slot.buffer.getNumSamples())
                sampleL += frac * (slot.buffer.getSample (0, idx1) - sampleL);

            float sampleR = (slot.buffer.getNumChannels() > 1) ? slot.buffer.getSample (1, idx0) : sampleL;
            if (slot.buffer.getNumChannels() > 1 && idx1 < slot.buffer.getNumSamples())
                sampleR += frac * (slot.buffer.getSample (1, idx1) - sampleR);

            float phaseMult = phaseInv ? -1.0f : 1.0f;
            float panL = std::cos ((slotPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
            float panR = std::sin ((slotPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);

            wetLeft  += sampleL * slot.currentVelocity * slotGain * phaseMult * panL;
            wetRight += sampleR * slot.currentVelocity * slotGain * phaseMult * panR;

            slot.playPosition += pitchRatio;
            if (slot.playPosition >= slot.buffer.getNumSamples())
                slot.isPlaying = false;
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float drySample = buffer.getSample (ch, i) * inputGainLin * gateGain * dryMix;
            float wetSample = (ch == 0 ? wetLeft : wetRight) * wetMix;
            float finalOut = (drySample + wetSample) * outputGain;

            buffer.setSample (ch, i, finalOut);
            blockOutPeak = juce::jmax (blockOutPeak, std::abs (finalOut));
        }

        if ((static_cast<int> (totalSamplesElapsed) + i) % 64 == 0)
        {
            int wPos = scopeWritePos.load();
            scopeData[wPos] = juce::jlimit (-1.0f, 1.0f, filteredSc);
            scopeMarkers[wPos] = scopeMark;
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

void AuroraTriggerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AuroraTriggerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AuroraTriggerAudioProcessor();
}
