#pragma once
#include <cstddef>
#include <utility>

namespace dualdeck
{
// A callback-local pin. Releasing a view only updates an atomic page counter.
struct PcmReadView
{
    const float* data = nullptr;
    int first = 0, count = 0;
    void* token = nullptr;
    void (*unpin)(void*) noexcept = nullptr;
    PcmReadView() = default;
    PcmReadView(const float* values, int start, int size, void* value = nullptr,
                void (*release)(void*) noexcept = nullptr) noexcept
        : data(values), first(start), count(size), token(value), unpin(release) {}
    PcmReadView(const PcmReadView&) = delete;
    PcmReadView& operator=(const PcmReadView&) = delete;
    PcmReadView(PcmReadView&& other) noexcept { *this = std::move(other); }
    PcmReadView& operator=(PcmReadView&& other) noexcept
    {
        if (this != &other)
        {
            reset(); data = other.data; first = other.first; count = other.count;
            token = other.token; unpin = other.unpin;
            other.data = nullptr; other.token = nullptr; other.unpin = nullptr;
        }
        return *this;
    }
    ~PcmReadView() { reset(); }
    void reset() noexcept
    {
        if (token && unpin) unpin(token);
        data = nullptr; token = nullptr; unpin = nullptr; count = 0;
    }
    bool contains(int frame) const noexcept { return data && frame >= first && frame - first < count; }
    bool read(int frame, float& left, float& right) const noexcept
    {
        if (!contains(frame)) return false;
        const auto index = static_cast<std::size_t>(frame - first) * 2;
        left = data[index]; right = data[index + 1]; return true;
    }
};
class StreamedPcm
{
public:
    virtual ~StreamedPcm() = default;
    virtual int frameCount() const noexcept = 0;
    virtual double rate() const noexcept = 0;
    virtual std::size_t prefixBytes() const noexcept = 0;
    virtual PcmReadView acquire(int frame) const noexcept = 0;
    virtual void request(int first, int count) const noexcept = 0;
    virtual bool failed() const noexcept = 0;
};
}
