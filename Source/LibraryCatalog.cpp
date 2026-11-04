#include "Library.h"
#include <map>
#include <set>

namespace dualdeck
{
namespace
{
juce::String pathKey(const juce::File& file)
{
#if JUCE_WINDOWS
    return file.getFullPathName().toLowerCase();
#else
    return file.getFullPathName();
#endif
}
ScannedFile inspect(const juce::File& file)
{
    const bool exists = file.existsAsFile();
    return {file, exists ? file.getSize() : 0, exists ? file.getLastModificationTime().toMilliseconds() : 0, exists, false};
}
bool supported(const juce::File& file) { return file.hasFileExtension("wav;mp3;flac;ogg;aif;aiff;m4a;aac;wma"); }
}
void Pad::refreshFileInfo()
{
    const auto info = inspect(file);
    fileMissing = !info.available; fileSizeBytes = info.size; modifiedMs = info.modified;
}
Profile* Library::findProfile(const juce::String& id)
{
    for (auto& profile : profiles) if (profile.id == id) return &profile;
    return nullptr;
}
juce::Result Library::renameProfile(const juce::String& id, const juce::String& name)
{
    auto* profile = findProfile(id);
    if (!profile) return juce::Result::fail("Profilo non trovato.");
    const auto trimmed = name.trim();
    if (trimmed.isEmpty()) return juce::Result::fail("Inserisci un nome per il profilo.");
    if (trimmed.length() > 80) return juce::Result::fail("Il nome non puo superare 80 caratteri.");
    for (const auto& other : profiles)
        if (other.id != id && other.name.equalsIgnoreCase(trimmed))
            return juce::Result::fail("Esiste gia un profilo con questo nome.");
    profile->name = trimmed;
    return juce::Result::ok();
}
bool Library::removeProfile(const juce::String& id)
{
    const auto found = std::find_if(profiles.begin(), profiles.end(), [&](const Profile& p) { return p.id == id; });
    if (found == profiles.end()) return false;
    const auto activeId = current().id;
    profiles.erase(found);
    for (std::size_t i = 0; i < profiles.size(); ++i)
        if (profiles[i].id == activeId) { activeProfile = static_cast<int>(i); break; }
    ensureProfile();
    return true;
}
FolderScan scanAudioFolders(const std::vector<juce::File>& folders, const std::vector<juce::File>& knownFiles,
                           const std::function<bool()>& cancelled)
{
    FolderScan result;
    std::map<juce::String, ScannedFile> files;
    std::set<juce::String> roots;
    std::size_t visited = 0, discovered = 0;
    bool limitReached = false;
    const auto mustCancel = [&] { return cancelled && cancelled(); };
    for (const auto& folder : folders)
    {
        if (!roots.insert(pathKey(folder)).second) continue;
        result.folders.push_back(folder);
        if (mustCancel()) { result.cancelled = true; return result; }
        if (!folder.isDirectory()) { result.warnings.add("Cartella non disponibile: " + folder.getFullPathName()); continue; }
        if (limitReached) continue;
        for (const auto& entry : juce::RangedDirectoryIterator(folder, true, "*", juce::File::findFiles, juce::File::FollowSymlinks::no))
        {
            if (mustCancel()) { result.cancelled = true; return result; }
            if (++visited > 1000000 || discovered >= 100000) { limitReached = true; break; }
            if (!supported(entry.getFile())) continue;
            const auto inserted = files.emplace(pathKey(entry.getFile()), ScannedFile{entry.getFile(), entry.getFileSize(), entry.getModificationTime().toMilliseconds(), true, true});
            if (inserted.second) ++discovered;
        }
        if (!limitReached) result.completeFolders.push_back(folder);
    }
    result.partial = limitReached;
    if (limitReached) result.warnings.add("Scansione parziale: limite di 100.000 audio o 1.000.000 file. Scegli cartelle piu specifiche.");
    // Probe known paths as well, retaining their pad settings while files are absent.
    for (const auto& file : knownFiles)
    {
        if (mustCancel()) { result.cancelled = true; return result; }
        if (files.count(pathKey(file)) == 0) files.emplace(pathKey(file), inspect(file));
    }
    for (auto& entry : files) result.files.push_back(std::move(entry.second));
    std::sort(result.files.begin(), result.files.end(), [](const ScannedFile& a, const ScannedFile& b)
    { return a.file.getFullPathName().compareNatural(b.file.getFullPathName()) < 0; });
    return result;
}
int Library::mergeFolderScan(const juce::String& profileId, const FolderScan& scan, int* removed)
{
    if (removed) *removed = 0;
    auto* profile = findProfile(profileId);
    if (!profile || scan.cancelled) return 0;
    for (const auto& folder : scan.folders)
        if (std::find(profile->folders.begin(), profile->folders.end(), folder) == profile->folders.end()) profile->folders.push_back(folder);
    std::map<juce::String, const ScannedFile*> metadata;
    for (const auto& item : scan.files) metadata.emplace(pathKey(item.file), &item);
    // A missing file can return later. Keep the original pad and persisted
    // settings so the same path is reactivated instead of imported from scratch.
    std::set<juce::String> existing;
    for (auto& pad : profile->pads)
    {
        const auto key = pathKey(pad.file); existing.insert(key);
        const auto found = metadata.find(key);
        if (found == metadata.end()) continue;
        const auto& item = *found->second;
        pad.fileMissing = !item.available; pad.fileSizeBytes = item.size; pad.modifiedMs = item.modified;
    }
    int added = 0;
    for (const auto& item : scan.files)
    {
        if (!item.discovered || !item.available || !existing.insert(pathKey(item.file)).second) continue;
        Pad pad; pad.id = juce::Uuid().toString(); pad.name = item.file.getFileNameWithoutExtension(); pad.file = item.file;
        pad.fileMissing = false; pad.fileSizeBytes = item.size; pad.modifiedMs = item.modified;
        profile->pads.push_back(std::move(pad)); ++added;
    }
    return added;
}
std::vector<PadFolderPage> folderPadPages(const Profile& profile, const std::vector<int>& ordered,
                                        const juce::File& folder)
{
    std::map<juce::String, PadFolderPage> groups;
    for (const int index : ordered)
    {
        const auto parent = profile.pads[static_cast<std::size_t>(index)].file.getParentDirectory();
        if (folder != juce::File() && pathKey(parent) != pathKey(folder)) continue;
        auto& group = groups[pathKey(parent)];
        group.folder = parent;
        group.indices.push_back(index);
    }
    std::vector<PadFolderPage> sorted;
    for (auto& entry : groups) sorted.push_back(std::move(entry.second));
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b)
    { return a.folder.getFullPathName().compareNatural(b.folder.getFullPathName()) < 0; });
    std::vector<PadFolderPage> pages;
    for (const auto& group : sorted)
        for (std::size_t start = 0; start < group.indices.size(); start += 24)
            pages.push_back({group.folder, std::vector<int>(group.indices.begin() + start,
                group.indices.begin() + std::min(start + 24, group.indices.size()))});
    return pages;
}
std::vector<int> orderedPads(const Profile& profile, const juce::String& query, bool favoritesOnly)
{
    std::vector<int> result;
    for (std::size_t i = 0; i < profile.pads.size(); ++i)
        result.push_back(static_cast<int>(i));
    std::sort(result.begin(), result.end(), [&](int ai, int bi)
    {
        const auto& a = profile.pads[ai]; const auto& b = profile.pads[bi];
        int comparison = 0;
        const auto numeric = [](auto x, auto y) { return x < y ? -1 : x > y ? 1 : 0; };
        switch (profile.sortField)
        {
            case SortField::index: comparison = numeric(ai, bi); break;
            case SortField::name: comparison = a.name.compareNatural(b.name); break;
            case SortField::modified: comparison = numeric(a.modifiedMs, b.modifiedMs); break;
            case SortField::size: comparison = numeric(a.fileSizeBytes, b.fileSizeBytes); break;
            case SortField::type: comparison = a.file.getFileExtension().compareIgnoreCase(b.file.getFileExtension()); break;
        }
        return comparison == 0 ? ai < bi : profile.ascending ? comparison < 0 : comparison > 0;
    });
    // Rank the complete profile before filtering: #25 must find the pad
    // displayed as #25, including while name/favorite filters are active.
    const auto search = query.trim();
    if (search.isNotEmpty() || favoritesOnly)
    {
        std::size_t matched = 0;
        for (std::size_t rank = 0; rank < result.size(); ++rank)
        {
            const int index = result[rank];
            const auto& pad = profile.pads[static_cast<std::size_t>(index)];
            if ((!favoritesOnly || pad.favorite) && (search.isEmpty() || pad.name.containsIgnoreCase(search)
                || pad.keyLabel.containsIgnoreCase(search) || search == "#" + juce::String(rank + 1)))
                result[matched++] = index;
        }
        result.resize(matched);
    }
    return result;
}
std::vector<int> padDisplayNumbers(const Profile& profile)
{
    const auto ordered = orderedPads(profile);
    std::vector<int> numbers(profile.pads.size());
    for (std::size_t rank = 0; rank < ordered.size(); ++rank)
        numbers[static_cast<std::size_t>(ordered[rank])] = static_cast<int>(rank + 1);
    return numbers;
}
}
