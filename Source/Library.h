#pragma once
#include <juce_core/juce_core.h>
#include "AudioCore.h"

namespace dualdeck
{
struct Pad
{
    juce::String id, name, key, keyLabel;
    juce::File file;
    VoiceSettings settings;
    bool favorite = false, useGlobalMode = true;
    // Cached on load/scan; sorting and painting never read file metadata.
    juce::int64 fileSizeBytes = 0, modifiedMs = 0;
    bool fileMissing = true;
    void refreshFileInfo();
};
enum class SortField { index, name, modified, size, type };
struct Profile
{
    juce::String id, name;
    std::vector<Pad> pads;
    std::vector<juce::File> folders;
    SortField sortField = SortField::index;
    bool ascending = true;
};
struct ScannedFile
{
    juce::File file;
    juce::int64 size = 0, modified = 0;
    bool available = false, discovered = false;
};
struct FolderScan
{
    std::vector<juce::File> folders;
    std::vector<juce::File> completeFolders;
    std::vector<ScannedFile> files;
    juce::StringArray warnings;
    bool cancelled = false, partial = false;
};
FolderScan scanAudioFolders(const std::vector<juce::File>& folders, const std::vector<juce::File>& knownFiles,
                           const std::function<bool()>& cancelled = {});
std::vector<int> orderedPads(const Profile&, const juce::String& query = {}, bool favoritesOnly = false);
// One-based rank in the full selected order, indexed by the stored pad index.
// Filters and folder pages retain these numbers so #number remains searchable.
std::vector<int> padDisplayNumbers(const Profile&);
struct PadFolderPage
{
    juce::File folder;
    std::vector<int> indices;
};
// Each page belongs to one physical directory; pad indices remain unchanged.
std::vector<PadFolderPage> folderPadPages(const Profile&, const std::vector<int>& ordered,
                                        const juce::File& folder = {});
class Library
{
public:
    std::vector<Profile> profiles;
    int activeProfile = 0;
    PlayMode defaultMode = PlayMode::restart;
    float outputVolume = 0.7f, monitorVolume = 0.7f, microphoneVolume = 0.7f;
    juce::String outputDevice, monitorDevice, inputDevice, keyboardDevice, djStopKey = "0039:0";
    bool monitorEnabled = false, microphoneEnabled = false, microphoneMonitor = false;
    bool globalPitchEnabled = false;
    double globalPitch = 0.0;
    juce::Result load(const juce::File& file);
    juce::Result save(const juce::File& file) const;
    Profile* findProfile(const juce::String& id);
    bool removeProfile(const juce::String& id);
    juce::Result renameProfile(const juce::String& id, const juce::String& name);
    int mergeFolderScan(const juce::String& profileId, const FolderScan&, int* removed = nullptr);
    Profile& current() { return profiles[static_cast<std::size_t>(activeProfile)]; }
    const Profile& current() const { return profiles[static_cast<std::size_t>(activeProfile)]; }
    void ensureProfile();
    int totalPads() const;
};
juce::String modeName(PlayMode mode);
juce::File findProjectRoot();
juce::Result runLibraryTests(const juce::File& tempRoot);
juce::Result runLibraryFeatureTests(const juce::File& tempRoot);
}
