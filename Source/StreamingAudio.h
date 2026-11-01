#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "StreamingPcm.h"
#include <memory>
#include <atomic>
#include <vector>

namespace dualdeck
{
// All decoders and page recycling run off the audio callbacks. A source can be
// read simultaneously by independent voices and by the two output devices.
class StreamingAudio
{
public:
    static constexpr int pageFrames = 8192;
    static constexpr std::size_t defaultPageBudget = 64u * 1024u * 1024u;
    explicit StreamingAudio(std::size_t pageBudget = defaultPageBudget);
    ~StreamingAudio();
    juce::Result prepare(const juce::File&, double startSeconds, std::shared_ptr<StreamedPcm>&);
    std::size_t pageBytes() const noexcept;
    juce::String takeError();
    juce::String statistics() const;
    static bool waitForRange(const std::shared_ptr<StreamedPcm>&, int first, int count, int timeoutMs = 5000);
private:
    struct Impl;
    std::vector<std::shared_ptr<Impl>> workers;
    std::atomic<unsigned> nextWorker{0};
};
}
