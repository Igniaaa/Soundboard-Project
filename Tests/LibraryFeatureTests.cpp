#include "../Source/MainComponent.h"

namespace dualdeck
{
juce::Result runLibraryFeatureTests(const juce::File& tempRoot)
{
    const auto root = tempRoot.getChildFile(juce::Uuid().toString());
    const auto source = root.getChildFile("external");
    const auto nested = source.getChildFile("nested");
    const auto project = root.getChildFile("project");
    if (nested.createDirectory().failed() || project.createDirectory().failed()) return juce::Result::fail("Fixture folders");
    const auto first = source.getChildFile("sound2.wav");
    const auto second = source.getChildFile("sound10.mp3");
    const auto sameName = nested.getChildFile("sound2.wav");
    if (!first.replaceWithText("first") || !second.replaceWithText("second longer") || !sameName.replaceWithText("nested"))
        return juce::Result::fail("Fixture files");
    source.getChildFile("ignore.txt").replaceWithText("not audio");
    Library library; library.ensureProfile();
    const auto id = library.current().id;
    if (library.globalPitchEnabled || library.globalPitch != 0)
        return juce::Result::fail("Global audio defaults");
    const auto originalName = library.current().name;
    if (library.renameProfile(id, "  ").wasOk() || library.current().name != originalName
        || library.renameProfile("missing", "Name").wasOk()
        || library.renameProfile(id, juce::String::repeatedString("x", 81)).wasOk())
        return juce::Result::fail("Invalid profile names accepted");
    library.profiles.push_back({"other", "Other", {}});
    if (library.renameProfile(id, " other ").wasOk()) return juce::Result::fail("Duplicate profile name accepted");
    library.removeProfile("other");
    if (library.renameProfile(id, "  Suoni personali  ").failed() || library.current().id != id
        || library.current().name != "Suoni personali" || library.activeProfile != 0)
        return juce::Result::fail("Profile rename changed identity or selection");
    library.globalPitchEnabled = true; library.globalPitch = -3.5;
    library.outputVolume = 0.23f; library.monitorVolume = 0.67f;
    const auto scan = scanAudioFolders({source, nested, source}, {});
    if (scan.cancelled || library.mergeFolderScan(id, scan) != 3 || library.totalPads() != 3 || library.current().folders.size() != 2)
        return juce::Result::fail("Recursive scan / overlapping folders / duplicate root");
    if (project.getChildFile("sounds").exists() || first.loadFileAsString() != "first")
        return juce::Result::fail("Scan copied or changed original audio");
    auto& pads = library.current().pads;
    const auto firstIt = std::find_if(pads.begin(), pads.end(), [&](const Pad& p) { return p.file == first; });
    if (firstIt == pads.end()) return juce::Result::fail("External reference missing");
    const auto index = static_cast<std::size_t>(firstIt - pads.begin());
    pads[index].name = "Custom label"; pads[index].key = "001E:0"; pads[index].keyLabel = "A";
    pads[index].settings.gain = 0.37f; pads[index].settings.overrideMasterVolume = true; pads[index].favorite = true;
    const auto padId = pads[index].id;
    const auto survivor = std::find_if(pads.begin(), pads.end(), [&](const Pad& pad) { return pad.file == second; });
    if (survivor == pads.end()) return juce::Result::fail("Second linked sound missing");
    survivor->key = "0030:0"; survivor->settings.gain = 0.42f;
    const auto survivorId = survivor->id;
    std::vector<juce::File> known;
    for (const auto& pad : pads) known.push_back(pad.file);
    if (library.mergeFolderScan(id, scanAudioFolders({source, nested}, known)) != 0
        || pads[index].id != padId || pads[index].key != "001E:0" || pads[index].name != "Custom label"
        || pads[index].settings.gain != 0.37f || !pads[index].settings.overrideMasterVolume)
        return juce::Result::fail("Rescan duplicated or reset pad settings");
    const auto added = source.getChildFile("new.ogg"); added.replaceWithText("new file");
    if (library.mergeFolderScan(id, scanAudioFolders({source}, known)) != 1 || pads[index].id != padId)
        return juce::Result::fail("New file changed stable indices");
    known.push_back(added);
    const auto temporarilyMissing = source.getChildFile("sound2.wav.fixture-backup");
    Library synced = library;
    if (!first.moveFileTo(temporarilyMissing)) return juce::Result::fail("Missing-file fixture");
    auto deletionScan = scanAudioFolders({source, nested}, known);
    auto partialScan = deletionScan; partialScan.partial = true;
    int removed = 0;
    if (synced.mergeFolderScan(id, partialScan, &removed) != 0 || removed != 0 || synced.totalPads() != 4)
        return juce::Result::fail("Partial scan removed a pad");
    if (synced.mergeFolderScan(id, deletionScan, &removed) != 0 || removed != 1 || synced.totalPads() != 3
        || std::any_of(synced.current().pads.begin(), synced.current().pads.end(), [&](const Pad& pad) { return pad.id == padId; })
        || std::none_of(synced.current().pads.begin(), synced.current().pads.end(), [&](const Pad& pad)
        { return pad.id == survivorId && pad.key == "0030:0" && pad.settings.gain == 0.42f; }))
        return juce::Result::fail("Deleted sound remained in linked-folder pads");
    if (!temporarilyMissing.moveFileTo(first)) return juce::Result::fail("Restore fixture");
    if (synced.mergeFolderScan(id, scanAudioFolders({source, nested}, known), &removed) != 1 || removed != 0
        || std::none_of(synced.current().pads.begin(), synced.current().pads.end(), [&](const Pad& pad) { return pad.file == first && pad.id != padId; }))
        return juce::Result::fail("Returning sound was not linked as a new pad");
    const auto offlineRoot = root.getChildFile("offline");
    Pad offlinePad; offlinePad.id = "offline-pad"; offlinePad.file = offlineRoot.getChildFile("lost.wav");
    synced.current().pads.push_back(offlinePad);
    const auto offlineScan = scanAudioFolders({offlineRoot}, {offlinePad.file});
    if (offlineScan.warnings.isEmpty() || synced.mergeFolderScan(id, offlineScan, &removed) != 0 || removed != 0
        || std::none_of(synced.current().pads.begin(), synced.current().pads.end(), [&](const Pad& pad) { return pad.id == "offline-pad"; }))
        return juce::Result::fail("Unavailable folder removed a pad");
    const auto cancelled = scanAudioFolders({source}, known, [] { return true; });
    if (!cancelled.cancelled || library.mergeFolderScan(id, cancelled) != 0 || pads.size() != 4)
        return juce::Result::fail("Cancelled scan applied changes");
    const auto unavailable = scanAudioFolders({root.getChildFile("offline")}, known);
    if (unavailable.warnings.isEmpty()) return juce::Result::fail("Unavailable folder not reported");
    library.current().sortField = SortField::modified; library.current().ascending = false;
    const auto saved = project.getChildFile("data/library.json");
    auto result = library.save(saved); if (result.failed()) return result;
    Library loaded; result = loaded.load(saved); if (result.failed()) return result;
    if (!loaded.globalPitchEnabled || loaded.globalPitch != -3.5
        || std::abs(loaded.outputVolume - 0.23f) > 0.00001f || std::abs(loaded.monitorVolume - 0.67f) > 0.00001f
        || loaded.current().name != "Suoni personali" || loaded.current().id != id
        || loaded.current().pads[index].key != "001E:0" || loaded.current().pads[index].settings.gain != 0.37f
        || !loaded.current().pads[index].settings.overrideMasterVolume)
        return juce::Result::fail("Per-pad volume / renamed profile persistence");
    auto settingsJson = juce::JSON::parse(saved);
    settingsJson.getDynamicObject()->setProperty("GlobalPitch", 100);
    const auto bounded = project.getChildFile("data/bounded.json"); bounded.replaceWithText(juce::JSON::toString(settingsJson));
    Library clamped; result = clamped.load(bounded);
    if (result.failed() || clamped.globalPitch != 12) return juce::Result::fail("Global pitch clamp");
    settingsJson.getDynamicObject()->removeProperty("IndividualVolumes");
    settingsJson.getDynamicObject()->removeProperty("GlobalPitchEnabled");
    settingsJson.getDynamicObject()->removeProperty("GlobalPitch");
    settingsJson.getDynamicObject()->setProperty("SchemaVersion", 4);
    bounded.replaceWithText(juce::JSON::toString(settingsJson)); result = clamped.load(bounded);
    if (result.failed() || clamped.current().pads[index].settings.overrideMasterVolume
        || clamped.globalPitchEnabled || clamped.globalPitch != 0)
        return juce::Result::fail("Existing library safe audio defaults");
    if (loaded.current().folders != library.current().folders || loaded.current().sortField != SortField::modified
        || loaded.current().ascending || loaded.current().pads[index].file != first || loaded.current().pads[index].id != padId
        || loaded.current().pads[index].name != "Custom label" || !loaded.current().pads[index].favorite)
        return juce::Result::fail("Folder/settings/order persistence");
    // Missing external references must not be redirected to a same-named local file.
    first.moveFileTo(temporarilyMissing);
    const auto localCollision = project.getChildFile("sounds/sound2.wav");
    localCollision.getParentDirectory().createDirectory(); localCollision.replaceWithText("different audio");
    Library collision; result = collision.load(saved);
    if (result.failed() || collision.current().pads[index].file != first || !collision.current().pads[index].fileMissing)
        return juce::Result::fail("External file redirected to local name collision");
    temporarilyMissing.moveFileTo(first);
    Profile sortProfile;
    const std::array<juce::String, 4> names {"Sound 10", "sound 2", "Alpha", "Alpha"};
    const std::array<juce::String, 4> extensions {"wav", "mp3", "ogg", "aif"};
    const std::array<juce::int64, 4> sizes {300, 100, 200, 200};
    for (int i = 0; i < 4; ++i)
    {
        Pad pad; pad.id = juce::String(i); pad.name = names[i];
        pad.file = source.getChildFile(juce::String(i) + "." + extensions[i]);
        pad.fileSizeBytes = sizes[i]; pad.modifiedMs = sizes[i]; pad.favorite = i == 1;
        sortProfile.pads.push_back(pad);
    }
    const auto expect = [&](SortField field, bool ascending, std::vector<int> order)
    {
        sortProfile.sortField = field; sortProfile.ascending = ascending;
        if (orderedPads(sortProfile) != order) return false;
        const auto numbers = padDisplayNumbers(sortProfile);
        for (std::size_t rank = 0; rank < order.size(); ++rank)
            if (numbers[static_cast<std::size_t>(order[rank])] != static_cast<int>(rank + 1)
                || orderedPads(sortProfile, "#" + juce::String(rank + 1)) != std::vector<int>({order[rank]}))
                return false;
        return true;
    };
    if (!expect(SortField::index, true, {0,1,2,3}) || !expect(SortField::index, false, {3,2,1,0})
        || !expect(SortField::name, true, {2,3,1,0}) || !expect(SortField::name, false, {0,1,2,3})
        || !expect(SortField::modified, true, {1,2,3,0}) || !expect(SortField::modified, false, {0,2,3,1})
        || !expect(SortField::size, true, {1,2,3,0}) || !expect(SortField::size, false, {0,2,3,1})
        || !expect(SortField::type, true, {3,1,2,0}) || !expect(SortField::type, false, {0,2,1,3}))
        return juce::Result::fail("Sort criteria / directions / stable ties");
    if (orderedPads(sortProfile, "sound") != std::vector<int>({0,1})
        || orderedPads(sortProfile, {}, true) != std::vector<int>({1})
        || orderedPads(sortProfile, "#3") != std::vector<int>({1})
        || orderedPads(sortProfile, "#3", true) != std::vector<int>({1})
        || !orderedPads(sortProfile, "#1", true).empty()
        || !orderedPads(sortProfile, "#0").empty() || !orderedPads(sortProfile, "#999").empty()
        || padDisplayNumbers(library.current()) != padDisplayNumbers(loaded.current())
        || !padDisplayNumbers(Profile {}).empty()
        || sortProfile.pads[0].id != "0")
        return juce::Result::fail("Filter or sort changed pad identities");
    Profile grouped;
    const auto folderA = root.getChildFile("group2/Clips"), folderB = root.getChildFile("group10/Clips");
    for (int i = 0; i < 27; ++i)
    {
        Pad pad; pad.id = juce::String(i); pad.name = "Clip " + juce::String(i);
        pad.file = (i == 1 ? folderB : folderA).getChildFile(pad.name + ".wav");
        pad.favorite = i == 1 || i == 26;
        grouped.pads.push_back(pad);
    }
    const auto groupedPages = folderPadPages(grouped, orderedPads(grouped));
    if (groupedPages.size() != 3 || groupedPages[0].folder != folderA || groupedPages[1].folder != folderA
        || groupedPages[2].folder != folderB || groupedPages[0].indices.size() != 24
        || groupedPages[1].indices != std::vector<int>({25,26}) || groupedPages[2].indices != std::vector<int>({1}))
        return juce::Result::fail("Folder pages mixed directories, lost missing files or changed stable indices");
    grouped.ascending = false;
    const auto descendingPages = folderPadPages(grouped, orderedPads(grouped), folderA);
    const auto favoritePages = folderPadPages(grouped, orderedPads(grouped, {}, true));
    const auto searchedPages = folderPadPages(grouped, orderedPads(grouped, "#26"));
    const auto groupedNumbers = padDisplayNumbers(grouped);
    if (descendingPages.size() != 2 || descendingPages[0].indices.front() != 26
        || descendingPages[1].indices != std::vector<int>({2,0})
        || favoritePages.size() != 2 || favoritePages[0].indices != std::vector<int>({26})
        || favoritePages[1].indices != std::vector<int>({1})
        || searchedPages.size() != 1 || searchedPages[0].folder != folderB
        || !folderPadPages(grouped, orderedPads(grouped, "#26"), folderA).empty()
        || groupedNumbers[26] != 1 || groupedNumbers[25] != 2 || groupedNumbers[1] != 26
        || groupedNumbers[2] != 25 || groupedNumbers[0] != 27
        || !folderPadPages(grouped, {}).empty())
        return juce::Result::fail("Folder selection, search, favorites, sorting or empty pages");
    loaded.profiles.push_back({"second", "Second", {}});
    loaded.profiles.push_back({"third", "Third", {}});
    loaded.activeProfile = 2;
    if (!loaded.removeProfile("second") || loaded.current().id != "third" || loaded.activeProfile != 1)
        return juce::Result::fail("Deleting preceding inactive profile changed selection");
    if (!loaded.removeProfile("third") || loaded.current().id != id || !first.existsAsFile())
        return juce::Result::fail("Deleting active profile or audio preservation");
    if (!loaded.removeProfile(id) || loaded.profiles.size() != 1 || !loaded.current().pads.empty()
        || !loaded.current().folders.empty() || !first.existsAsFile() || loaded.removeProfile("absent")
        || loaded.mergeFolderScan(id, scan) != 0)
        return juce::Result::fail("Last profile replacement / stale scan after deletion");
    return juce::Result::ok();
}

juce::Result runPadOrderingUiTests()
{
    auto board = std::make_unique<MainComponent>(true);
    board->stopTimer();
    board->library = Library {};
    board->library.ensureProfile();
    auto& profile = board->library.current();
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("DualDeck-order-fixture");
    for (int i = 0; i < 30; ++i)
    {
        Pad pad; pad.id = "order-" + juce::String(i); pad.name = "Sound " + juce::String(30 - i);
        pad.file = folder.getChildFile(pad.name + ".wav"); pad.favorite = i == 0;
        if (i == 0) { pad.key = "003B:0"; pad.keyLabel = "F1"; }
        profile.pads.push_back(pad);
    }
    board->refresh(); board->selectPad(0);
    board->keyboard.keyPressed(juce::KeyPress(juce::KeyPress::rightKey)); // Focus F1.
    board->sortBy.setSelectedId(2, juce::sendNotificationSync);
    if (board->tiles[0]->index != 29 || board->tiles[0]->displayNumber != 1
        || board->tiles[0]->getTitle() != "Pad 1: Sound 1"
        || board->padHeading.getText() != "SUONO #30"
        || !board->keyboard.getDescription().startsWith("Associato al pad 30: Sound 30."))
        return juce::Result::fail("Name sort did not renumber tiles, details or keyboard accessibility");
    board->page = 1; board->rebuildTiles();
    if (board->tiles[0]->index != 5 || board->tiles[0]->displayNumber != 25)
        return juce::Result::fail("Pad numbers restarted on the second page");
    board->sortDirection.onClick();
    if (board->page != 0 || board->tiles[0]->index != 0 || board->tiles[0]->displayNumber != 1
        || board->padHeading.getText() != "SUONO #1" || board->keyboard.padNumber(0) != 1
        || !board->keyboard.getDescription().startsWith("Associato al pad 1: Sound 30."))
        return juce::Result::fail("Descending sort did not update the selected pad number");
    board->search.setText("#25", false); board->rebuildTiles();
    if (board->filtered != std::vector<int>({24}) || board->tiles[0]->displayNumber != 25)
        return juce::Result::fail("Index search disagrees with displayed pad number");
    board->search.clear(); board->favoritesOnly.setToggleState(true, juce::dontSendNotification);
    board->sortDirection.onClick();
    if (board->filtered != std::vector<int>({0}) || board->tiles[0]->displayNumber != 30)
        return juce::Result::fail("Favorite filtering changed the full-profile number");
    board->favoritesOnly.setToggleState(false, juce::dontSendNotification);
    profile.pads[0].name = "A first"; board->rebuildTiles();
    if (board->tiles[0]->index != 0 || board->tiles[0]->displayNumber != 1
        || board->padHeading.getText() != "SUONO #1"
        || profile.pads[0].id != "order-0" || profile.pads[0].key != "003B:0")
        return juce::Result::fail("Renaming did not update numbering or changed the pad binding");
    profile.pads[29].useGlobalMode = false; profile.pads[29].settings.mode = PlayMode::holdLoop;
    board->tiles[1]->press(board->tiles[1]->index, true);
    if (!board->pressedPads.isHeld(29) || board->pressedPads.isHeld(1))
        return juce::Result::fail("Display number was used as a playback index");
    board->tiles[1]->press(board->tiles[1]->index, false);
    if (board->pressedPads.isHeld(29)) return juce::Result::fail("Sorted pad release used the wrong index");
    return juce::Result::ok();
}
}
