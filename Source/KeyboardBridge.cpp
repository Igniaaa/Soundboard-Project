#include "KeyboardBridge.h"
#if JUCE_WINDOWS
#include "KeyboardBridgeProtocol.h"
#include "KeyboardFilterChannel.h"
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <vector>

namespace dualdeck
{
namespace
{
using namespace keyboardbridge;
using Context = void*;
using Stroke = WireStroke;
struct KeyStroke { unsigned short code, state; unsigned int information; };
static_assert(sizeof(KeyStroke) == 8 && sizeof(Stroke) == 20, "Interception v1.0.1 x64 ABI");
struct Handle
{
    HANDLE value = nullptr;
    Handle() = default;
    explicit Handle(HANDLE handle) : value(handle) {}
    ~Handle() { reset(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    void reset(HANDLE next = nullptr) { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); value = next; }
    explicit operator bool() const { return value && value != INVALID_HANDLE_VALUE; }
};
struct Mapping
{
    Shared* shared = nullptr;
    ~Mapping() { if (shared) UnmapViewOfFile(shared); }
};
std::uint64_t now() { return GetTickCount64(); }
std::wstring wide(const juce::String& value) { return value.toWideCharPointer(); }
juce::String winError(const char* text) { return juce::String(text) + " (Windows " + juce::String(static_cast<int>(GetLastError())) + ")"; }
bool allReleased()
{
    // 1..6 are mouse buttons; do not make mouse usage prevent keyboard arming.
    for (int key = 8; key <= 254; ++key) if ((GetAsyncKeyState(key) & 0x8000) != 0) return false;
    return true;
}
juce::Result verifyLibrary(HANDLE file)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return juce::Result::fail("Impossibile verificare SHA-256 della libreria tastiera.");
    struct AlgorithmGuard { BCRYPT_ALG_HANDLE value; ~AlgorithmGuard() { BCryptCloseAlgorithmProvider(value, 0); } } guard{algorithm};
    DWORD bytes = 0, objectLength = 0;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &bytes, 0) < 0)
        return juce::Result::fail("Verifica della libreria tastiera non disponibile.");
    std::vector<unsigned char> object(objectLength);
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptCreateHash(algorithm, &hash, object.data(), objectLength, nullptr, 0, 0) < 0)
        return juce::Result::fail("Impossibile preparare SHA-256 della libreria tastiera.");
    struct HashGuard { BCRYPT_HASH_HANDLE value; ~HashGuard() { BCryptDestroyHash(value); } } hashGuard{hash};
    std::array<unsigned char, 65536> buffer{};
    for (;;)
    {
        DWORD count = 0;
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr))
            return juce::Result::fail("Impossibile leggere la libreria tastiera.");
        if (count == 0) break;
        if (BCryptHashData(hash, buffer.data(), count, 0) < 0)
            return juce::Result::fail("Verifica SHA-256 della libreria tastiera fallita.");
    }
    std::array<unsigned char, 32> digest{};
    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
        return juce::Result::fail("Verifica SHA-256 della libreria tastiera fallita.");
    constexpr unsigned char expected[] = {0xAB,0x88,0x16,0x4C,0x11,0xB1,0xB4,0x84,0x88,0x77,0x2D,0x4C,0x3B,0xFA,0xA4,0x50,0x9D,0x5B,0x0A,0xE9,0xDB,0xC5,0xA6,0x91,0xDC,0x4F,0x96,0xF0,0x26,0x04,0x43,0xC8};
    if (!std::equal(digest.begin(), digest.end(), std::begin(expected)))
        return juce::Result::fail("Libreria tastiera diversa dal pacchetto ufficiale verificato (SHA-256).");
    return juce::Result::ok();
}
struct Driver
{
    Handle lockedFile;
    HMODULE module = nullptr;
    Context context = nullptr;
    Context (__cdecl* create)() = nullptr;
    void (__cdecl* destroy)(Context) = nullptr;
    int (__cdecl* wait)(Context, unsigned long) = nullptr;
    int (__cdecl* receive)(Context, int, Stroke*, unsigned int) = nullptr;
    int (__cdecl* send)(Context, int, const Stroke*, unsigned int) = nullptr;
    unsigned int (__cdecl* hardwareId)(Context, int, void*, unsigned int) = nullptr;
    ~Driver() { if (context && destroy) destroy(context); if (module) FreeLibrary(module); }
    juce::Result open(const juce::File& library)
    {
        const auto path = wide(library.getFullPathName());
        lockedFile.reset(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!lockedFile) return juce::Result::fail("Libreria tastiera mancante o non leggibile: " + library.getFullPathName());
        auto result = verifyLibrary(lockedFile.value);
        if (result.failed()) return result;
        module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module) return juce::Result::fail(winError("Impossibile caricare la libreria tastiera x64"));
#define DD_LOAD(member, name) member = reinterpret_cast<decltype(member)>(GetProcAddress(module, name)); if (!member) return juce::Result::fail("API ufficiale tastiera incompleta: " name)
        DD_LOAD(create, "interception_create_context");
        DD_LOAD(destroy, "interception_destroy_context");
        DD_LOAD(wait, "interception_wait_with_timeout");
        DD_LOAD(receive, "interception_receive");
        DD_LOAD(send, "interception_send");
        DD_LOAD(hardwareId, "interception_get_hardware_id");
#undef DD_LOAD
        context = create();
        if (!context) return juce::Result::fail("Driver Interception non disponibile o occupato: impossibile aprire il contesto.");
        return juce::Result::ok();
    }
    bool id(int slot, std::wstring& result)
    {
        std::array<wchar_t, 4096> buffer{};
        const auto bytes = hardwareId(context, slot, buffer.data(), static_cast<unsigned int>(sizeof(buffer)));
        result.clear();
        if (bytes == 0) return true; // Unpopulated keyboard slot.
        if (bytes > sizeof(buffer) || bytes % sizeof(wchar_t) != 0) return false;
        const auto length = bytes / sizeof(wchar_t);
        auto end = std::find(buffer.begin(), buffer.begin() + length, L'\0');
        if (end == buffer.begin() + length || end == buffer.begin()) return false;
        result.assign(buffer.begin(), end); // PnP returns most-specific HardwareId first.
        return true;
    }
    bool ids(std::array<std::wstring, 10>& result)
    {
        for (int slot = 1; slot <= 10; ++slot) if (!id(slot, result[static_cast<std::size_t>(slot - 1)])) return false;
        return true;
    }
};
// Broker has one context and one selected slot for its entire lifetime.
int selectedSlot = 0;
struct CaptureBackend
{
    Driver& driver;
    bool clearFilter(int slot) { return writePinnedKeyboardFilter(driver.context, slot, 0).ok; }
    int wait(unsigned long timeout) { return driver.wait(driver.context, timeout); }
    int receive(int device, WireStroke& stroke) { return driver.receive(driver.context, device, &stroke, 1); }
    int send(int device, const WireStroke& stroke) { return driver.send(driver.context, device, &stroke, 1); }
    void close() { if (driver.context) { driver.destroy(driver.context); driver.context = nullptr; } }
};

void publish(Shared& shared, State state, const juce::String& message = {})
{
    const auto text = wide(message);
    const auto count = std::min(text.size(), std::size(shared.error) - 1);
    std::copy_n(text.data(), count, shared.error); shared.error[count] = 0;
    InterlockedExchange(&shared.state, state);
}
}

struct KeyboardBridge::Impl
{
    explicit Impl(KeyboardBridge& value) : owner(value) {}
    KeyboardBridge& owner;
    Handle mapping, parent, process;
    Mapping view;
    bool armed = false, pendingStart = false;
    juce::String message;
    std::uint64_t launched = 0, generation = 0;
    ~Impl() { close(); }
    void close()
    {
        ++generation;
        armed = pendingStart = false;
        if (view.shared) InterlockedExchange(&view.shared->stop, 1);
        if (process)
        {
            if (WaitForSingleObject(process.value, 200) == WAIT_TIMEOUT)
            {
                // This is the handle returned for OUR child; never PID lookup.
                TerminateProcess(process.value, 2);
                WaitForSingleObject(process.value, 200);
            }
            process.reset();
        }
        if (view.shared) { UnmapViewOfFile(view.shared); view.shared = nullptr; }
        mapping.reset(); parent.reset();
    }
    juce::Result fail(const juce::String& error)
    {
        message = error; close();
        const auto callback = owner.onStateChanged;
        if (callback) callback();
        return juce::Result::fail(error);
    }
};
KeyboardBridge::KeyboardBridge() : impl(std::make_unique<Impl>(*this)) {}
KeyboardBridge::~KeyboardBridge() = default;
juce::File KeyboardBridge::runtimeLibrary()
{
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("keyboard-runtime").getChildFile("interception.dll");
}
juce::Result KeyboardBridge::start(const juce::String& exactHardwareId)
{
    impl->close(); impl->message.clear();
    const auto id = wide(exactHardwareId);
    if (id.empty() || id.size() >= std::size(Shared{}.hardwareId) || id.find(L'\0') != std::wstring::npos)
        return impl->fail("Identita PnP completa della tastiera assente o non valida.");
    if (!allReleased()) return impl->fail("Rilascia tutti i tasti prima di attivare la tastiera dedicata.");
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    impl->mapping.reset(CreateFileMappingW(INVALID_HANDLE_VALUE, &security, PAGE_READWRITE, 0, sizeof(Shared), nullptr));
    if (!impl->mapping) return impl->fail(winError("Impossibile creare il collegamento tastiera"));
    impl->view.shared = static_cast<Shared*>(MapViewOfFile(impl->mapping.value, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
    if (!impl->view.shared) return impl->fail(winError("Memoria del collegamento tastiera non disponibile"));
    auto& shared = *new (impl->view.shared) Shared{};
    std::copy(id.begin(), id.end(), shared.hardwareId);
    impl->launched = now(); InterlockedExchange64(&shared.uiHeartbeat, static_cast<LONG64>(impl->launched));
    InterlockedExchange64(&shared.brokerHeartbeat, static_cast<LONG64>(impl->launched));
    HANDLE parent = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &parent, SYNCHRONIZE, TRUE, 0))
        return impl->fail(winError("Impossibile monitorare il processo principale"));
    impl->parent.reset(parent);
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<unsigned char> attributes(bytes);
    auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if (!InitializeProcThreadAttributeList(list, 1, 0, &bytes)) return impl->fail(winError("Avvio protetto del broker non disponibile"));
    struct AttributeGuard { LPPROC_THREAD_ATTRIBUTE_LIST value; ~AttributeGuard() { DeleteProcThreadAttributeList(value); } } guard{list};
    HANDLE handles[] = {impl->mapping.value, impl->parent.value};
    if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles, sizeof(handles), nullptr, nullptr))
        return impl->fail(winError("Impossibile limitare gli handle del broker tastiera"));
    const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto command = L"\"" + wide(executable.getFullPathName()) + L"\" --keyboard-broker "
        + std::to_wstring(reinterpret_cast<std::uintptr_t>(impl->mapping.value)) + L" "
        + std::to_wstring(reinterpret_cast<std::uintptr_t>(impl->parent.value)) + L" \"" + wide(runtimeLibrary().getFullPathName()) + L"\"";
    STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup); startup.StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startup.StartupInfo.wShowWindow = SW_HIDE; startup.lpAttributeList = list;
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(wide(executable.getFullPathName()).c_str(), command.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo, &info))
        return impl->fail(winError("Impossibile avviare il broker tastiera"));
    impl->process.reset(info.hProcess); CloseHandle(info.hThread);
    // Only the child needs these handles inheritable. Future launches cannot inherit them.
    SetHandleInformation(impl->mapping.value, HANDLE_FLAG_INHERIT, 0);
    impl->parent.reset();
    impl->pendingStart = true;
    const auto callback = onStateChanged;
    if (callback) callback();
    return juce::Result::ok();
}
void KeyboardBridge::stop()
{
    const bool changed = impl->armed || impl->pendingStart;
    impl->close(); impl->message.clear();
    const auto callback = onStateChanged;
    if (changed && callback) callback();
}
bool KeyboardBridge::active() const { return impl->armed; }
bool KeyboardBridge::starting() const { return impl->pendingStart; }
juce::String KeyboardBridge::error() const { return impl->message; }
void KeyboardBridge::poll()
{
    auto* shared = impl->view.shared;
    if (!shared) return;
    const auto generation = impl->generation;
    const auto heartbeat = sampleHeartbeat(shared->brokerHeartbeat);
    const auto time = heartbeat.time;
    InterlockedExchange64(&shared->uiHeartbeat, static_cast<LONG64>(time));
    const auto state = read32(shared->state);
    if (state == stopping || state == failed || state == stopped)
    {
        discard(*shared);
        // The broker writes final error text only after cleanup; never read it
        // while that publication is still in progress.
        const auto error = state == stopping ? juce::String("Cattura tastiera in chiusura; pad fermati.") : juce::String(shared->error);
        impl->fail(error.isNotEmpty() ? error : "Collegamento tastiera terminato; riattiva manualmente la cattura."); return;
    }
    if (heartbeat.isExpired())
    { impl->fail("Il broker tastiera non risponde: cattura disattivata e pad fermati."); return; }
    if (WaitForSingleObject(impl->process.value, 0) != WAIT_TIMEOUT)
    { impl->fail("Il broker tastiera si e chiuso: cattura disattivata e pad fermati."); return; }
    if (state == running && !impl->armed)
    {
        impl->armed = true; impl->pendingStart = false;
        const auto callback = onStateChanged;
        if (callback) callback();
        if (impl->generation != generation) return; // Callback can stop/restart, reusing an address.
    }
    if (!impl->armed) return;
    if (queued(*shared) > capacity) { impl->fail("Coda tastiera non valida: cattura disattivata."); return; }
    Packet packet;
    unsigned delivered = 0;
    while (delivered++ < capacity && popForDelivery(*shared, packet))
    {
        if (!valid(packet)) { impl->fail("Pacchetto tastiera non valido: cattura disattivata."); return; }
        if (read32(shared->state) != running) break;
        // Protocol prefix 0/1/2 maps to the Raw Input flags 0/E0=2/E1=4.
        const unsigned flags = packet.prefix == 0 ? 0 : (packet.prefix == 1 ? 2 : 4);
        const auto callback = onKey;
        if (callback) callback(juce::String::formatted("%04X:%u", packet.scan, flags), packet.down != 0);
        if (impl->generation != generation || !impl->armed) return;
    }
    if (read32(shared->state) != running) poll();
}

int runKeyboardBroker(std::uintptr_t mappingHandle, std::uintptr_t parentHandle, const juce::File& library)
{
    Handle mapping(reinterpret_cast<HANDLE>(mappingHandle)), parent(reinterpret_cast<HANDLE>(parentHandle));
    if (!mapping || !parent || WaitForSingleObject(parent.value, 0) != WAIT_TIMEOUT) return 2;
    Mapping view; view.shared = static_cast<Shared*>(MapViewOfFile(mapping.value, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
    if (!view.shared) return 2;
    auto& shared = *view.shared;
    if (shared.signature != magic || shared.protocol != version || read32(shared.state) != pending) return 2;
    auto fail = [&](const juce::String& error) { publish(shared, failed, error); return 1; };
    const auto end = std::find(std::begin(shared.hardwareId), std::end(shared.hardwareId), L'\0');
    if (end == std::end(shared.hardwareId) || end == std::begin(shared.hardwareId)) return fail("Identita PnP tastiera non valida.");
    const std::wstring wanted(std::begin(shared.hardwareId), end);
    InterlockedExchange64(&shared.brokerHeartbeat, static_cast<LONG64>(now()));
    Driver driver;
    auto opened = driver.open(library);
    if (opened.failed()) return fail(opened.getErrorMessage());
    CaptureBackend backend{driver};
    FilterOwner<CaptureBackend> capture(backend, 0);
    auto abort = [&](const juce::String& error) { const bool cleared = retireCapture(shared, capture); return fail(error + (cleared ? "" : " Rimozione del filtro non confermata; contesto chiuso.")); };
    std::array<std::wstring, 10> ids;
    if (!driver.ids(ids)) return abort("Risposta HardwareId del driver non valida.");
    selectedSlot = uniqueSlot(ids, wanted);
    if (selectedSlot == 0) return abort("Tastiera assente o identita PnP non univoca nei dieci slot del driver.");
    if (!allReleased()) return abort("Rilascia tutti i tasti prima di attivare la tastiera dedicata.");
    const auto startupHeartbeat = sampleHeartbeat(shared.uiHeartbeat);
    if (read32(shared.stop) || WaitForSingleObject(parent.value, 0) != WAIT_TIMEOUT
        || startupHeartbeat.isExpired()) return abort("Attivazione tastiera annullata: collegamento non disponibile.");
    KeyboardBlockSession policy;
    KeyboardScanSequence sequence;
    policy.setBackendAvailable(true); policy.select(static_cast<std::uintptr_t>(selectedSlot));
    if (!policy.confirmAllKeysReleased() || !policy.arm(now())) return abort("Impossibile armare la sessione tastiera.");
    capture.slot = selectedSlot;
    capture.attempted = true;
    // This fresh DLL context has independent zero-initialised per-open filters.
    // The legacy driver has no GET_FILTER handler (it returns zero bytes).
    // Check the actual SET_FILTER completion instead, targeting only this slot.
    const auto filterWrite = writePinnedKeyboardFilter(driver.context, selectedSlot, 0xffff);
    if (!filterWrite.ok)
        return abort(juce::String::formatted("Filtro tastiera rifiutato: slot %d, errore Windows %lu.", selectedSlot, filterWrite.error));
    // Catch a key pressed during setup; never arm against a known held key.
    if (!allReleased()) return abort("Un tasto e stato premuto durante l'attivazione: rilascia i tasti e riprova.");
    if (!driver.ids(ids) || uniqueSlot(ids, wanted) != selectedSlot) return abort("Identita della tastiera cambiata durante l'attivazione.");
    auto checkedIdentity = now();
    auto observedUi = static_cast<std::uint64_t>(read64(shared.uiHeartbeat));
    InterlockedExchange64(&shared.brokerHeartbeat, static_cast<LONG64>(now()));
    publish(shared, running);
    for (;;)
    {
        const auto heartbeat = sampleHeartbeat(shared.uiHeartbeat);
        const auto time = heartbeat.time;
        InterlockedExchange64(&shared.brokerHeartbeat, static_cast<LONG64>(time));
        if (read32(shared.stop))
        {
            policy.disable(); const bool cleared = retireCapture(shared, capture);
            publish(shared, cleared ? stopped : failed, cleared ? juce::String() : juce::String("Rimozione del filtro non confermata; contesto chiuso.")); return cleared ? 0 : 1;
        }
        if (WaitForSingleObject(parent.value, 0) != WAIT_TIMEOUT) return abort("Processo principale chiuso: cattura disattivata.");
        if (heartbeat.isExpired()) return abort("Heartbeat dell'interfaccia scaduto: cattura disattivata.");
        if (heartbeat.heartbeat != observedUi)
        {
            if (!policy.heartbeat(time)) return abort("Sessione tastiera scaduta: cattura disattivata.");
            observedUi = heartbeat.heartbeat;
        }
        policy.poll(time);
        if (!policy.isArmed()) return abort("Sessione tastiera scaduta: cattura disattivata.");
        if (time - checkedIdentity >= 250)
        {
            if (!driver.ids(ids) || uniqueSlot(ids, wanted) != selectedSlot) return abort("Tastiera scollegata o identita cambiata: selezionala di nuovo.");
            checkedIdentity = time;
        }
        if (queued(shared) >= capacity) return abort("Coda tastiera piena: cattura disattivata e pad fermati.");
        const auto device = driver.wait(driver.context, 10);
        if (device == 0) continue;
        if (device < 1 || device > 20) return abort("Errore di attesa del driver tastiera.");
        Stroke stroke{};
        if (driver.receive(driver.context, device, &stroke, 1) != 1) return abort("Errore di lettura del driver tastiera.");
        if (device != selectedSlot)
        {
            if (driver.send(driver.context, device, &stroke, 1) != 1) return abort("Errore nell'inoltro di input non selezionato.");
            continue;
        }
        KeyStroke key{}; std::memcpy(&key, stroke.data(), sizeof(key));
        Packet packet;
        if (!decode(key.code, key.state, packet))
        {
            forwardRetiring(shared, backend, device, stroke);
            return abort("Pacchetto tastiera non valido: cattura disattivata.");
        }
        if (!deliverPhysicalKey(shared, policy, sequence, static_cast<std::uintptr_t>(device), packet, now()))
        {
            forwardRetiring(shared, backend, device, stroke);
            return abort("Sessione tastiera interrotta o coda piena: pad fermati.");
        }
    }
}

juce::Result writeKeyboardDriverStatus(const juce::File& library, const juce::File& report)
{
    Driver driver;
    const auto opened = driver.open(library);
    juce::String output = "DualDeck - verifica passiva driver tastiera\nLibreria: " + library.getFullPathName() + "\n";
    juce::Result result = opened;
    if (opened.failed()) output += "STATO: ERRORE\n" + opened.getErrorMessage() + "\n";
    else
    {
        std::array<std::wstring, 10> ids;
        if (!driver.ids(ids)) { result = juce::Result::fail("Risposta HardwareId non valida."); output += "STATO: ERRORE HardwareId\n"; }
        else
        {
            unsigned count = 0;
            for (unsigned index = 0; index < ids.size(); ++index)
            {
                if (!ids[index].empty()) ++count;
                output += "Slot tastiera " + juce::String(index + 1) + ": " + (ids[index].empty() ? juce::String("vuoto / non disponibile") : juce::String(ids[index].c_str())) + "\n";
            }
            output += "STATO: " + juce::String(count > 0 ? "CONTESTO APERTO" : "NESSUNA TASTIERA DISPONIBILE") + "\nTastiere leggibili: " + juce::String(count) + "\nFiltri non applicati; nessun input letto o inviato.\n";
            if (count == 0) result = juce::Result::fail("Driver non accessibile o nessuna tastiera disponibile nei dieci slot.");
        }
    }
    if (!report.replaceWithText(output)) return juce::Result::fail("Impossibile scrivere il rapporto passivo: " + report.getFullPathName());
    return result;
}
}
#else
namespace dualdeck
{
struct KeyboardBridge::Impl {};
KeyboardBridge::KeyboardBridge() : impl(std::make_unique<Impl>()) {}
KeyboardBridge::~KeyboardBridge() = default;
juce::Result KeyboardBridge::start(const juce::String&) { return juce::Result::fail("Blocco tastiera disponibile solo su Windows."); }
void KeyboardBridge::stop() {}
void KeyboardBridge::poll() {}
bool KeyboardBridge::active() const { return false; }
bool KeyboardBridge::starting() const { return false; }
juce::String KeyboardBridge::error() const { return "Blocco tastiera disponibile solo su Windows."; }
juce::File KeyboardBridge::runtimeLibrary() { return {}; }
int runKeyboardBroker(std::uintptr_t, std::uintptr_t, const juce::File&) { return 2; }
juce::Result writeKeyboardDriverStatus(const juce::File&, const juce::File&) { return juce::Result::fail("Windows richiesto."); }
}
#endif
