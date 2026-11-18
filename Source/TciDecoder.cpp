// C++ port of the tci2wav decoder (MIT License, Copyright (c) 2026 Aidan Scroggins,
// https://github.com/AidanScroggins/tci2wav). The structure mirrors web/decode.js,
// web/solve.js, web/stereo.js and web/export.js so behaviour stays comparable;
// see that project's WRITEUP.md for how the format was worked out.

#include "TciDecoder.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace tci
{
namespace
{
using int64 = juce::int64;

constexpr int blockSamples = 201;
constexpr int maxK = 24;
constexpr int attackWindow = 5000;          // frames used to measure the attack peak
constexpr int64 maxFrames = 20 * 60 * 48000; // sanity cap against corrupt headers
constexpr float fullScale = 8388608.0f;      // 2^23: values are 24-bit sign-magnitude

//==============================================================================
// MSB-first bitstream over a byte range. Reads past the end yield zero bits,
// matching the reference decoder.
struct Bits
{
    const juce::uint8* d = nullptr;
    size_t n = 0;

    int64 bitLength() const noexcept { return (int64) n * 8; }

    int byteAtIndex (size_t i) const noexcept { return i < n ? d[i] : 0; }

    int byteAt (int64 pos) const noexcept
    {
        if (pos < 0)
            return 0;
        const auto j = (size_t) (pos >> 3);
        const int s = (int) (pos & 7);
        int v = byteAtIndex (j) << s;
        if (s != 0)
            v |= byteAtIndex (j + 1) >> (8 - s);
        return v & 0xff;
    }

    // k-bit sign-magnitude field (first bit = sign). k = 1 always decodes to 0.
    int readSM (int64 pos, int k) const noexcept
    {
        jassert (k >= 1 && k <= maxK);
        const auto j = (size_t) (pos >> 3);
        const int s = (int) (pos & 7);
        const juce::uint32 word = ((juce::uint32) byteAtIndex (j) << 24) | ((juce::uint32) byteAtIndex (j + 1) << 16)
                                | ((juce::uint32) byteAtIndex (j + 2) << 8) | (juce::uint32) byteAtIndex (j + 3);
        const juce::uint32 code = (word >> (32 - s - k)) & ((1u << k) - 1u);
        const int mag = (int) (code & ((1u << (k - 1)) - 1u));
        return (((code >> (k - 1)) & 1u) != 0 && mag != 0) ? -mag : mag;
    }

    Bits from (size_t byteOffset) const noexcept
    {
        const auto o = juce::jmin (byteOffset, n);
        return { d + o, n - o };
    }
};

int sm24 (int b0, int b1, int b2) noexcept
{
    const int v = (b0 << 16) | (b1 << 8) | b2;
    return (v & 0x800000) ? -(v & 0x7fffff) : v;
}

juce::uint32 readU32 (const juce::uint8* p, bool littleEndian) noexcept
{
    return littleEndian ? ((juce::uint32) p[0] | ((juce::uint32) p[1] << 8) | ((juce::uint32) p[2] << 16) | ((juce::uint32) p[3] << 24))
                        : (((juce::uint32) p[0] << 24) | ((juce::uint32) p[1] << 16) | ((juce::uint32) p[2] << 8) | (juce::uint32) p[3]);
}

//==============================================================================
struct RawWave
{
    int64 comp = 0;          // used bit length
    int64 frames = 0;        // voice play length
    juce::String stereo;     // footer flag, "1..." = stereo
    Bits blob;               // V2 only
    bool hasV1 = false;      // V1 / Editor waves arrive already decoded
    std::vector<int> v1;
};

// Chain of [k:8][201 x k-bit] blocks from `bitPos`; must yield exactly `count`
// samples and consume exactly `comp` bits.
std::optional<std::vector<int>> v1Stream (const Bits& b, int64 bitPos, int64 comp, int64 count)
{
    if (count < 0 || count > maxFrames)
        return std::nullopt;

    std::vector<int> out ((size_t) count);
    const int64 end = bitPos + comp;
    int64 pos = bitPos;
    int64 n = 0;

    while (n < count)
    {
        if (pos + 8 > end)
            return std::nullopt;
        const int k = b.byteAt (pos);
        if (k < 1 || k > maxK)
            return std::nullopt;
        pos += 8;
        const int64 take = juce::jmin ((int64) blockSamples, count - n);
        if (pos + take * k > end)
            return std::nullopt;
        for (int64 i = 0; i < take; ++i, ++n)
            out[(size_t) n] = b.readSM (pos + i * k, k);
        pos += take * k;
    }

    return pos == end ? std::optional<std::vector<int>> (std::move (out)) : std::nullopt;
}

struct BlockRun
{
    std::vector<int> samples;
    int64 endPos = 0;
};

// V1 blocks from `pos` until the stream stops looking like blocks.
BlockRun decodeBlocks (const Bits& b, int64 bitLen, int64 pos)
{
    BlockRun r;
    while (pos + 8 <= bitLen)
    {
        const int k = b.byteAt (pos);
        if (k < 1 || k > maxK)
            break;
        pos += 8;
        int64 n = blockSamples;
        if (pos + n * k > bitLen)
            n = (bitLen - pos) / k;
        if (n <= 0)
            break;
        for (int64 i = 0; i < n; ++i)
        {
            r.samples.push_back (b.readSM (pos, k));
            pos += k;
        }
    }
    r.endPos = pos;
    return r;
}

struct TailCount
{
    int64 count = 0;
    int64 endPos = 0;
};

TailCount tailCount (const Bits& b, int64 bitLen, int64 t)
{
    int64 pos = t, cnt = 0;
    while (pos + 8 <= bitLen)
    {
        const int k = b.byteAt (pos);
        if (k < 1 || k > maxK)
            break;
        pos += 8;
        int64 n = blockSamples;
        if (pos + n * k > bitLen)
            n = (bitLen - pos) / k;
        if (n <= 0)
            break;
        pos += n * k;
        cnt += n;
    }
    return { cnt, pos };
}

//==============================================================================
// Containers
juce::MemoryBlock inflate (const void* data, size_t size, juce::GZIPDecompressorInputStream::Format format)
{
    juce::MemoryInputStream in (data, size, false);
    juce::GZIPDecompressorInputStream gz (&in, false, format);
    juce::MemoryBlock out;
    gz.readIntoMemoryBlock (out);
    return out;
}

bool startsWithVC2 (const juce::MemoryBlock& m)
{
    const auto* p = static_cast<const juce::uint8*> (m.getData());
    return m.getSize() >= 8 && p[0] == 'V' && p[1] == 'C' && p[2] == '2' && p[3] == '!';
}

// Attributes of the single <trigger_instrument ...> element, as name="value" pairs.
std::map<juce::String, juce::String> parseInstrumentAttributes (const juce::String& xml)
{
    std::map<juce::String, juce::String> attrs;
    const int start = xml.indexOf ("<trigger_instrument");
    if (start < 0)
        return attrs;
    const int end = xml.indexOfChar (start, '>');
    if (end < 0)
        return attrs;

    const auto body = xml.substring (start + 19, end);
    int pos = 0;
    while (true)
    {
        const int eq = body.indexOf (pos, "=\"");
        if (eq < 0)
            break;
        int nameStart = eq;
        while (nameStart > pos && ! juce::CharacterFunctions::isWhitespace (body[nameStart - 1]))
            --nameStart;
        const int valueEnd = body.indexOfChar (eq + 2, '"');
        if (valueEnd < 0)
            break;
        const auto name = body.substring (nameStart, eq);
        if (name.isNotEmpty())
            attrs[name] = body.substring (eq + 2, valueEnd);
        pos = valueEnd + 1;
    }
    return attrs;
}

std::optional<int64> intAttr (const std::map<juce::String, juce::String>& attrs, const juce::String& key)
{
    const auto it = attrs.find (key);
    if (it == attrs.end())
        return std::nullopt;
    const auto s = it->second.trimStart();
    if (s.isEmpty() || ! (juce::CharacterFunctions::isDigit (s[0]) || ((s[0] == '-' || s[0] == '+') && s.length() > 1)))
        return std::nullopt;
    return s.getLargeIntValue();
}

// Trigger 2 container: magic + audio_len(u32 LE @64) + 0 + fmt + blob + footer(8 + zlib("VC2!" + XML)).
std::optional<std::vector<RawWave>> parseV2 (const juce::uint8* u8, size_t len)
{
    if (len < 76 + 8)
        return std::nullopt;

    const auto audioLen = (size_t) readU32 (u8 + 64, true);
    const size_t blobStart = 76;
    const size_t blobEnd = juce::jmin (len, blobStart + audioLen);
    if (blobStart + audioLen + 8 >= len)
        return std::nullopt;

    const auto* foot = u8 + blobStart + audioLen + 8;
    const auto footLen = len - (blobStart + audioLen + 8);

    auto dec = inflate (foot, footLen, juce::GZIPDecompressorInputStream::zlibFormat);
    if (! startsWithVC2 (dec))
    {
        // Some streams are raw deflate; strip a zlib wrapper if one is there.
        const bool wrapped = footLen > 6 && foot[0] == 0x78;
        dec = wrapped ? inflate (foot + 2, footLen - 6, juce::GZIPDecompressorInputStream::deflateFormat)
                      : inflate (foot, footLen, juce::GZIPDecompressorInputStream::deflateFormat);
    }
    if (! startsWithVC2 (dec))
        return std::nullopt;

    const auto* text = static_cast<const char*> (dec.getData()) + 8;
    const auto textLen = dec.getSize() - 8;
    juce::String xml;
    if (juce::CharPointer_UTF8::isValidString (text, (int) textLen))
    {
        xml = juce::String::fromUTF8 (text, (int) textLen);
    }
    else
    {
        // latin-1 fallback, one byte per character
        juce::String latin;
        latin.preallocateBytes (textLen * 2);
        for (size_t i = 0; i < textLen; ++i)
            latin += (juce::juce_wchar) (juce::uint8) text[i];
        xml = latin;
    }

    const auto attrs = parseInstrumentAttributes (xml);
    const auto count = intAttr (attrs, "data_count");
    if (! count || *count <= 0 || *count > 4096)
        return std::nullopt;

    const Bits blob { u8 + blobStart, blobEnd - blobStart };
    std::vector<RawWave> waves;
    size_t off = 0;

    for (int64 i = 0; i < *count; ++i)
    {
        const auto prefix = "wd" + juce::String (i);
        const auto comp = intAttr (attrs, prefix + "comp1");
        const auto frames = intAttr (attrs, prefix + "frames");
        if (! comp || ! frames || *comp < 0 || *frames < 0 || *frames > maxFrames)
            return std::nullopt;

        RawWave w;
        w.comp = *comp;
        w.frames = *frames;
        const auto st = attrs.find (prefix + "stereo");
        w.stereo = st != attrs.end() ? st->second : juce::String ("?");
        const auto nb = (size_t) ((*comp + 7) / 8);
        const auto sub = blob.from (off);
        w.blob = { sub.d, juce::jmin (nb, sub.n) };
        off += nb;
        waves.push_back (std::move (w));
    }
    return waves;
}

// Trigger Instrument Editor files: "TRIGGER COMPRESSED INSTRUMENT", 128-byte header,
// then [01][comp LE][frames LE][V1 blocks] waves joined by [05][span] gap records.
std::optional<std::vector<RawWave>> parseEditor (const juce::uint8* u8, size_t len)
{
    if (len < 128 || std::memcmp (u8, "TRIGGER ", 8) != 0)
        return std::nullopt;

    juce::String tag;
    for (size_t i = 8; i < 64 && u8[i] != 0; ++i)
        tag += (char) u8[i];
    if (! tag.startsWith ("COMPRESSED"))
        return std::nullopt;

    const Bits all { u8, len };
    std::vector<RawWave> waves;
    size_t pos = 128;

    for (int w = 0; w < 256; ++w)
    {
        if (pos + 9 > len || u8[pos] != 0x01)
            break;
        const auto comp = (int64) readU32 (u8 + pos + 1, true);
        const auto frames = (int64) readU32 (u8 + pos + 5, true);
        if (! (comp > 0 && frames > 1 && frames < 10000000))
            break;
        const size_t base = pos + 9;
        const auto blen = (size_t) ((comp + 7) / 8);
        if (base + blen > len)
            break;
        auto samples = v1Stream (all, (int64) base * 8, comp, frames - 1);
        if (! samples)
            break;

        RawWave rw;
        rw.comp = comp;
        rw.frames = frames;
        rw.stereo = "0-ed";
        rw.hasV1 = true;
        rw.v1 = std::move (*samples);
        waves.push_back (std::move (rw));

        pos = base + blen;
        if (pos + 8 > len || u8[pos] != 0x05) // 0x06 = params footer to EOF
            break;
        pos += 8;
    }

    if (waves.empty())
        return std::nullopt;
    return waves;
}

// Single V1 wave: [01][comp u32][frames u32] + V1 stream (big- then little-endian header).
std::optional<std::vector<RawWave>> parseV1 (const juce::uint8* u8, size_t len)
{
    if (len < 9 || u8[0] != 0x01)
        return std::nullopt;

    const Bits all { u8, len };
    for (const bool le : { false, true })
    {
        const auto comp = (int64) readU32 (u8 + 1, le);
        const auto frames = (int64) readU32 (u8 + 5, le);
        if (! (comp > 0 && comp <= 8 * (int64) (len - 9)))
            continue;
        if (! (frames > 1 && frames < 10000000))
            continue;
        if (auto samples = v1Stream (all, 9 * 8, comp, frames - 1))
        {
            RawWave rw;
            rw.comp = comp;
            rw.frames = frames;
            rw.stereo = "0-x";
            rw.hasV1 = true;
            rw.v1 = std::move (*samples);
            return std::vector<RawWave> { std::move (rw) };
        }
    }
    return std::nullopt;
}

//==============================================================================
// Mono wave solver (port of solve.js)
struct Spec
{
    enum class Kind { none, raw, blk, blocks, tail8 };
    Kind kind = Kind::none;
    int nraw = 0;
    int64 s = 0, t = 0;
    int k = 0, c = 0;
    std::vector<std::pair<int, int>> blocks; // (k, count)
    char grade = 'X';
    juce::String note;
};

// Width bytes seen in mono attack-block heads -> bit width actually used.
int kmap (int b) noexcept
{
    switch (b)
    {
        case 145: case 149: case 29: case 62: case 75: return 23;
        case 142: case 129: case 191: case 20: return 20;
        case 22: case 140: return 21;
        case 24: return 24;
        default: return -1;
    }
}

struct RunHit
{
    double run;
    int64 pos;
};

std::vector<RunHit> runScan (const Bits& b, int64 bitLen, int64 lo, int64 hi, double minRun, int count)
{
    std::vector<RunHit> out;
    const int64 end = juce::jmin (hi, bitLen - 8);
    for (int64 pos = lo; pos < end; ++pos)
    {
        const int k0 = b.byteAt (pos);
        if (! (k0 >= 1 && k0 <= maxK))
            continue;
        double run = 0;
        int64 p = pos;
        while (p + 8 <= bitLen)
        {
            const int k = b.byteAt (p);
            if (! (k >= 1 && k <= maxK))
                break;
            if (p + 8 + (int64) count * k > bitLen)
            {
                run += 0.5;
                break;
            }
            p += 8 + (int64) count * k;
            run += 1;
            if (run > 600)
                break;
        }
        if (run >= minRun)
            out.push_back ({ run, pos });
    }
    std::stable_sort (out.begin(), out.end(), [] (const RunHit& a, const RunHit& c) { return a.run > c.run; });
    return out;
}

double smoothScore (const std::vector<int>& seg, int count, int k)
{
    if (count < 8)
        return 9.0;
    double s = 0;
    for (int i = 1; i < count; ++i)
        s += std::abs ((double) seg[(size_t) i] - (double) seg[(size_t) i - 1]);
    return (s / (count - 1)) / std::pow (2.0, k - 1);
}

void decodeSpan (const Bits& b, int64 pos, int k, int c, std::vector<int>& out)
{
    if ((int) out.size() < c)
        out.resize ((size_t) c);
    for (int i = 0; i < c; ++i, pos += k)
        out[(size_t) i] = b.readSM (pos, k);
}

bool tailEndOk (int64 endPos, int64 bitLen, int64 tol = 64) noexcept { return endPos >= bitLen - tol; }

Spec solveWave (const Bits& w, int64 fr)
{
    const int64 bitLen = w.bitLength();

    // 1) raw 24-bit head of 200 samples + V1 tail
    {
        int raw[200];
        for (int i = 0; i < 200; ++i)
            raw[i] = sm24 (w.byteAtIndex ((size_t) (3 * i)), w.byteAtIndex ((size_t) (3 * i + 1)), w.byteAtIndex ((size_t) (3 * i + 2)));
        double rmad = 0;
        for (int i = 1; i < 200; ++i)
            rmad += std::abs ((double) raw[i] - raw[i - 1]);
        rmad = (rmad / 199.0) / std::pow (2.0, 23);
        if (rmad < 0.25)
        {
            const auto tail = w.from (600);
            const auto t = decodeBlocks (tail, tail.bitLength(), 0);
            const int64 over = 200 + (int64) t.samples.size() - fr;
            if (over >= -2 && tailEndOk (4800 + t.endPos, bitLen))
            {
                Spec s;
                s.kind = Spec::Kind::raw;
                s.nraw = 200;
                s.grade = 'A';
                s.note = "raw200 over=" + juce::String (over);
                return s;
            }
        }
    }

    // 2) several identity block chains from bit 0
    {
        struct Blk { int k; int c; int64 p; };
        std::vector<Blk> blks;
        int64 p = 0;
        while (p + 8 <= bitLen && blks.size() < 12)
        {
            const int k = w.byteAt (p);
            if (! (k >= 1 && k <= maxK) || p + 8 + (int64) blockSamples * k > bitLen)
                break;
            blks.push_back ({ k, blockSamples, p });
            p += 8 + (int64) blockSamples * k;
        }
        for (int n = juce::jmin ((int) blks.size(), 12); n >= 2; --n)
        {
            const auto& last = blks[(size_t) n - 1];
            const int64 tp = last.p + 8 + (int64) blockSamples * last.k;
            const auto t = tailCount (w, bitLen, tp);
            const int64 over = (int64) blockSamples * n + t.count - fr;
            if (over >= -2 && over <= 44 && tailEndOk (t.endPos, bitLen))
            {
                Spec s;
                s.kind = Spec::Kind::blocks;
                for (int i = 0; i < n; ++i)
                    s.blocks.push_back ({ blks[(size_t) i].k, blks[(size_t) i].c });
                s.t = tp;
                s.grade = 'A';
                s.note = juce::String (n) + "x identity blocks over=" + juce::String (over);
                return s;
            }
        }
    }

    // 3) single attack block fits in front of a V1 tail
    struct Cand { bool bonus; int64 s; int64 absOver; int c; int k; int64 t; int b; double sc; };
    std::vector<Cand> cands, k1cands;
    std::vector<int> span;

    for (const auto& hit : runScan (w, bitLen, 0, 60000, 10, blockSamples))
    {
        const int64 t = hit.pos;
        const auto tc = tailCount (w, bitLen, t);
        if (! tailEndOk (tc.endPos, bitLen))
            continue;
        const int64 lo = juce::jmax ((int64) 8, fr - tc.count - 8);
        for (int64 c = lo; c <= fr - tc.count + 44; ++c)
        {
            for (int k = 1; k <= maxK; ++k)
            {
                const int64 s = t - 8 - c * k;
                if (s < 0 || s > 6000)
                    continue;
                decodeSpan (w, s + 8, k, (int) c, span);
                const double sc = smoothScore (span, (int) c, k);
                if (sc >= 0.15)
                    continue;
                const int b = w.byteAt (s);
                const int64 absOver = std::abs (c + tc.count - fr);
                if (k == 1)
                    k1cands.push_back ({ false, s, absOver, (int) c, 1, t, b, sc });
                else
                    cands.push_back ({ b == k || kmap (b) == k, s, absOver, (int) c, k, t, b, sc });
            }
        }
    }

    std::optional<Spec> best;
    if (! cands.empty())
    {
        std::stable_sort (cands.begin(), cands.end(), [] (const Cand& a, const Cand& b)
        {
            if (a.bonus != b.bonus) return a.bonus;
            if (a.s != b.s) return a.s < b.s;
            return a.absOver < b.absOver;
        });
        const auto& c = cands.front();
        const int64 over = c.c + tailCount (w, bitLen, c.t).count - fr;
        Spec s;
        s.kind = Spec::Kind::blk;
        s.s = c.s; s.k = c.k; s.c = c.c; s.t = c.t;
        s.grade = (over == 0 && c.sc < 0.08) ? 'A' : (std::abs (over) <= 2 ? 'B' : 'C');
        s.note = "blk C=" + juce::String (c.c) + " k=" + juce::String (c.k) + " over=" + juce::String (over);
        best = s;
    }
    else if (! k1cands.empty())
    {
        std::stable_sort (k1cands.begin(), k1cands.end(), [] (const Cand& a, const Cand& b)
        {
            if (a.s != b.s) return a.s < b.s;
            return a.absOver < b.absOver;
        });
        const auto& c = k1cands.front();
        Spec s;
        s.kind = Spec::Kind::blk;
        s.s = c.t - 8 - c.c; s.k = 1; s.c = c.c; s.t = c.t;
        s.grade = 'C';
        s.note = "blk k=1 (silence?)";
        best = s;
    }

    // 4) leading zero bytes + V1 tail; preferred over a k=1 "silence" fit
    std::optional<Spec> tailN;
    {
        int z = 0;
        while (z < 4 && w.byteAtIndex ((size_t) z) == 0 && (size_t) z < w.n)
            ++z;
        if (z > 0)
        {
            const int64 t0 = 8 * z;
            const auto t = tailCount (w, bitLen, t0);
            const int64 over = t.count - fr;
            if (over >= -8 && over <= 44 && tailEndOk (t.endPos, bitLen))
            {
                const int kk = w.byteAt (t0);
                if (kk >= 1 && kk <= maxK)
                {
                    decodeSpan (w, t0, kk, blockSamples, span);
                    if (smoothScore (span, blockSamples, kk) < 0.15)
                    {
                        Spec s;
                        s.kind = Spec::Kind::tail8;
                        s.t = t0;
                        s.grade = 'B';
                        s.note = "tail-from-" + juce::String (t0) + " over=" + juce::String (over);
                        tailN = s;
                    }
                }
            }
        }
    }

    if (best && best->kind == Spec::Kind::blk && best->k == 1 && tailN)
        return *tailN;
    if (best)
        return *best;
    if (tailN)
        return *tailN;

    Spec unsolved;
    unsolved.note = "unsolved";
    return unsolved;
}

std::vector<int> decodeWave (const Bits& w, const Spec& spec)
{
    std::vector<int> out;
    switch (spec.kind)
    {
        case Spec::Kind::raw:
        {
            for (int i = 0; i < spec.nraw; ++i)
                out.push_back (sm24 (w.byteAtIndex ((size_t) (3 * i)), w.byteAtIndex ((size_t) (3 * i + 1)), w.byteAtIndex ((size_t) (3 * i + 2))));
            const auto tail = w.from ((size_t) (3 * spec.nraw));
            auto t = decodeBlocks (tail, tail.bitLength(), 0);
            out.insert (out.end(), t.samples.begin(), t.samples.end());
            break;
        }
        case Spec::Kind::tail8:
            out = decodeBlocks (w, w.bitLength(), spec.t).samples;
            break;
        case Spec::Kind::blk:
        {
            for (int i = 0; i < spec.c; ++i)
                out.push_back (w.readSM (spec.s + 8 + (int64) i * spec.k, spec.k));
            auto t = decodeBlocks (w, w.bitLength(), spec.t);
            out.insert (out.end(), t.samples.begin(), t.samples.end());
            break;
        }
        case Spec::Kind::blocks:
        {
            int64 p = 0;
            for (const auto& [k, c] : spec.blocks)
            {
                p += 8;
                for (int i = 0; i < c; ++i, p += k)
                    out.push_back (w.readSM (p, k));
            }
            auto t = decodeBlocks (w, w.bitLength(), spec.t);
            out.insert (out.end(), t.samples.begin(), t.samples.end());
            break;
        }
        case Spec::Kind::none:
            break;
    }
    return out;
}

//==============================================================================
// Stereo waves (port of stereo.js): one joint stream of 2*frames samples, L on even
// global indices and R on odd ones; tail = V1 block chain reaching the bit budget.
constexpr int64 headScanBits = 65536;
constexpr int64 headMaxSamples = 4096;
constexpr int64 tailToleranceBits = 64;

struct Walk
{
    bool ok = false;
    int64 count = 0, end = 0;
};

Walk walkTail (const Bits& b, int64 start, int64 comp, int64 cap, int64 tol)
{
    int64 pos = start, count = 0;
    auto nearEnd = [&] { return pos >= comp - tol ? Walk { true, count, pos } : Walk {}; };
    while (pos + 8 <= comp)
    {
        const int k = b.byteAt (pos);
        if (k < 1 || k > maxK)
            return nearEnd();
        pos += 8;
        int64 n = blockSamples;
        if (pos + n * k > comp)
            n = (comp - pos) / k;
        if (n <= 0)
            return nearEnd();
        pos += n * k;
        count += n;
        if (count > cap)
            return {};
    }
    return nearEnd();
}

struct StereoResult
{
    std::vector<int> left, right;
    int64 headSamples = 0;
    int64 tailStart = 0;
};

std::optional<StereoResult> decodeStereo (const Bits& b, int64 comp, int64 frames)
{
    if (comp <= 0 || frames <= 0 || frames > maxFrames)
        return std::nullopt;

    const int64 total = 2 * frames;
    const int64 scanEnd = juce::jmin (juce::jmin (headScanBits, comp), b.bitLength());
    bool found = false;
    int64 bestHead = 0, bestT = 0, bestCount = 0, bestEnd = 0;

    for (int64 t = 0; t + 8 <= scanEnd; ++t)
    {
        const int k = b.byteAt (t);
        if (k < 1 || k > maxK)
            continue;
        const auto w = walkTail (b, t, comp, total, tailToleranceBits);
        if (! w.ok)
            continue;
        const int64 head = total - w.count;
        if (head < 0 || head > headMaxSamples)
            continue;
        if (! found || head < bestHead)
        {
            found = true;
            bestHead = head; bestT = t; bestCount = w.count; bestEnd = w.end;
        }
    }
    if (! found)
        return std::nullopt;

    // joint V1 stream between tail start and chain end
    std::vector<int> joint ((size_t) bestCount);
    {
        int64 pos = bestT, n = 0;
        while (pos + 8 <= bestEnd && n < bestCount)
        {
            const int k = b.byteAt (pos);
            if (k < 1 || k > maxK)
                break;
            pos += 8;
            const int64 m = juce::jmin ((int64) blockSamples, bestCount - n, (bestEnd - pos) / k);
            for (int64 i = 0; i < m; ++i, ++n)
                joint[(size_t) n] = b.readSM (pos + i * k, k);
            pos += m * k;
        }
    }

    StereoResult r;
    r.left.assign ((size_t) frames, 0);
    r.right.assign ((size_t) frames, 0);
    for (int64 j = 0; j < frames; ++j)
    {
        const int64 i = 2 * j - bestHead;
        if (i >= 0 && i < bestCount)
            r.left[(size_t) j] = joint[(size_t) i];
        if (i + 1 >= 0 && i + 1 < bestCount)
            r.right[(size_t) j] = joint[(size_t) (i + 1)];
    }
    r.headSamples = bestHead;
    r.tailStart = bestT;
    return r;
}

//==============================================================================
float attackPeakOf (const juce::AudioBuffer<float>& buf)
{
    const int n = juce::jmin (buf.getNumSamples(), attackWindow);
    float m = 0.0f;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        m = juce::jmax (m, buf.getMagnitude (ch, 0, n));
    return m;
}

// Voice rule: the voice plays exactly `frames` samples (truncate / zero-pad).
juce::AudioBuffer<float> toBuffer (const std::vector<int>& mono, int64 frames)
{
    juce::AudioBuffer<float> buf (1, (int) frames);
    buf.clear();
    auto* d = buf.getWritePointer (0);
    const auto n = (int64) juce::jmin ((int64) mono.size(), frames);
    for (int64 i = 0; i < n; ++i)
        d[i] = (float) mono[(size_t) i] / fullScale;
    return buf;
}

std::optional<Instrument> decodeWaves (std::vector<RawWave>& raw, const juce::String& container, juce::String& error)
{
    Instrument inst;
    inst.container = container;

    for (size_t i = 0; i < raw.size(); ++i)
    {
        auto& rw = raw[i];
        Wave wave;
        wave.index = (int) i;

        if (rw.frames <= 0)
        {
            ++inst.skippedWaves;
            continue;
        }

        if (rw.stereo.startsWith ("1"))
        {
            auto st = decodeStereo (rw.blob, rw.comp, rw.frames);
            if (! st)
            {
                ++inst.skippedWaves;
                continue;
            }
            wave.audio.setSize (2, (int) rw.frames);
            for (int64 j = 0; j < rw.frames; ++j)
            {
                wave.audio.setSample (0, (int) j, (float) st->left[(size_t) j] / fullScale);
                wave.audio.setSample (1, (int) j, (float) st->right[(size_t) j] / fullScale);
            }
            wave.silentHeadFrames = (int) ((st->headSamples + 1) / 2);
            wave.note = "stereo tail@" + juce::String (st->tailStart);
            ++inst.stereoWaves;
        }
        else if (rw.hasV1)
        {
            wave.audio = toBuffer (rw.v1, rw.frames);
            wave.note = "V1";
        }
        else
        {
            const auto spec = solveWave (rw.blob, rw.frames);
            if (spec.kind == Spec::Kind::none)
            {
                ++inst.skippedWaves;
                continue;
            }
            wave.audio = toBuffer (decodeWave (rw.blob, spec), rw.frames);
            wave.note = spec.note;
        }

        wave.attackPeak = attackPeakOf (wave.audio);
        inst.waves.push_back (std::move (wave));
    }

    if (inst.waves.empty())
    {
        error = "no wave in the file could be decoded";
        return std::nullopt;
    }

    std::vector<float> peaks;
    for (const auto& w : inst.waves)
        peaks.push_back (w.attackPeak);
    inst.layers = groupIntoLayers (peaks);
    return inst;
}
} // namespace

//==============================================================================
bool hasTciExtension (const juce::File& file)
{
    return file.hasFileExtension (".tci");
}

std::vector<std::vector<int>> groupIntoLayers (const std::vector<float>& peaks, float rrRatio)
{
    std::vector<int> ranked (peaks.size());
    for (size_t i = 0; i < ranked.size(); ++i)
        ranked[i] = (int) i;
    std::stable_sort (ranked.begin(), ranked.end(), [&] (int a, int b) { return peaks[(size_t) a] < peaks[(size_t) b]; });

    std::vector<std::vector<int>> groups;
    std::vector<int> cur;
    float curMax = 0.0f;
    for (const int idx : ranked)
    {
        const float pk = peaks[(size_t) idx];
        if (! cur.empty() && pk / juce::jmax (curMax, 1.0e-9f) >= rrRatio)
        {
            groups.push_back (cur);
            cur.clear();
            curMax = 0.0f;
        }
        cur.push_back (idx);
        curMax = juce::jmax (curMax, pk);
    }
    if (! cur.empty())
        groups.push_back (cur);
    return groups;
}

std::optional<Instrument> decodeMemory (const void* data, size_t size, juce::String& error)
{
    try
    {
        const auto* u8 = static_cast<const juce::uint8*> (data);
        if (auto waves = parseV2 (u8, size))
            return decodeWaves (*waves, "V2", error);
        if (auto waves = parseV1 (u8, size))
            return decodeWaves (*waves, "V1", error);
        if (auto waves = parseEditor (u8, size))
            return decodeWaves (*waves, "Editor", error);
        error = "not a recognised TCI file (V1, V2 or Instrument Editor)";
    }
    catch (const std::exception& e)
    {
        error = "TCI decode failed: " + juce::String (e.what());
    }
    return std::nullopt;
}

std::optional<Instrument> decodeFile (const juce::File& file, juce::String& error)
{
    juce::MemoryBlock data;
    if (! file.loadFileAsData (data))
    {
        error = "could not read " + file.getFullPathName();
        return std::nullopt;
    }
    return decodeMemory (data.getData(), data.getSize(), error);
}
} // namespace tci
