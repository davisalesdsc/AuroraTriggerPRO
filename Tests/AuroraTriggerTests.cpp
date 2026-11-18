// Offline tests for Aurora Trigger.
//
//   AuroraTriggerTests                       run every test, exit 0 on success
//   AuroraTriggerTests --write-fixtures DIR  write the synthetic .tci fixtures to DIR
//   AuroraTriggerTests --screenshots DIR     render the editor views to PNG
//   AuroraTriggerTests --dump FILE.tci       print one line per decoded wave
//                                            (frames, channels, checksum) for parity checks

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/TciDecoder.h"
#include "TciTestEncoder.h"
#include <functional>
#include <iostream>

namespace
{
int failures = 0;
int checks = 0;

void check (bool ok, const juce::String& what)
{
    ++checks;
    if (! ok)
    {
        ++failures;
        std::cout << "  FAIL: " << what << std::endl;
    }
}

void section (const juce::String& name) { std::cout << "- " << name << std::endl; }

// 24-bit integer view of a decoded channel (exact: values are k/2^23).
std::vector<int> toInts (const juce::AudioBuffer<float>& b, int ch)
{
    std::vector<int> v ((size_t) b.getNumSamples());
    for (int i = 0; i < b.getNumSamples(); ++i)
        v[(size_t) i] = (int) std::lround (b.getSample (ch, i) * 8388608.0);
    return v;
}

std::optional<tci::Instrument> decode (const juce::MemoryBlock& m, juce::String& err)
{
    return tci::decodeMemory (m.getData(), m.getSize(), err);
}

//==============================================================================
struct Fixtures
{
    std::vector<int> hitA = tcitest::makeHit (6000, 0.80, 190.0, 1);
    std::vector<int> hitB = tcitest::makeHit (5000, 0.20, 180.0, 2);
    std::vector<int> hitB2 = tcitest::makeHit (5000, 0.21, 181.0, 3); // within 12% of hitB -> round robin
    std::vector<int> hitC = tcitest::makeHit (4000, 0.50, 200.0, 4);
    std::vector<int> stL = tcitest::makeHit (7000, 0.60, 150.0, 5);
    std::vector<int> stR = tcitest::makeHit (7000, 0.55, 152.0, 6);

    juce::MemoryBlock v1() const { return tcitest::makeV1 (hitA); }
    juce::MemoryBlock editor() const { return tcitest::makeEditor ({ hitC, hitB, hitA, hitB2 }); }
    juce::MemoryBlock v2() const
    {
        using K = tcitest::V2Wave::Kind;
        return tcitest::makeV2 ({ { K::monoRaw200, hitA, {} },
                                  { K::monoBlocks, hitC, {} },
                                  { K::stereo, stL, stR },
                                  { K::monoRaw200, hitB, {} } });
    }
};

void testTci()
{
    const Fixtures fx;
    juce::String err;

    section ("TCI: V1 single wave decodes bit-exact");
    {
        auto inst = decode (fx.v1(), err);
        check (inst.has_value(), "V1 decodes: " + err);
        if (inst)
        {
            check (inst->container == "V1", "container is V1");
            check (inst->waves.size() == 1, "one wave");
            const auto got = toInts (inst->waves[0].audio, 0);
            check ((int) got.size() == (int) fx.hitA.size(), "voice rule keeps `frames` samples");
            bool same = true;
            for (size_t i = 0; i + 1 < fx.hitA.size(); ++i)
                same = same && got[i] == fx.hitA[i];
            check (same, "V1 samples identical");
            check (got.back() == 0, "last frame is zero-padded (stream holds frames-1)");
        }
    }

    section ("TCI: Instrument Editor file -> velocity layers + round robin");
    {
        auto inst = decode (fx.editor(), err);
        check (inst.has_value(), "Editor decodes: " + err);
        if (inst)
        {
            check (inst->container == "Editor", "container is Editor");
            check (inst->waves.size() == 4, "four waves");
            // peaks: hitB 0.20, hitB2 0.21 (same layer), hitC 0.50, hitA 0.80
            check (inst->layers.size() == 3, "three velocity layers, got " + juce::String ((int) inst->layers.size()));
            if (inst->layers.size() == 3)
            {
                check (inst->layers[0].size() == 2, "soft layer has 2 round robins");
                check (inst->layers[0][0] == 1 && inst->layers[0][1] == 3, "soft layer = hitB, hitB2");
                check (inst->layers[1] == std::vector<int> { 0 }, "mid layer = hitC");
                check (inst->layers[2] == std::vector<int> { 2 }, "loud layer = hitA");
            }
            if (inst->waves.size() == 4)
            {
                const auto got = toInts (inst->waves[2].audio, 0);
                bool same = true;
                for (size_t i = 0; i + 1 < fx.hitA.size(); ++i)
                    same = same && got[i] == fx.hitA[i];
                check (same, "Editor wave samples identical");
            }
        }
    }

    section ("TCI: Trigger 2 V2 container (mono raw head, mono blocks, stereo)");
    {
        auto inst = decode (fx.v2(), err);
        check (inst.has_value(), "V2 decodes: " + err);
        if (inst)
        {
            check (inst->container == "V2", "container is V2");
            check (inst->waves.size() == 4, "four waves, got " + juce::String ((int) inst->waves.size()));
            check (inst->stereoWaves == 1, "one stereo wave");
            check (inst->skippedWaves == 0, "nothing skipped");
            if (inst->waves.size() == 4)
            {
                check (toInts (inst->waves[0].audio, 0) == fx.hitA, "raw200 mono wave identical");
                check (toInts (inst->waves[1].audio, 0) == fx.hitC, "block-chain mono wave identical");
                const auto& st = inst->waves[2];
                check (st.audio.getNumChannels() == 2, "stereo wave has 2 channels");
                check (st.silentHeadFrames == 0, "head-less stereo wave has no silent head");
                if (st.audio.getNumChannels() == 2)
                {
                    check (toInts (st.audio, 0) == fx.stL, "stereo L identical");
                    check (toInts (st.audio, 1) == fx.stR, "stereo R identical");
                }
                check (toInts (inst->waves[3].audio, 0) == fx.hitB, "second raw200 wave identical");
            }
        }
    }

    section ("TCI: garbage is rejected, not crashed on");
    {
        juce::MemoryBlock junk (4096);
        juce::Random rng (99);
        for (size_t i = 0; i < junk.getSize(); ++i)
            junk[i] = (char) rng.nextInt (256);
        auto inst = decode (junk, err);
        check (! inst.has_value(), "random bytes are not a TCI");
        check (err.isNotEmpty(), "an error message is given");

        auto truncated = fx.v2();
        truncated.setSize (truncated.getSize() / 2);
        check (! decode (truncated, err).has_value(), "truncated V2 is rejected");
    }
}

//==============================================================================
int dumpFile (const juce::File& f)
{
    juce::String err;
    auto inst = tci::decodeFile (f, err);
    if (! inst)
    {
        std::cout << "ERROR " << err << std::endl;
        return 1;
    }
    for (const auto& w : inst->waves)
    {
        juce::int64 sum = 0, wsum = 0;
        for (int ch = 0; ch < w.audio.getNumChannels(); ++ch)
        {
            const auto v = toInts (w.audio, ch);
            for (size_t i = 0; i < v.size(); ++i)
            {
                sum += v[i];
                wsum += (juce::int64) v[i] * (juce::int64) ((i % 1000) + 1);
            }
        }
        std::cout << "wave" << w.index << " ch=" << w.audio.getNumChannels() << " frames=" << w.audio.getNumSamples()
                  << " sum=" << sum << " wsum=" << wsum << std::endl;
    }
    return 0;
}

int writeFixtures (const juce::File& dir)
{
    dir.createDirectory();
    const Fixtures fx;
    dir.getChildFile ("v1.tci").replaceWithData (fx.v1().getData(), fx.v1().getSize());
    dir.getChildFile ("editor.tci").replaceWithData (fx.editor().getData(), fx.editor().getSize());
    dir.getChildFile ("v2.tci").replaceWithData (fx.v2().getData(), fx.v2().getSize());
    std::cout << "fixtures written to " << dir.getFullPathName() << std::endl;
    return 0;
}
} // namespace

void runProcessorTests (std::function<void (bool, const juce::String&)> checkFn,
                        std::function<void (const juce::String&)> sectionFn);
int writeScreenshots (const juce::File& dir);

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc == 3 && juce::String (argv[1]) == "--dump")
        return dumpFile (juce::File (juce::String (argv[2])));
    if (argc == 3 && juce::String (argv[1]) == "--screenshots")
        return writeScreenshots (juce::File (juce::String (argv[2])));
    if (argc == 3 && juce::String (argv[1]) == "--write-fixtures")
        return writeFixtures (juce::File (juce::String (argv[2])));

    testTci();
    runProcessorTests (check, section);

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << " (" << checks - failures << "/" << checks << " checks)" << std::endl;
    return failures == 0 ? 0 : 1;
}
