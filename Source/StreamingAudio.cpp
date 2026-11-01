#include "StreamingAudio.h"
#include "WindowsStreamReader.h"
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <map>
#include <mutex>
#include <vector>

namespace dualdeck
{
namespace
{
std::unique_ptr<juce::AudioFormatReader> openReader(const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    return reader ? std::move(reader) : createWindowsStreamReader(file, error);
}
// Bounded multi-producer queue. Requests use weak source references so evicting
// a prepared entry does not retain its head behind a backlog of obsolete reads.
template<class T, std::size_t Capacity> class WorkQueue
{
    struct Cell { std::atomic<std::size_t> sequence{0}; T value{}; };
    std::array<Cell, Capacity> cells;
    std::atomic<std::size_t> enqueue{0}, dequeue{0};
public:
    WorkQueue() { for (std::size_t i = 0; i < Capacity; ++i) cells[i].sequence.store(i); }
    bool push(const T& value) noexcept
    {
        auto position = enqueue.load(std::memory_order_relaxed);
        for (int attempts = 0; attempts < 8; ++attempts)
        {
            auto& cell = cells[position % Capacity];
            const auto difference = static_cast<std::intptr_t>(cell.sequence.load(std::memory_order_acquire)) - static_cast<std::intptr_t>(position);
            if (difference == 0)
            {
                if (!enqueue.compare_exchange_weak(position, position + 1, std::memory_order_relaxed)) continue;
                cell.value = value; cell.sequence.store(position + 1, std::memory_order_release); return true;
            }
            if (difference < 0) return false;
            position = enqueue.load(std::memory_order_relaxed);
        }
        return false;
    }
    bool pop(T& value) noexcept
    {
        const auto position = dequeue.load(std::memory_order_relaxed);
        auto& cell = cells[position % Capacity];
        if (cell.sequence.load(std::memory_order_acquire) != position + 1) return false;
        value = std::move(cell.value); cell.value = T{};
        cell.sequence.store(position + Capacity, std::memory_order_release);
        dequeue.store(position + 1, std::memory_order_relaxed); return true;
    }
};
}

struct StreamingAudio::Impl : juce::Thread, std::enable_shared_from_this<Impl>
{
    struct Source;
    static constexpr unsigned writing = 0x80000000u;
    struct Page
    {
        std::atomic<unsigned> users{0};
        std::atomic<std::uint64_t> used{0};
        std::uint64_t owner = 0;
        int number = -1, frames = 0;
        std::weak_ptr<Source> source;
        std::unique_ptr<float[]> values{new float[pageFrames * 2]};
    };
    struct Request { std::weak_ptr<Source> source; int page = 0; };
    struct Store
    {
        std::vector<std::unique_ptr<Page>> pool;
        std::atomic<std::size_t> allocated{0};
        std::atomic<std::uint64_t> access{0};
        const std::size_t maximum;
        explicit Store(std::size_t budget) : maximum(std::max<std::size_t>(2, budget / (pageFrames * 2 * sizeof(float)))) {}
    };
    struct Source final : StreamedPcm, std::enable_shared_from_this<Source>
    {
        std::weak_ptr<Impl> engine;
        std::shared_ptr<Store> storage; // Keeps page memory alive without a worker/source cycle.
        juce::File file;
        juce::int64 fileSize = 0, modified = 0;
        std::uint64_t id = 0;
        int total = 0, prefixFirst = 0, prefixFrames = 0, pages = 0;
        double sampleRate = 0;
        std::vector<float> prefix;
        std::unique_ptr<std::atomic<Page*>[]> table;
        std::unique_ptr<std::atomic<bool>[]> pending;
        std::atomic<bool> broken{false};
        int frameCount() const noexcept override { return total; }
        double rate() const noexcept override { return sampleRate; }
        std::size_t prefixBytes() const noexcept override { return prefix.size() * sizeof(float); }
        bool failed() const noexcept override { return broken.load(std::memory_order_acquire); }
        static void releasePage(void* token) noexcept
        { static_cast<Page*>(token)->users.fetch_sub(1, std::memory_order_release); }
        PcmReadView acquire(int frame) const noexcept override
        {
            if (frame < 0 || frame >= total) return {};
            if (frame >= prefixFirst && frame - prefixFirst < prefixFrames)
                return {prefix.data(), prefixFirst, prefixFrames};
            const int number = frame / pageFrames;
            auto* page = table[number].load(std::memory_order_acquire);
            if (!page) return {};
            auto users = page->users.load(std::memory_order_relaxed);
            for (int attempt = 0; attempt < 4 && (users & writing) == 0; ++attempt)
            {
                if (!page->users.compare_exchange_weak(users, users + 1, std::memory_order_acquire, std::memory_order_relaxed)) continue;
                if (page->owner != id || page->number != number)
                { releasePage(page); return {}; }
                page->used.store(storage->access.fetch_add(1, std::memory_order_relaxed) + 1, std::memory_order_relaxed);
                return {page->values.get(), number * pageFrames, page->frames, page, releasePage};
            }
            return {};
        }
        void request(int first, int count) const noexcept override
        {
            if (count <= 0 || failed()) return;
            const auto worker = engine.lock();
            if (!worker || !worker->accepting.load(std::memory_order_acquire)) return;
            const int begin = std::clamp(first, 0, total - 1) / pageFrames;
            const int end = static_cast<int>(std::min<std::int64_t>(total - 1, static_cast<std::int64_t>(std::max(0, first)) + count - 1)) / pageFrames;
            for (int page = begin; page <= end; ++page)
            {
                const int start = page * pageFrames;
                const int size = std::min(pageFrames, total - start);
                if (start >= prefixFirst && start + size <= prefixFirst + prefixFrames) continue;
                if (table[page].load(std::memory_order_acquire) != nullptr) continue;
                bool expected = false;
                // Queue only the nearest missing page per request. A long
                // lookahead must not put an entire sound ahead of the next
                // voice's imminent page. The next callback advances this
                // window once the worker has published the page.
                if (!pending[page].compare_exchange_strong(expected, true, std::memory_order_acq_rel)) break;
                if (!worker->queue.push({const_cast<Source*>(this)->weak_from_this(), page}))
                    pending[page].store(false, std::memory_order_release);
                break;
            }
        }
    };
    struct Reader
    {
        std::weak_ptr<Source> source;
        std::unique_ptr<juce::AudioFormatReader> decoder;
        std::uint64_t used = 0;
    };
    WorkQueue<Request, 4096> queue;
    std::shared_ptr<Store> storage;
    std::map<std::uint64_t, Reader> readers;
    std::atomic<bool> accepting{true};
    std::atomic<std::uint64_t> nextId{1};
    std::uint64_t clock = 0;
    std::atomic<unsigned> opened{0}, filled{0};
    std::atomic<double> openMs{0}, readMs{0};
    std::mutex errorLock;
    juce::String error;
    juce::AudioBuffer<float> scratch{2, pageFrames};
    explicit Impl(std::size_t budget) : Thread("Streaming audio"), storage(std::make_shared<Store>(budget)) {}
    ~Impl() override { stopThread(-1); }
    void fail(const std::shared_ptr<Source>& source, const juce::String& detail)
    {
        source->broken.store(true, std::memory_order_release);
        const std::lock_guard<std::mutex> lock(errorLock);
        error = source->file.getFileName() + ": " + detail;
    }
    juce::AudioFormatReader* readerFor(const std::shared_ptr<Source>& source)
    {
        auto found = readers.find(source->id);
        if (found != readers.end()) { found->second.used = ++clock; return found->second.decoder.get(); }
        if (!source->file.existsAsFile() || source->file.getSize() != source->fileSize
            || source->file.getLastModificationTime().toMilliseconds() != source->modified)
        { fail(source, "File cambiato o non disponibile. Aggiorna le cartelle."); return nullptr; }
        juce::String detail;
        const auto beforeOpen = juce::Time::getMillisecondCounterHiRes();
        auto decoder = openReader(source->file, detail);
        openMs.store(openMs.load() + juce::Time::getMillisecondCounterHiRes() - beforeOpen);
        opened.fetch_add(1);
        if (!decoder || decoder->sampleRate != source->sampleRate || decoder->lengthInSamples < source->total)
        { fail(source, detail.isNotEmpty() ? detail : "Il formato audio non corrisponde più al file preparato."); return nullptr; }
        if (readers.size() >= 12)
        {
            const auto oldest = std::min_element(readers.begin(), readers.end(),
                [](const auto& a, const auto& b) { return a.second.used < b.second.used; });
            readers.erase(oldest);
        }
        auto* result = decoder.get();
        readers.emplace(source->id, Reader{source, std::move(decoder), ++clock});
        return result;
    }
    Page* writablePage()
    {
        auto& pool = storage->pool;
        if (pool.size() < storage->maximum)
        {
            auto page = std::make_unique<Page>(); page->users.store(writing);
            auto* result = page.get(); pool.push_back(std::move(page));
            storage->allocated.fetch_add(pageFrames * 2 * sizeof(float)); return result;
        }
        Page* best = nullptr;
        for (auto& page : pool)
            if (page->users.load(std::memory_order_acquire) == 0
                && (!best || page->used.load() < best->used.load())) best = page.get();
        if (!best) return nullptr;
        unsigned expected = 0;
        if (!best->users.compare_exchange_strong(expected, writing, std::memory_order_acq_rel)) return nullptr;
        if (auto previous = best->source.lock())
        {
            auto* expectedPage = best;
            previous->table[best->number].compare_exchange_strong(expectedPage, nullptr, std::memory_order_acq_rel);
        }
        return best;
    }
    void fill(const std::shared_ptr<Source>& source, int number)
    {
        if (source->failed()) return;
        if (source->table[number].load(std::memory_order_acquire)) return;
        auto* decoder = readerFor(source);
        if (!decoder) return;
        const int first = number * pageFrames;
        const int count = std::min(pageFrames, source->total - first);
        const auto beforeRead = juce::Time::getMillisecondCounterHiRes();
        const bool ok = decoder->read(&scratch, 0, count, first, true, true);
        readMs.store(readMs.load() + juce::Time::getMillisecondCounterHiRes() - beforeRead);
        if (!ok)
        { fail(source, "Errore di lettura durante la riproduzione."); return; }
        auto* page = writablePage();
        if (!page) return; // Retried by the next callback, never wait for readers.
        for (int frame = 0; frame < count; ++frame) for (int channel = 0; channel < 2; ++channel)
        {
            const float value = scratch.getSample(channel, frame);
            page->values[frame * 2 + channel] = std::isfinite(value) ? value : 0.0f;
        }
        page->owner = source->id; page->number = number; page->frames = count;
        page->source = source; page->used.store(storage->access.fetch_add(1, std::memory_order_relaxed) + 1);
        source->table[number].store(page, std::memory_order_release);
        page->users.store(0, std::memory_order_release);
        filled.fetch_add(1);
    }
    void run() override
    {
        while (!threadShouldExit())
        {
            Request request;
            if (!queue.pop(request)) { wait(1); continue; }
            if (auto source = request.source.lock())
            {
                fill(source, request.page);
                source->pending[request.page].store(false, std::memory_order_release);
            }
            if ((++clock & 63) == 0)
                for (auto it = readers.begin(); it != readers.end();)
                    if (it->second.source.expired()) it = readers.erase(it); else ++it;
        }
        // Media Foundation/COM reader destruction stays on its creation thread.
        readers.clear();
        Request request; while (queue.pop(request)) request = {};
    }
};

StreamingAudio::StreamingAudio(std::size_t budget)
{
    // Independent shards keep each decoder on one background thread while
    // allowing simultaneous first seeks. Split, never multiply, the budget.
    constexpr auto pageBytes = pageFrames * 2 * sizeof(float);
    const unsigned count = budget >= 8 * pageBytes ? 4 : 1;
    const auto pages = budget / pageBytes;
    for (unsigned i = 0; i < count; ++i)
    {
        auto worker = std::make_shared<Impl>((pages / count + (i < pages % count ? 1 : 0)) * pageBytes);
        worker->startThread(); workers.push_back(std::move(worker));
    }
}
StreamingAudio::~StreamingAudio()
{
    for (const auto& worker : workers) { worker->accepting.store(false, std::memory_order_release); worker->signalThreadShouldExit(); }
    for (const auto& worker : workers) worker->stopThread(-1);
}
std::size_t StreamingAudio::pageBytes() const noexcept
{
    std::size_t bytes = 0;
    for (const auto& worker : workers) bytes += worker->storage->allocated.load();
    return bytes;
}
juce::String StreamingAudio::statistics() const
{
    unsigned opened = 0, filled = 0; double openMs = 0, readMs = 0;
    for (const auto& worker : workers)
    { opened += worker->opened.load(); filled += worker->filled.load(); openMs += worker->openMs.load(); readMs += worker->readMs.load(); }
    return "Readers " + juce::String(opened) + " / open time " + juce::String(openMs, 2)
        + " ms; pages " + juce::String(filled) + " / decode time " + juce::String(readMs, 2) + " ms";
}
juce::String StreamingAudio::takeError()
{
    juce::StringArray errors;
    for (const auto& worker : workers)
    {
        const std::lock_guard<std::mutex> lock(worker->errorLock);
        if (worker->error.isNotEmpty()) errors.add(worker->error);
        worker->error.clear();
    }
    return errors.joinIntoString("\n");
}
juce::Result StreamingAudio::prepare(const juce::File& file, double startSeconds, std::shared_ptr<StreamedPcm>& result)
{
    const auto& impl = workers[nextWorker.fetch_add(1, std::memory_order_relaxed) % workers.size()];
    if (!file.existsAsFile()) return juce::Result::fail("File non trovato: " + file.getFileName());
    juce::String error;
    auto reader = openReader(file, error);
    if (!reader) return juce::Result::fail(error.isNotEmpty() ? error : "Formato audio non disponibile.");
    if (!std::isfinite(reader->sampleRate) || reader->sampleRate <= 0 || reader->lengthInSamples <= 0
        || reader->numChannels < 1 || reader->numChannels > 2)
        return juce::Result::fail("Audio vuoto o formato non valido.");
    auto source = std::make_shared<Impl::Source>();
    source->engine = impl; source->storage = impl->storage; source->file = file; source->fileSize = file.getSize();
    source->modified = file.getLastModificationTime().toMilliseconds(); source->id = impl->nextId.fetch_add(1);
    source->sampleRate = reader->sampleRate;
    const double frames = std::min(static_cast<double>(reader->lengthInSamples), reader->sampleRate * 180.0);
    if (frames < 1 || frames > std::numeric_limits<int>::max()) return juce::Result::fail("Durata audio non valida.");
    source->total = static_cast<int>(frames);
    source->prefixFirst = static_cast<int>(std::clamp(std::isfinite(startSeconds) ? startSeconds * source->sampleRate : 0.0, 0.0, static_cast<double>(source->total - 1)));
    source->prefixFrames = std::min(source->total - source->prefixFirst, std::max(1, static_cast<int>(std::ceil(source->sampleRate * 0.5))));
    source->prefix.resize(static_cast<std::size_t>(source->prefixFrames) * 2);
    juce::AudioBuffer<float> head(2, source->prefixFrames);
    if (!reader->read(&head, 0, source->prefixFrames, source->prefixFirst, true, true)) return juce::Result::fail("Lettura iniziale audio non riuscita.");
    for (int frame = 0; frame < source->prefixFrames; ++frame) for (int channel = 0; channel < 2; ++channel)
    {
        const float value = head.getSample(channel, frame);
        source->prefix[frame * 2 + channel] = std::isfinite(value) ? value : 0.0f;
    }
    source->pages = (source->total + pageFrames - 1) / pageFrames;
    source->table = std::make_unique<std::atomic<Impl::Page*>[]>(static_cast<std::size_t>(source->pages));
    source->pending = std::make_unique<std::atomic<bool>[]>(static_cast<std::size_t>(source->pages));
    for (int page = 0; page < source->pages; ++page) { source->table[page].store(nullptr); source->pending[page].store(false); }
    result = std::move(source); return juce::Result::ok();
}
bool StreamingAudio::waitForRange(const std::shared_ptr<StreamedPcm>& source, int first, int count, int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + timeoutMs;
    const int end = std::min(source->frameCount(), first + count);
    for (;;)
    {
        source->request(first, count);
        bool ready = true;
        for (int frame = first; frame < end;)
        {
            auto view = source->acquire(frame);
            if (!view.data) { ready = false; break; }
            frame = std::min(end, view.first + view.count);
        }
        if (ready) return true;
        if (source->failed() || juce::Time::getMillisecondCounterHiRes() >= deadline) return false;
        juce::Thread::sleep(1);
    }
}
}
