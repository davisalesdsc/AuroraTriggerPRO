// Processor behaviour: TCI velocity layers + round robin, curves (velocity / dynamics /
// range / envelope), Curve Link, slot Init, session state with sample paths.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/PluginProcessor.h"
#include "TciTestEncoder.h"
#include <functional>
#include <iostream>

namespace
{
using CheckFn = std::function<void (bool, const juce::String&)>;

void setParam (AuroraTriggerAudioProcessor& p, const juce::String& id, float value)
{
    auto* param = p.apvts.getParameter (id);
    jassert (param != nullptr);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

float getParam (AuroraTriggerAudioProcessor& p, const juce::String& id)
{
    return p.apvts.getRawParameterValue (id)->load();
}

struct Render
{
    float peak = 0.0f;
    double sum = 0.0;
};

// Sends one MIDI note (velocity 0 = none) and renders ~6000 samples of output.
Render renderHit (AuroraTriggerAudioProcessor& p, int velocity, int blocks = 12)
{
    Render r;
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        buf.clear();
        midi.clear();
        if (b == 0 && velocity > 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 38, (juce::uint8) velocity), 0);
        p.processBlock (buf, midi);
        r.peak = juce::jmax (r.peak, buf.getMagnitude (0, 0, 512));
        for (int i = 0; i < 512; ++i)
            r.sum += std::abs (buf.getSample (0, i));
    }
    return r;
}

juce::File writeTempFile (const juce::String& name, const juce::MemoryBlock& data)
{
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("AuroraTriggerTests").getChildFile (name);
    f.getParentDirectory().createDirectory();
    f.replaceWithData (data.getData(), data.getSize());
    return f;
}

juce::File writeTempWav (const juce::String& name, float amplitude)
{
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("AuroraTriggerTests").getChildFile (name);
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    juce::AudioBuffer<float> b (1, 4000);
    for (int i = 0; i < 4000; ++i)
        b.setSample (0, i, amplitude * std::exp (-i / 800.0f) * std::sin (i * 0.05f));
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), 44100.0, 1, 24, {}, 0)))
    {
        os.release();
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
    return f;
}
} // namespace

void runProcessorTests (std::function<void (bool, const juce::String&)> check, std::function<void (const juce::String&)> section)
{
    using P = AuroraTriggerAudioProcessor;

    section ("Curves math");
    {
        check (std::abs (P::mapVelocity (1.0f, 50.0f, 0.0f, 127.0f) - 127.0f) < 1.0e-3f, "full velocity maps to 127");
        check (std::abs (P::mapVelocity (0.5f, 50.0f, 0.0f, 127.0f) - 63.5f) < 1.0e-3f, "VEL 50 is linear");
        check (P::mapVelocity (0.25f, 100.0f, 0.0f, 127.0f) > 0.25f * 127.0f + 20.0f, "VEL 100 boosts soft hits");
        check (P::mapVelocity (0.25f, 0.0f, 0.0f, 127.0f) < 0.25f * 127.0f - 20.0f, "VEL 0 favours hard hits");
        check (std::abs (P::mapVelocity (0.0f, 50.0f, 40.0f, 100.0f) - 40.0f) < 1.0e-3f, "range min is the floor");
        check (std::abs (P::mapVelocity (1.0f, 50.0f, 40.0f, 100.0f) - 100.0f) < 1.0e-3f, "range max is the ceiling");
        check (std::abs (P::dynamicsGain (10.0f, 0.0f) - 1.0f) < 1.0e-6f, "DYN 0 ignores velocity");
        check (std::abs (P::dynamicsGain (0.0f, 100.0f)) < 1.0e-6f, "DYN 100 at velocity 0 is silent");
        check (std::abs (P::dynamicsGain (127.0f, 100.0f) - 1.0f) < 1.0e-6f, "DYN 100 at velocity 127 is unity");
        check (P::isSupportedFile (juce::File::getCurrentWorkingDirectory().getChildFile ("Kick.TCI")), ".tci (any case) is supported");
        check (! P::isSupportedFile (juce::File::getCurrentWorkingDirectory().getChildFile ("notes.txt")), ".txt is not supported");
    }

    P proc;
    proc.prepareToPlay (44100.0, 512);

    section ("Default kit and Init (right-click > Clear Slot)");
    {
        check (proc.getSlotInfo (0).loaded && proc.getSlotInfo (2).loaded, "Chilli Snare kit fills slots 1-3");
        check (! proc.getSlotInfo (3).loaded, "slot 4 starts empty");
        check (std::abs (getParam (proc, "slot1_vol") + 9.12f) < 0.01f, "slot 2 starts at -9.12 dB");
        setParam (proc, "slot1_tune", 5.0f);
        setParam (proc, "slot1_att", 120.0f);
        setParam (proc, "linkTune", 1.0f);
        for (int i = 0; i < 3; ++i)
            proc.initSlot (i);
        check (! proc.getSlotInfo (0).loaded && ! proc.getSlotInfo (1).loaded && ! proc.getSlotInfo (2).loaded, "Init empties the slots");
        check (proc.getSlotInfo (1).micLabel == "Empty", "an initialised slot is labelled Empty");
        check (std::abs (getParam (proc, "slot1_vol")) < 0.01f, "Init resets volume to 0 dB");
        check (std::abs (getParam (proc, "slot1_tune")) < 0.01f, "Init resets tune");
        check (std::abs (getParam (proc, "slot1_att")) < 0.01f, "Init resets curve attack");
        setParam (proc, "linkTune", 0.0f);
        check (renderHit (proc, 100).peak == 0.0f, "no sound once every slot is empty");
    }

    // TCI with 3 velocity layers: soft (2 round robins), mid, loud
    const auto hitA = tcitest::makeHit (6000, 0.80, 190.0, 1);
    const auto hitB = tcitest::makeHit (5000, 0.20, 180.0, 2);
    const auto hitB2 = tcitest::makeHit (5000, 0.21, 181.0, 3);
    const auto hitC = tcitest::makeHit (4000, 0.50, 200.0, 4);
    const auto tciFile = writeTempFile ("Snare Test.tci", tcitest::makeEditor ({ hitC, hitB, hitA, hitB2 }));

    section ("TCI loads into a slot in the background");
    {
        proc.loadFileIntoSlotAsync (3, tciFile);
        check (proc.waitForPendingLoads (20000), "load finishes");
        const auto info = proc.getSlotInfo (3);
        check (info.loaded, "slot 4 loaded: " + info.error);
        check (info.numLayers == 3, "3 velocity layers, got " + juce::String (info.numLayers));
        check (info.numHits == 4, "4 hits");
        check (info.micLabel == "Snare Test", "pad shows the file name");
        check (info.error.isEmpty(), "no warning for a mono TCI");
    }

    section ("Velocity picks the layer, round robin alternates");
    {
        setParam (proc, "slot3_dyn", 0.0f); // level from the samples only
        const auto soft = renderHit (proc, 20);
        const auto mid = renderHit (proc, 64);
        const auto loud = renderHit (proc, 127);
        check (soft.peak > 0.0f, "soft hit sounds");
        check (mid.peak > soft.peak * 1.8f, "mid layer louder than soft");
        check (loud.peak > mid.peak * 1.3f, "loud layer louder than mid");

        const auto rr1 = renderHit (proc, 20);
        const auto rr2 = renderHit (proc, 20);
        check (std::abs (rr1.sum - rr2.sum) > 1.0e-3, "consecutive soft hits use different round robins");
        const auto rr3 = renderHit (proc, 20);
        check (std::abs (rr3.sum - rr1.sum) < 1.0e-6, "round robin cycles back");
    }

    section ("Curves shape the hit");
    {
        setParam (proc, "slot3_dyn", 100.0f);
        const auto full = renderHit (proc, 127);
        const auto half = renderHit (proc, 64);
        check (full.peak > 0.0f && half.peak < full.peak, "DYN 100 makes softer hits quieter");

        setParam (proc, "slot3_rmax", 0.0f);
        check (renderHit (proc, 127).peak == 0.0f, "Range Max 0 silences the slot");
        setParam (proc, "slot3_rmax", 127.0f);

        setParam (proc, "slot3_sus", 0.0f);
        setParam (proc, "slot3_rel", 0.0f);
        check (renderHit (proc, 127).peak == 0.0f, "Sustain 0 + Release 0 cut the sound");
        setParam (proc, "slot3_sus", 0.02f);
        setParam (proc, "slot3_rel", 0.0f);
        const auto shortHit = renderHit (proc, 127);
        check (shortHit.peak > 0.0f && shortHit.sum < full.sum * 0.6, "short sustain shortens the hit");
        setParam (proc, "slot3_sus", 5.0f);
        setParam (proc, "slot3_rel", 10.0f);

        setParam (proc, "slot3_att", 200.0f);
        const auto slow = renderHit (proc, 127);
        check (slow.peak < full.peak * 0.8f, "slow attack softens the transient");
        setParam (proc, "slot3_att", 0.0f);

        setParam (proc, "slot3_mute", 1.0f);
        check (renderHit (proc, 127).peak == 0.0f, "mute silences the slot");
        setParam (proc, "slot3_mute", 0.0f);
    }

    section ("Curve Link");
    {
        proc.applyLinkedSlotParam ("dyn", 3, 42.0f);
        check (std::abs (getParam (proc, "slot5_dyn") - 100.0f) < 0.01f, "unlinked: other slots keep their value");
        setParam (proc, "curveLinkDynamics", 1.0f);
        setParam (proc, "slot3_dyn", 42.0f);
        proc.applyLinkedSlotParam ("dyn", 3, 42.0f);
        bool all = true;
        for (int i = 0; i < P::numSlots; ++i)
            all = all && std::abs (getParam (proc, "slot" + juce::String (i) + "_dyn") - 42.0f) < 0.01f;
        check (all, "linked: every slot follows");
        check (P::linkParamIdFor ("rmax") == "curveLinkRangeMax" && P::linkParamIdFor ("tune") == "linkTune", "link ids");
        setParam (proc, "curveLinkDynamics", 0.0f);
    }

    section ("A bad file keeps the slot's current sound");
    {
        juce::MemoryBlock junk (2048);
        junk.fillWith (0x5a);
        const auto bad = writeTempFile ("broken.tci", junk);
        proc.loadFileIntoSlotAsync (3, bad);
        proc.waitForPendingLoads (20000);
        const auto info = proc.getSlotInfo (3);
        check (info.loaded && info.numLayers == 3, "previous TCI still loaded");
        check (info.error.isNotEmpty(), "error is reported: " + info.error);
    }

    const auto wavFile = writeTempWav ("Kick Test.wav", 0.7f);

    section ("WAV loads as one layer");
    {
        proc.loadFileIntoSlotAsync (4, wavFile);
        proc.waitForPendingLoads (20000);
        const auto info = proc.getSlotInfo (4);
        check (info.loaded && info.numLayers == 1 && info.numHits == 1, "wav = 1 layer / 1 hit: " + info.error);
    }

    section ("Session state remembers what each slot holds");
    {
        juce::MemoryBlock state;
        proc.getStateInformation (state);

        P restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.waitForPendingLoads (20000), "restored loads finish");
        check (! restored.getSlotInfo (0).loaded, "slot 1 stays empty (was Init) instead of the default kit");
        const auto s3 = restored.getSlotInfo (3);
        check (s3.loaded && s3.numLayers == 3, "slot 4 reloads the TCI from its path");
        check (restored.getSlotInfo (4).loaded, "slot 5 reloads the WAV");
        check (std::abs (getParam (restored, "slot3_dyn") - 42.0f) < 0.01f, "parameters restored");

        // A session saved by v0.2.0 (no slot list) keeps the built-in kit
        P legacy;
        legacy.initSlot (0);
        auto onlyParams = proc.apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml (onlyParams.createXml());
        juce::MemoryBlock legacyState;
        juce::AudioProcessor::copyXmlToBinary (*xml, legacyState);
        P fresh;
        fresh.setStateInformation (legacyState.getData(), (int) legacyState.getSize());
        check (fresh.getSlotInfo (0).loaded && fresh.getSlotInfo (1).loaded, "old sessions keep the Chilli Snare kit");

        // Missing sample file: slot shows it, path is kept for the next save
        tciFile.copyFileTo (tciFile.getSiblingFile ("Moved.tci"));
        P temp;
        temp.loadFileIntoSlotAsync (6, tciFile.getSiblingFile ("Moved.tci"));
        temp.waitForPendingLoads (20000);
        juce::MemoryBlock withMoved;
        temp.getStateInformation (withMoved);
        tciFile.getSiblingFile ("Moved.tci").deleteFile();

        P missing;
        missing.setStateInformation (withMoved.getData(), (int) withMoved.getSize());
        missing.waitForPendingLoads (20000);
        const auto m = missing.getSlotInfo (6);
        check (! m.loaded && m.micLabel == "Missing: Moved", "missing file is shown on the pad");
        check (m.error.contains ("not found"), "missing file error");
        juce::MemoryBlock again;
        missing.getStateInformation (again);
        check (again.toString().contains ("Moved.tci") || juce::AudioProcessor::getXmlFromBinary (again.getData(), (int) again.getSize())->toString().contains ("Moved.tci"),
               "path of the missing file is kept");
    }

    section ("Init with links on only resets that slot");
    {
        setParam (proc, "curveLinkDynamics", 1.0f);
        proc.initSlot (3);
        check (! proc.getSlotInfo (3).loaded, "slot 4 cleared");
        check (std::abs (getParam (proc, "slot3_dyn") - 100.0f) < 0.01f, "slot 4 dynamics back to 100");
        check (std::abs (getParam (proc, "slot5_dyn") - 42.0f) < 0.01f, "other slots untouched");
        setParam (proc, "curveLinkDynamics", 0.0f);
    }

    section ("Editor opens and closes");
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        check (ed != nullptr && ed->getWidth() == 1040 && ed->getHeight() == 640, "editor 1040 x 640");
        ed.reset();
    }

    tciFile.getParentDirectory().deleteRecursively();
}

//==============================================================================
// --screenshots DIR: renders the editor in each view to PNG (layout review)
namespace
{
juce::Button* findButton (juce::Component& root, const juce::String& text)
{
    for (auto* c : root.getChildren())
        if (auto* b = dynamic_cast<juce::Button*> (c))
            if (b->getButtonText() == text)
                return b;
    return nullptr;
}

void snapshot (juce::Component& c, const juce::File& file)
{
    const auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 2.0f);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat().writeImageToStream (img, out);
}
} // namespace

int writeScreenshots (const juce::File& dir)
{
    dir.createDirectory();

    // A small sample folder for the browser view
    auto lib = dir.getChildFile ("Sample Library");
    lib.getChildFile ("Kicks").createDirectory();
    lib.getChildFile ("Snares").createDirectory();
    writeTempWav ("x.wav", 0.5f).copyFileTo (lib.getChildFile ("Kicks/Kick Punch.wav"));
    writeTempWav ("x.wav", 0.5f).copyFileTo (lib.getChildFile ("Kicks/Kick Deep.wav"));
    const auto tci = tcitest::makeEditor ({ tcitest::makeHit (4000, 0.3, 180, 1), tcitest::makeHit (4000, 0.7, 180, 2) });
    lib.getChildFile ("Snares/Snare Fat.tci").replaceWithData (tci.getData(), tci.getSize());
    lib.getChildFile ("Snares/Snare Tight.tci").replaceWithData (tci.getData(), tci.getSize());

    juce::PropertiesFile::Options opts;
    opts.applicationName = "AuroraTrigger";
    opts.folderName = "Aurora Audio";
    opts.filenameSuffix = ".settings";
    opts.osxLibrarySubFolder = "Application Support";
    juce::PropertiesFile settings (opts);
    settings.setValue ("browserRoot", lib.getFullPathName());
    settings.saveIfNeeded();

    AuroraTriggerAudioProcessor proc;
    proc.loadFileIntoSlotAsync (3, lib.getChildFile ("Snares/Snare Fat.tci"));
    proc.waitForPendingLoads (20000);
    setParam (proc, "slot0_vel", 75.0f);
    setParam (proc, "slot0_dyn", 60.0f);
    setParam (proc, "slot1_att", 40.0f);
    setParam (proc, "slot1_sus", 1.2f);
    setParam (proc, "slot2_rmin", 30.0f);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto pump = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (400); };
    pump();
    snapshot (*ed, dir.getChildFile ("1-triggering-mixer.png"));

    if (auto* b = findButton (*ed, "View Curves")) b->onClick();
    pump();
    snapshot (*ed, dir.getChildFile ("2-triggering-curves.png"));

    if (auto* b = findButton (*ed, "View Mixer")) b->onClick();
    if (auto* b = findButton (*ed, "Browser")) b->onClick();
    pump();
    snapshot (*ed, dir.getChildFile ("3-browser.png"));

    ed.reset();
    std::cout << "screenshots written to " << dir.getFullPathName() << std::endl;
    return 0;
}
