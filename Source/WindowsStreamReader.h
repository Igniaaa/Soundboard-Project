#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace dualdeck
{
// Create, read, and destroy on the same background thread. This reader owns
// that thread's COM/MF startup and retains at most one decoded output block.
std::unique_ptr<juce::AudioFormatReader> createWindowsStreamReader(const juce::File& file,
                                                                juce::String& error);
}
