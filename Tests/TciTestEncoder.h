#pragma once

// Minimal TCI *writer* used only by the tests: it produces files in the layouts the
// decoder reads (V1 single wave, Trigger Instrument Editor, Trigger 2 V2 container with
// mono raw-head / block-chain waves and head-less stereo waves) so decoding can be
// checked sample-for-sample against known input.

#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

namespace tcitest
{
using int64 = juce::int64;

struct BitWriter
{
    std::vector<juce::uint8> bytes;
    int64 pos = 0;

    void put (juce::uint32 v, int nbits)
    {
        for (int i = nbits - 1; i >= 0; --i)
        {
            if ((size_t) (pos >> 3) >= bytes.size())
                bytes.push_back (0);
            if ((v >> i) & 1u)
                bytes[(size_t) (pos >> 3)] |= (juce::uint8) (0x80 >> (pos & 7));
            ++pos;
        }
    }

    void putSM (int v, int k)
    {
        put (v < 0 ? 1u : 0u, 1);
        if (k > 1)
            put ((juce::uint32) std::abs (v), k - 1);
    }

    // One V1 block: [k:8][n x k-bit], k = smallest width that holds the block.
    void putBlock (const int* s, int n)
    {
        int maxMag = 0;
        for (int i = 0; i < n; ++i)
            maxMag = juce::jmax (maxMag, std::abs (s[i]));
        int bits = 0;
        while ((maxMag >> bits) != 0)
            ++bits;
        const int k = maxMag == 0 ? 1 : bits + 1;
        jassert (k <= 24);
        put ((juce::uint32) k, 8);
        for (int i = 0; i < n; ++i)
            putSM (s[i], k);
    }

    void putBlocks (const std::vector<int>& s, size_t from = 0)
    {
        for (size_t i = from; i < s.size(); i += 201)
            putBlock (s.data() + i, (int) juce::jmin ((size_t) 201, s.size() - i));
    }
};

// Decaying drum-like hit, 24-bit integer samples, deterministic.
inline std::vector<int> makeHit (int frames, double amplitude, double freq, int seed)
{
    std::vector<int> s ((size_t) frames);
    juce::Random rng (seed);
    for (int i = 0; i < frames; ++i)
    {
        const double t = i / 44100.0;
        const double v = amplitude * std::exp (-t * 18.0)
                       * (0.7 * std::sin (2.0 * juce::MathConstants<double>::pi * freq * t) + 0.3 * (rng.nextDouble() * 2.0 - 1.0));
        s[(size_t) i] = (int) std::lround (juce::jlimit (-8388607.0, 8388607.0, v * 8388607.0));
    }
    return s;
}

inline void appendU32 (juce::MemoryOutputStream& out, juce::uint32 v, bool littleEndian)
{
    if (littleEndian)
        out.writeInt ((int) v);
    else
        out.writeIntBigEndian ((int) v);
}

// [01][comp BE][frames BE][V1 stream of frames-1 samples]
inline juce::MemoryBlock makeV1 (const std::vector<int>& wave)
{
    BitWriter bw;
    bw.putBlocks (std::vector<int> (wave.begin(), wave.end() - 1));
    juce::MemoryOutputStream out;
    out.writeByte (1);
    appendU32 (out, (juce::uint32) bw.pos, false);
    appendU32 (out, (juce::uint32) wave.size(), false);
    out.write (bw.bytes.data(), bw.bytes.size());
    return out.getMemoryBlock();
}

// Trigger Instrument Editor layout.
inline juce::MemoryBlock makeEditor (const std::vector<std::vector<int>>& waves)
{
    juce::MemoryOutputStream out;
    char header[128] = {};
    std::memcpy (header, "TRIGGER COMPRESSED INSTRUMENT", 29);
    out.write (header, sizeof (header));

    for (size_t w = 0; w < waves.size(); ++w)
    {
        BitWriter bw;
        bw.putBlocks (std::vector<int> (waves[w].begin(), waves[w].end() - 1));
        out.writeByte (1);
        appendU32 (out, (juce::uint32) bw.pos, true);
        appendU32 (out, (juce::uint32) waves[w].size(), true);
        out.write (bw.bytes.data(), bw.bytes.size());
        out.writeByte (w + 1 < waves.size() ? 5 : 6);
        for (int i = 0; i < 7; ++i)
            out.writeByte (0);
    }
    for (int i = 0; i < 16; ++i) // params table
        out.writeByte (0);
    return out.getMemoryBlock();
}

struct V2Wave
{
    enum class Kind { monoRaw200, monoBlocks, stereo };
    Kind kind;
    std::vector<int> left, right; // right only for stereo
};

// Trigger 2 container: 64-byte magic, audio_len, 0, 24, blob, 8-byte footer header + zlib("VC2!"+4+XML)
inline juce::MemoryBlock makeV2 (const std::vector<V2Wave>& waves)
{
    juce::MemoryOutputStream blob;
    juce::String attrs;
    attrs << " data_count=\"" << (int) waves.size() << "\"";

    for (size_t i = 0; i < waves.size(); ++i)
    {
        const auto& w = waves[i];
        BitWriter bw;
        if (w.kind == V2Wave::Kind::monoRaw200)
        {
            for (int j = 0; j < 200; ++j)
            {
                const int v = w.left[(size_t) j];
                const juce::uint32 code = (v < 0 ? 0x800000u : 0u) | (juce::uint32) std::abs (v);
                bw.put (code, 24);
            }
            bw.putBlocks (w.left, 200);
        }
        else if (w.kind == V2Wave::Kind::monoBlocks)
        {
            bw.putBlocks (w.left);
        }
        else
        {
            std::vector<int> joint;
            for (size_t j = 0; j < w.left.size(); ++j)
            {
                joint.push_back (w.left[j]);
                joint.push_back (w.right[j]);
            }
            bw.putBlocks (joint);
        }
        blob.write (bw.bytes.data(), bw.bytes.size());

        const bool st = w.kind == V2Wave::Kind::stereo;
        attrs << " wd" << (int) i << "comp1=\"" << (juce::int64) bw.pos << "\""
              << " wd" << (int) i << "frames=\"" << (int) w.left.size() << "\""
              << " wd" << (int) i << "stereo=\"" << (st ? "1" : "0") << "\""
              << " wd" << (int) i << "samples=\"" << (int) (st ? 2 * w.left.size() : w.left.size()) << "\""
              << " data" << (int) i << "_offset1=\"0\"";
    }

    juce::MemoryOutputStream footerPlain;
    footerPlain.write ("VC2!", 4);
    footerPlain.writeInt (0);
    const auto xml = "<trigger_instrument" + attrs + "/>";
    footerPlain.write (xml.toRawUTF8(), xml.getNumBytesAsUTF8());

    juce::MemoryOutputStream footerZ;
    {
        juce::GZIPCompressorOutputStream gz (footerZ, 6);
        gz.write (footerPlain.getData(), footerPlain.getDataSize());
        gz.flush();
    }

    juce::MemoryOutputStream out;
    char magic[64] = {};
    std::memcpy (magic, "TRIGGER INSTRUMENT V2 (test fixture)", 36);
    out.write (magic, sizeof (magic));
    out.writeInt ((int) blob.getDataSize());
    out.writeInt (0);
    out.writeInt (24);
    out.write (blob.getData(), blob.getDataSize());
    for (int i = 0; i < 8; ++i)
        out.writeByte (0);
    out.write (footerZ.getData(), footerZ.getDataSize());
    return out.getMemoryBlock();
}
} // namespace tcitest
