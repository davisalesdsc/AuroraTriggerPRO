#pragma once

// Decoder for Trigger ".tci" instrument files (Trigger Compressed Instrument).
//
// The format is proprietary and undocumented. This is a C++ port of the clean-room
// reverse-engineered decoder "tci2wav" by Aidan Scroggins (MIT License,
// https://github.com/AidanScroggins/tci2wav). See THIRD_PARTY_NOTICES.md.
//
// What decodes:
//   - V2 containers (Trigger 2 libraries): mono waves bit-exact (raw / block heads are
//     solved structurally); stereo waves decode their tail exactly, but the first
//     ~2 ms attack head of a stereo wave uses a path nobody has cracked yet, so those
//     frames are left silent (reported via Wave::silentHeadFrames).
//   - V1 single-wave files and files written by Slate's Trigger Instrument Editor.
//
// Waves are grouped into velocity layers by attack peak (soft -> loud); waves whose
// peaks are within 12% of each other share a layer and become round-robins.

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <optional>
#include <vector>

namespace tci
{
struct Wave
{
    int index = 0;                      // position in the file
    juce::AudioBuffer<float> audio;     // 1 or 2 channels, 44.1 kHz
    float attackPeak = 0.0f;            // peak over the first 5000 frames (0..1)
    int silentHeadFrames = 0;           // stereo only: undecodable attack frames left silent
    juce::String note;                  // how it was decoded
};

struct Instrument
{
    juce::String container;                 // "V2", "V1" or "Editor"
    double sampleRate = 44100.0;
    std::vector<Wave> waves;                // decoded waves, file order
    std::vector<std::vector<int>> layers;   // [velocity layer soft->loud][round robin] -> index into waves
    int skippedWaves = 0;                   // waves no parse could explain
    int stereoWaves = 0;
};

// True when the file has a .tci extension.
bool hasTciExtension (const juce::File& file);

// Decode a whole .tci file. Returns nothing (and sets `error`) when the file is not a
// recognised TCI or no wave could be decoded.
std::optional<Instrument> decodeFile (const juce::File& file, juce::String& error);
std::optional<Instrument> decodeMemory (const void* data, size_t size, juce::String& error);

// Velocity layering shared with plain sample folders: sorts by attack peak, groups
// peaks closer than `rrRatio` into the same layer.
std::vector<std::vector<int>> groupIntoLayers (const std::vector<float>& peaks, float rrRatio = 1.12f);
} // namespace tci
