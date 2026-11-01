#include "Library.h"
#include <set>

namespace dualdeck
{
namespace
{
juce::var object() { return juce::var(new juce::DynamicObject()); }
void put(juce::var& value, const juce::Identifier& key, const juce::var& content) { value.getDynamicObject()->setProperty(key, content); }
double number(const juce::var& value, const char* name, double fallback, double low, double high)
{
    if (!value.hasProperty(name)) return fallback;
    double result = static_cast<double>(value[name]);
    return std::isfinite(result) ? std::clamp(result, low, high) : fallback;
}
juce::String textOr(const juce::var& value, const char* name, const juce::String& fallback)
{
    auto text = value[name].toString().trim();
    return text.isEmpty() ? fallback : text;
}
}
void Library::ensureProfile()
{
    if (profiles.empty()) profiles.push_back({juce::Uuid().toString(), "Principale", {}});
    activeProfile = juce::jlimit(0, static_cast<int>(profiles.size()) - 1, activeProfile);
}
int Library::totalPads() const
{
    int count = 0;
    for (const auto& p : profiles) count += static_cast<int>(p.pads.size());
    return count;
}
juce::Result Library::load(const juce::File& file)
{
    if (!file.existsAsFile()) return juce::Result::fail("File del profilo non trovato.");
    if (file.getSize() > 128 * 1024 * 1024) return juce::Result::fail("Il profilo supera 128 MiB.");
    juce::var root;
    auto parsed = juce::JSON::parse(file.loadFileAsString(), root);
    if (parsed.failed() || !root.isObject()) return juce::Result::fail("Il profilo JSON non e valido: " + parsed.getErrorMessage());
    const int schema = static_cast<int>(root["SchemaVersion"]);
    if (schema < 3 || schema > 10)
        return juce::Result::fail("Versione della libreria non supportata.");
    Library next;
    next.defaultMode = static_cast<PlayMode>(static_cast<int>(number(root, "DefaultMode", 0, 0, 3)));
    next.outputVolume = static_cast<float>(number(root, "Volume", 70, 0, 100) / 100.0);
    next.monitorVolume = static_cast<float>(number(root, "MonitorVolume", next.outputVolume * 100, 0, 100) / 100.0);
    next.microphoneVolume = static_cast<float>(number(root, "MicrophoneVolume", 70, 0, 100) / 100.0);
    const bool legacyIndividualVolumes = static_cast<bool>(root["IndividualVolumes"]);
    next.globalPitchEnabled = static_cast<bool>(root["GlobalPitchEnabled"]);
    next.globalPitch = number(root, "GlobalPitch", 0, -12, 12);
    next.djStopKey = textOr(root, "DjStopKey", "0039:0");
    next.outputDevice = root["OutputDevice"].toString();
    next.monitorDevice = root["MonitorDevice"].toString();
    next.inputDevice = root["InputDevice"].toString();
    next.monitorEnabled = static_cast<bool>(root["MonitorEnabled"]);
    next.microphoneEnabled = static_cast<bool>(root["MicrophoneEnabled"]);
    next.microphoneMonitor = static_cast<bool>(root["MicrophoneMonitor"]);
    next.keyboardDevice = root["KeyboardDevice"].toString();
    auto* entries = root["Profiles"].getArray();
    if (!entries || entries->isEmpty()) return juce::Result::fail("Nessun profilo nel file.");
    const auto mediaRoot = file.getParentDirectory();
    std::set<juce::String> profileIds;
    for (const auto& p : *entries)
    {
        const auto* pads = p["Pads"].getArray();
        if (!p.isObject() || !pads) return juce::Result::fail("Profilo senza elenco suoni.");
        Profile profile;
        profile.id = textOr(p, "Id", juce::Uuid().toString());
        if (!profileIds.insert(profile.id).second) { profile.id = juce::Uuid().toString(); profileIds.insert(profile.id); }
        profile.name = textOr(p, "Name", "Principale");
        profile.sortField = static_cast<SortField>(static_cast<int>(number(p, "SortField", 0, 0, 4)));
        profile.ascending = !p.hasProperty("SortAscending") || static_cast<bool>(p["SortAscending"]);
        if (const auto* folders = p["Folders"].getArray())
            for (const auto& folder : *folders)
            {
                const auto path = folder.toString();
                if (path.isEmpty()) continue;
                const auto resolved = juce::File::isAbsolutePath(path) ? juce::File(path) : mediaRoot.getChildFile(path);
                if (std::find(profile.folders.begin(), profile.folders.end(), resolved) == profile.folders.end()) profile.folders.push_back(resolved);
            }
        std::set<juce::String> padIds, keys;
        for (const auto& entry : *pads)
        {
            const auto path = entry["File"].toString();
            if (!entry.isObject() || path.isEmpty()) return juce::Result::fail("Suono senza percorso audio.");
            Pad pad;
            pad.id = textOr(entry, "Id", juce::Uuid().toString());
            if (!padIds.insert(pad.id).second) { pad.id = juce::Uuid().toString(); padIds.insert(pad.id); }
            pad.file = juce::File::isAbsolutePath(path) ? juce::File(path) : mediaRoot.getChildFile(path);
            // Recover an old absolute path only when the exact local filename exists.
            if (!pad.file.existsAsFile() && (schema == 3 || static_cast<bool>(entry["LocalMedia"])))
            {
                const auto local = mediaRoot.getParentDirectory().getChildFile("sounds").getChildFile(pad.file.getFileName());
                if (local.existsAsFile()) pad.file = local;
            }
            pad.name = textOr(entry, "Name", pad.file.getFileNameWithoutExtension());
            pad.key = entry["Key"].toString(); pad.keyLabel = entry["KeyLabel"].toString();
            if (pad.key == "0001:0" || pad.key == next.djStopKey || (!pad.key.isEmpty() && !keys.insert(pad.key).second))
            { pad.key.clear(); pad.keyLabel.clear(); }
            pad.favorite = static_cast<bool>(entry["Favorite"]);
            pad.settings.mode = static_cast<PlayMode>(static_cast<int>(number(entry, "Mode", 0, 0, 3)));
            pad.useGlobalMode = entry.hasProperty("UseGlobalMode") ? static_cast<bool>(entry["UseGlobalMode"]) : pad.settings.mode == PlayMode::restart;
            pad.settings.gain = static_cast<float>(number(entry, "Volume", 85, 0, 100) / 100.0);
            pad.settings.overrideMasterVolume = schema >= 9
                ? static_cast<bool>(entry["OverrideMasterVolume"]) : legacyIndividualVolumes;
            pad.settings.speed = number(entry, "Speed", 1, 0.5, 2);
            pad.settings.trimStart = number(entry, "TrimStart", 0, 0, 180);
            pad.settings.trimEnd = number(entry, "TrimEnd", 0, 0, 180);
            pad.settings.fadeIn = number(entry, "FadeIn", 0, 0, 10);
            pad.settings.fadeOut = number(entry, "FadeOut", 0.08, 0, 10);
            if (schema >= 7)
            {
                pad.settings.filterCutoffHz = static_cast<float>(number(entry, "FilterCutoffHz", 3000, 100, 18000));
                pad.settings.filterMix = static_cast<float>(number(entry, "FilterMix", 0, 0, 100) / 100.0);
                pad.settings.highPassCutoffHz = static_cast<float>(number(entry, "HighPassCutoffHz", 100, 100, 18000));
                pad.settings.highPassMix = static_cast<float>(number(entry, "HighPassMix", 0, 0, 100) / 100.0);
                pad.settings.delayTimeMs = static_cast<float>(number(entry, "DelayTimeMs", 220, 50, 1000));
                pad.settings.delayFeedback = static_cast<float>(number(entry, "DelayFeedback", 35, 0, 80) / 100.0);
                pad.settings.delayMix = static_cast<float>(number(entry, "DelayMix", 0, 0, 100) / 100.0);
            }
            else
            {
                const auto oldFilter = number(entry, "FilterAmount", 0, 0, 100) / 100.0;
                pad.settings.filterCutoffHz = oldFilter > 0
                    ? static_cast<float>(18000.0 * std::pow(300.0 / 18000.0, oldFilter)) : 3000.0f;
                pad.settings.filterMix = oldFilter > 0 ? 1.0f : 0.0f;
                pad.settings.delayMix = static_cast<float>(number(entry, "EchoAmount", 0, 0, 100) / 100.0);
            }
            if (schema >= 10)
            {
                pad.settings.reverbDecay = static_cast<float>(number(entry, "ReverbDecay", 2, 0.2, 2));
                pad.settings.reverbMix = static_cast<float>(number(entry, "ReverbMix", 0, 0, 100) / 100.0);
                pad.settings.compressorEnabled = static_cast<bool>(entry["CompressorEnabled"]);
                pad.settings.compressorThresholdDb = static_cast<float>(number(entry, "CompressorThresholdDb", -24, -48, 0));
                pad.settings.compressorRatio = static_cast<float>(number(entry, "CompressorRatio", 4, 1, 20));
                pad.settings.tremoloEnabled = static_cast<bool>(entry["TremoloEnabled"]);
                pad.settings.tremoloSpeedHz = static_cast<float>(number(entry, "TremoloSpeedHz", 1.6, 0.1, 16));
                pad.settings.tremoloDepth = static_cast<float>(number(entry, "TremoloDepth", 50, 0, 100) / 100.0);
                pad.settings.distortionEnabled = static_cast<bool>(entry["DistortionEnabled"]);
                pad.settings.distortionDrive = static_cast<float>(number(entry, "DistortionDrive", 40, 0, 100));
                pad.settings.pingPongTimeMs = static_cast<float>(number(entry, "PingPongTimeMs", 250, 50, 1000));
                pad.settings.pingPongFeedback = static_cast<float>(number(entry, "PingPongFeedback", 30, 0, 80) / 100.0);
                pad.settings.pingPongMix = static_cast<float>(number(entry, "PingPongMix", 0, 0, 100) / 100.0);
                pad.settings.chorusSpeedHz = static_cast<float>(number(entry, "ChorusSpeedHz", 1.5, 0.1, 10));
                pad.settings.chorusDepth = static_cast<float>(number(entry, "ChorusDepth", 70, 0, 100) / 100.0);
                pad.settings.chorusMix = static_cast<float>(number(entry, "ChorusMix", 0, 0, 100) / 100.0);
            }
            pad.refreshFileInfo();
            profile.pads.push_back(std::move(pad));
        }
        if (profile.id == root["ActiveProfileId"].toString()) next.activeProfile = static_cast<int>(next.profiles.size());
        next.profiles.push_back(std::move(profile));
    }
    next.ensureProfile();
    *this = std::move(next);
    return juce::Result::ok();
}
juce::Result Library::save(const juce::File& file) const
{
    if (profiles.empty()) return juce::Result::fail("Nessun profilo da salvare.");
    auto root = object();
    put(root, "SchemaVersion", 10); put(root, "ActiveProfileId", current().id);
    put(root, "DefaultMode", static_cast<int>(defaultMode));
    put(root, "Volume", outputVolume * 100.0); put(root, "MonitorVolume", monitorVolume * 100.0);
    put(root, "OutputDevice", outputDevice); put(root, "MonitorDevice", monitorDevice);
    put(root, "InputDevice", inputDevice); put(root, "MicrophoneVolume", microphoneVolume * 100.0);
    put(root, "MicrophoneEnabled", microphoneEnabled); put(root, "MicrophoneMonitor", microphoneMonitor);
    put(root, "MonitorEnabled", monitorEnabled); put(root, "DjStopKey", djStopKey);
    put(root, "KeyboardDevice", keyboardDevice);
    put(root, "GlobalPitchEnabled", globalPitchEnabled); put(root, "GlobalPitch", globalPitch);
    juce::Array<juce::var> profileArray;
    for (const auto& p : profiles)
    {
        auto profile = object(); put(profile, "Id", p.id); put(profile, "Name", p.name);
        put(profile, "SortField", static_cast<int>(p.sortField)); put(profile, "SortAscending", p.ascending);
        juce::Array<juce::var> folders;
        const auto projectRoot = file.getParentDirectory().getParentDirectory();
        for (const auto& folder : p.folders)
            folders.add(folder == projectRoot || folder.isAChildOf(projectRoot) ? folder.getRelativePathFrom(file.getParentDirectory()) : folder.getFullPathName());
        put(profile, "Folders", folders);
        juce::Array<juce::var> pads;
        for (const auto& pad : p.pads)
        {
            auto value = object();
            put(value, "Id", pad.id); put(value, "Name", pad.name);
            const auto ownSounds = file.getParentDirectory().getParentDirectory().getChildFile("sounds");
            put(value, "LocalMedia", pad.file.isAChildOf(ownSounds));
            put(value, "File", pad.file.isAChildOf(projectRoot) ? pad.file.getRelativePathFrom(file.getParentDirectory()) : pad.file.getFullPathName());
            put(value, "Key", pad.key); put(value, "KeyLabel", pad.keyLabel); put(value, "Favorite", pad.favorite);
            put(value, "Mode", static_cast<int>(pad.settings.mode)); put(value, "UseGlobalMode", pad.useGlobalMode);
            put(value, "Volume", pad.settings.gain * 100.0); put(value, "OverrideMasterVolume", pad.settings.overrideMasterVolume);
            put(value, "Speed", pad.settings.speed);
            put(value, "TrimStart", pad.settings.trimStart); put(value, "TrimEnd", pad.settings.trimEnd);
            put(value, "FadeIn", pad.settings.fadeIn); put(value, "FadeOut", pad.settings.fadeOut);
            put(value, "FilterCutoffHz", pad.settings.filterCutoffHz);
            put(value, "FilterMix", pad.settings.filterMix * 100.0);
            put(value, "HighPassCutoffHz", pad.settings.highPassCutoffHz);
            put(value, "HighPassMix", pad.settings.highPassMix * 100.0);
            put(value, "DelayTimeMs", pad.settings.delayTimeMs);
            put(value, "DelayFeedback", pad.settings.delayFeedback * 100.0);
            put(value, "DelayMix", pad.settings.delayMix * 100.0);
            put(value, "ReverbDecay", pad.settings.reverbDecay);
            put(value, "ReverbMix", pad.settings.reverbMix * 100.0);
            put(value, "CompressorEnabled", pad.settings.compressorEnabled);
            put(value, "CompressorThresholdDb", pad.settings.compressorThresholdDb);
            put(value, "CompressorRatio", pad.settings.compressorRatio);
            put(value, "TremoloEnabled", pad.settings.tremoloEnabled);
            put(value, "TremoloSpeedHz", pad.settings.tremoloSpeedHz);
            put(value, "TremoloDepth", pad.settings.tremoloDepth * 100.0);
            put(value, "DistortionEnabled", pad.settings.distortionEnabled);
            put(value, "DistortionDrive", pad.settings.distortionDrive);
            put(value, "PingPongTimeMs", pad.settings.pingPongTimeMs);
            put(value, "PingPongFeedback", pad.settings.pingPongFeedback * 100.0);
            put(value, "PingPongMix", pad.settings.pingPongMix * 100.0);
            put(value, "ChorusSpeedHz", pad.settings.chorusSpeedHz);
            put(value, "ChorusDepth", pad.settings.chorusDepth * 100.0);
            put(value, "ChorusMix", pad.settings.chorusMix * 100.0);
            pads.add(value);
        }
        put(profile, "Pads", pads); profileArray.add(profile);
    }
    put(root, "Profiles", profileArray);
    const auto result = file.getParentDirectory().createDirectory();
    if (result.failed()) return result;
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(juce::JSON::toString(root, true))) return juce::Result::fail("Scrittura della libreria non riuscita.");
    if (file.existsAsFile() && !file.copyFileTo(file.getSiblingFile(file.getFileName() + ".bak")))
        return juce::Result::fail("Backup della libreria non riuscito; salvataggio annullato.");
    return temporary.overwriteTargetFileWithTemporary() ? juce::Result::ok() : juce::Result::fail("Sostituzione della libreria non riuscita.");
}
juce::String modeName(PlayMode mode)
{
    switch (mode)
    {
        case PlayMode::restart: return "Normale";
        case PlayMode::overlap: return "Spam";
        case PlayMode::toggleLoop: return "Loop on/off";
        case PlayMode::holdLoop: return "Loop premuto";
    }
    return "Normale";
}
juce::File findProjectRoot()
{
    auto directory = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    // A packaged portable app owns its data even when extracted inside a checkout.
    if (directory.getChildFile("BUILD-INFO.json").existsAsFile()) return directory;
    for (int i = 0; i < 7; ++i)
    {
        if (directory.getChildFile("CMakeLists.txt").existsAsFile() && directory.getChildFile("Source").isDirectory()) return directory;
        directory = directory.getParentDirectory();
    }
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
}
juce::Result runLibraryTests(const juce::File& tempRoot)
{
    const auto data = tempRoot.getChildFile("data");
    data.createDirectory();
    const auto source = data.getChildFile("source-library.json");
    const juce::String fixture = R"({"SchemaVersion":4,"Volume":0,"MonitorVolume":35,"DefaultMode":2,"Profiles":[{"Id":"p","Name":"Prova","Pads":[{"Id":"a","Name":"Uno","File":"../sounds/test.wav","Key":"001E:0","KeyLabel":"A","Volume":42,"Mode":1,"Speed":1.5,"TrimStart":0.2,"TrimEnd":1,"FadeOut":0.1},{"Id":"b","File":"../sounds/b.wav","Mode":0}]}],"ActiveProfileId":"p"})";
    source.replaceWithText(fixture);
    Library a;
    auto r = a.load(source);
    if (r.failed()) return r;
    if (a.totalPads() != 2 || a.outputVolume != 0 || a.current().pads[0].useGlobalMode || !a.current().pads[1].useGlobalMode)
        return juce::Result::fail("Schema 4 inheritance or volume mismatch");
    if (a.current().pads[0].file != tempRoot.getChildFile("sounds/test.wav")) return juce::Result::fail("Library relative path mismatch");
    const auto saved = data.getChildFile("library.json");
    a.keyboardDevice = "test-keyboard-path";
    a.inputDevice = "test-microphone"; a.microphoneEnabled = true;
    a.microphoneMonitor = true; a.microphoneVolume = 0.42f;
    a.current().pads[0].settings.filterCutoffHz = 1250.0f;
    a.current().pads[0].settings.overrideMasterVolume = true;
    a.current().pads[0].settings.filterMix = 0.25f;
    a.current().pads[0].settings.highPassCutoffHz = 725.0f;
    a.current().pads[0].settings.highPassMix = 0.6f;
    a.current().pads[0].settings.delayTimeMs = 480.0f;
    a.current().pads[0].settings.delayFeedback = 0.55f;
    a.current().pads[0].settings.delayMix = 0.6f;
    a.current().pads[0].settings.reverbDecay = 1.3f;
    a.current().pads[0].settings.reverbMix = 0.4f;
    a.current().pads[0].settings.compressorEnabled = true;
    a.current().pads[0].settings.compressorThresholdDb = -18.0f;
    a.current().pads[0].settings.compressorRatio = 3.0f;
    a.current().pads[0].settings.tremoloEnabled = true;
    a.current().pads[0].settings.tremoloSpeedHz = 3.5f;
    a.current().pads[0].settings.tremoloDepth = 0.7f;
    a.current().pads[0].settings.distortionEnabled = true;
    a.current().pads[0].settings.distortionDrive = 65.0f;
    a.current().pads[0].settings.pingPongTimeMs = 375.0f;
    a.current().pads[0].settings.pingPongFeedback = 0.45f;
    a.current().pads[0].settings.pingPongMix = 0.5f;
    a.current().pads[0].settings.chorusSpeedHz = 2.5f;
    a.current().pads[0].settings.chorusDepth = 0.8f;
    a.current().pads[0].settings.chorusMix = 0.25f;
    r = a.save(saved); if (r.failed()) return r;
    Library b; r = b.load(saved); if (r.failed()) return r;
    if (b.totalPads() != 2 || b.current().pads[0].key != "001E:0" || b.current().pads[0].settings.speed != 1.5 || b.defaultMode != PlayMode::toggleLoop || b.keyboardDevice != a.keyboardDevice)
        return juce::Result::fail("Round trip mismatch");
    if (b.inputDevice != a.inputDevice || !b.microphoneEnabled || !b.microphoneMonitor
        || std::abs(b.microphoneVolume - a.microphoneVolume) > 0.0001f
        || std::abs(b.current().pads[0].settings.filterCutoffHz - 1250.0f) > 0.001f
        || !b.current().pads[0].settings.overrideMasterVolume
        || b.current().pads[1].settings.overrideMasterVolume
        || std::abs(b.current().pads[0].settings.filterMix - 0.25f) > 0.0001f
        || std::abs(b.current().pads[0].settings.highPassCutoffHz - 725.0f) > 0.001f
        || std::abs(b.current().pads[0].settings.highPassMix - 0.6f) > 0.0001f
        || std::abs(b.current().pads[0].settings.delayTimeMs - 480.0f) > 0.001f
        || std::abs(b.current().pads[0].settings.delayFeedback - 0.55f) > 0.0001f
        || std::abs(b.current().pads[0].settings.delayMix - 0.6f) > 0.0001f
        || std::abs(b.current().pads[0].settings.reverbDecay - 1.3f) > 0.0001f
        || std::abs(b.current().pads[0].settings.reverbMix - 0.4f) > 0.0001f
        || !b.current().pads[0].settings.compressorEnabled
        || std::abs(b.current().pads[0].settings.compressorThresholdDb + 18.0f) > 0.0001f
        || std::abs(b.current().pads[0].settings.compressorRatio - 3.0f) > 0.0001f
        || !b.current().pads[0].settings.tremoloEnabled
        || std::abs(b.current().pads[0].settings.tremoloSpeedHz - 3.5f) > 0.0001f
        || std::abs(b.current().pads[0].settings.tremoloDepth - 0.7f) > 0.0001f
        || !b.current().pads[0].settings.distortionEnabled
        || std::abs(b.current().pads[0].settings.distortionDrive - 65.0f) > 0.0001f
        || std::abs(b.current().pads[0].settings.pingPongTimeMs - 375.0f) > 0.0001f
        || std::abs(b.current().pads[0].settings.pingPongFeedback - 0.45f) > 0.0001f
        || std::abs(b.current().pads[0].settings.pingPongMix - 0.5f) > 0.0001f
        || std::abs(b.current().pads[0].settings.chorusSpeedHz - 2.5f) > 0.0001f
        || std::abs(b.current().pads[0].settings.chorusDepth - 0.8f) > 0.0001f
        || std::abs(b.current().pads[0].settings.chorusMix - 0.25f) > 0.0001f)
        return juce::Result::fail("Schema 10 per-pad effect settings mismatch");
    const auto savedJson = juce::JSON::parse(saved);
    if (static_cast<int>(savedJson["SchemaVersion"]) != 10 || savedJson.hasProperty("IndividualVolumes"))
        return juce::Result::fail("Schema 10 per-pad effects not saved");
    auto schemaNineJson = juce::JSON::parse(saved);
    put(schemaNineJson, "SchemaVersion", 9);
    const auto schemaNineFile = data.getChildFile("schema-9.json");
    schemaNineFile.replaceWithText(juce::JSON::toString(schemaNineJson));
    Library schemaNine; r = schemaNine.load(schemaNineFile);
    if (r.failed() || schemaNine.current().pads[0].settings.reverbMix != 0.0f
        || schemaNine.current().pads[0].settings.compressorEnabled
        || schemaNine.current().pads[0].settings.pingPongMix != 0.0f)
        return juce::Result::fail("Schema 9 effect defaults mismatch");
    auto schemaEightJson = juce::JSON::parse(saved);
    put(schemaEightJson, "SchemaVersion", 8);
    put(schemaEightJson, "IndividualVolumes", true);
    const auto schemaEightFile = data.getChildFile("schema-8.json");
    schemaEightFile.replaceWithText(juce::JSON::toString(schemaEightJson));
    Library schemaEight; r = schemaEight.load(schemaEightFile);
    if (r.failed() || !schemaEight.current().pads[0].settings.overrideMasterVolume
        || !schemaEight.current().pads[1].settings.overrideMasterVolume)
        return juce::Result::fail("Schema 8 global volume migration mismatch");
    auto schemaSevenJson = juce::JSON::parse(saved);
    put(schemaSevenJson, "SchemaVersion", 7);
    auto* schemaSevenPad = schemaSevenJson["Profiles"][0]["Pads"][0].getDynamicObject();
    schemaSevenPad->removeProperty("HighPassCutoffHz");
    schemaSevenPad->removeProperty("HighPassMix");
    const auto schemaSevenFile = data.getChildFile("schema-7.json");
    schemaSevenFile.replaceWithText(juce::JSON::toString(schemaSevenJson));
    Library schemaSeven; r = schemaSeven.load(schemaSevenFile);
    if (r.failed() || schemaSeven.current().pads[0].settings.highPassCutoffHz != 100.0f
        || schemaSeven.current().pads[0].settings.highPassMix != 0.0f)
        return juce::Result::fail("Schema 7 high-pass defaults mismatch");
    const auto expectedCutoff = static_cast<float>(18000.0 * std::pow(300.0 / 18000.0, 0.25));
    for (const int oldSchema : {5, 6})
    {
        auto legacyJson = juce::JSON::parse(saved);
        put(legacyJson, "SchemaVersion", oldSchema);
        auto* legacyPad = legacyJson["Profiles"][0]["Pads"][0].getDynamicObject();
        legacyPad->setProperty("FilterAmount", 25);
        legacyPad->setProperty("EchoAmount", 60);
        const auto legacyFile = data.getChildFile("legacy" + juce::String(oldSchema) + ".json");
        legacyFile.replaceWithText(juce::JSON::toString(legacyJson));
        Library legacy; r = legacy.load(legacyFile);
        if (r.failed() || std::abs(legacy.current().pads[0].settings.filterCutoffHz - expectedCutoff) > 0.01f
            || legacy.current().pads[0].settings.filterMix != 1.0f
            || legacy.current().pads[0].settings.highPassMix != 0.0f
            || std::abs(legacy.current().pads[0].settings.delayMix - 0.6f) > 0.0001f
            || legacy.current().pads[0].settings.delayTimeMs != 220.0f
            || std::abs(legacy.current().pads[0].settings.delayFeedback - 0.35f) > 0.0001f)
            return juce::Result::fail("Old effect migration mismatch: schema " + juce::String(oldSchema));
    }
    if (juce::File::isAbsolutePath(savedJson["Profiles"][0]["Pads"][0]["File"].toString()))
        return juce::Result::fail("Local audio path is not portable");
    const auto movedRoot = tempRoot.getChildFile("moved");
    const auto movedData = movedRoot.getChildFile("data/library.json"); movedData.getParentDirectory().createDirectory();
    saved.copyFileTo(movedData);
    Library moved; r = moved.load(movedData);
    if (r.failed() || moved.current().pads[0].file != movedRoot.getChildFile("sounds/test.wav"))
        return juce::Result::fail("Library relocation mismatch");
    const auto recovered = tempRoot.getChildFile("sounds/test.wav"); recovered.getParentDirectory().createDirectory(); recovered.replaceWithText("fixture");
    a.current().pads[0].file = tempRoot.getChildFile("old-missing-root/sounds/test.wav");
    a.current().pads[1].file = tempRoot.getChildFile("external-still-missing/b.wav");
    const auto external = a.current().pads[1].file;
    r = a.save(saved); if (r.failed()) return r;
    r = b.load(saved);
    if (r.failed() || b.current().pads[0].file != a.current().pads[0].file || b.current().pads[1].file != external)
        return juce::Result::fail("External folder reference was changed");
    auto previousSchema = juce::JSON::parse(saved); put(previousSchema, "SchemaVersion", 3);
    saved.replaceWithText(juce::JSON::toString(previousSchema));
    r = b.load(saved);
    if (r.failed() || b.current().pads[0].file != recovered)
        return juce::Result::fail("Previous version local recovery mismatch");
    if (source.loadFileAsString() != fixture) return juce::Result::fail("Source library was changed");
    const auto unsupported = data.getChildFile("profile.json");
    unsupported.replaceWithText("{\"Profiles\":[{\"Name\":\"Old\",\"Pads\":[]}]}");
    if (b.load(unsupported).wasOk() || b.totalPads() != 2) return juce::Result::fail("Legacy preferences accepted");
    const auto broken = data.getChildFile("broken.json"); broken.replaceWithText("{}");
    if (b.load(broken).wasOk() || b.totalPads() != 2) return juce::Result::fail("Invalid load changed library");
    return juce::Result::ok();
}
}
