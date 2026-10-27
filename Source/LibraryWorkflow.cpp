#include "MainComponent.h"

namespace dualdeck
{
namespace
{
class SaveJob final : public juce::ThreadPoolJob
{
public:
    explicit SaveJob(std::function<void()> function) : ThreadPoolJob("Save library"), work(std::move(function)) {}
    JobStatus runJob() override { work(); return jobHasFinished; }
private:
    std::function<void()> work;
};
}

void MainComponent::save()
{
    if (smokeTest) return;
    if (!writable) { dirty = true; return; }
    auto snapshot = std::make_shared<Library>(library);
    const auto generation = saveGeneration.fetch_add(1) + 1;
    dirty = false;
    {
        const juce::ScopedLock lock(saveLock);
        pendingSave = std::move(snapshot);
        pendingSaveGeneration = generation;
        if (saveWorkerQueued) return;
        saveWorkerQueued = true;
    }
    juce::Component::SafePointer<MainComponent> safe(this);
    saveLoader.addJob(new SaveJob([this, safe]
    {
        for (;;)
        {
            std::shared_ptr<Library> next;
            std::uint64_t generation = 0;
            {
                const juce::ScopedLock lock(saveLock);
                if (!pendingSave) { saveWorkerQueued = false; return; }
                next = std::move(pendingSave);
                generation = pendingSaveGeneration;
            }
            if (generation != saveGeneration.load()) continue;
            const auto result = next->save(libraryFile);
            if (result.wasOk()) savedGeneration.store(generation);
            juce::MessageManager::callAsync([safe, generation, result]
            {
                if (!safe || generation != safe->saveGeneration.load()) return;
                if (result.failed())
                {
                    safe->dirty = true;
                    safe->dirtyTicks = 0;
                    safe->setStatus(result.getErrorMessage(), true);
                    if (safe->closePending)
                    {
                        safe->closePending = false;
                        auto onSaved = std::move(safe->closeReady);
                        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon,
                            "Libreria non salvata", result.getErrorMessage() + "\n\nRiprova a salvare prima di uscire?",
                            "Riprova", "Annulla", safe.getComponent(),
                            juce::ModalCallbackFunction::create([safe, onSaved = std::move(onSaved)](int answer)
                            {
                                if (safe && answer == 1) safe->requestClose(onSaved);
                            }));
                    }
                    return;
                }
                if (safe->closePending)
                {
                    if (safe->dirty) { safe->save(); return; }
                    safe->closePending = false;
                    auto onSaved = std::move(safe->closeReady);
                    if (onSaved) onSaved();
                }
            });
        }
    }), true);
}

void MainComponent::requestClose(std::function<void()> onSaved)
{
    if (closePending) return;
    if (!writable && dirty)
    {
        closePending = true;
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon,
            "Modifiche non salvate", "La libreria esistente non è leggibile. Le modifiche di questa sessione andranno perse. Uscire comunque?",
            "Esci senza salvare", "Resta", this,
            juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<MainComponent>(this), onSaved = std::move(onSaved)](int answer)
            {
                if (!safe) return;
                safe->closePending = false;
                if (answer == 1 && onSaved) onSaved();
            }));
        return;
    }
    if (smokeTest || !writable || (!dirty && savedGeneration.load() == saveGeneration.load()))
    {
        if (onSaved) onSaved();
        return;
    }
    closePending = true;
    closeReady = std::move(onSaved);
    setStatus("Salvataggio della libreria prima della chiusura...");
    save();
}

void MainComponent::removeCurrentProfile()
{
    if (!writable) { setStatus("Libreria non scrivibile: impossibile rimuovere profili.", true); return; }
    const auto id = library.current().id, name = library.current().name;
    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon, "Rimuovere il profilo?",
        "Rimuovere \"" + name + "\" e le sue associazioni?\nI file audio nelle cartelle rimarranno sul disco.\nSe rimuovi l'ultimo profilo, ne viene creato uno vuoto.",
        "Rimuovi", "Annulla", this,
        juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<MainComponent>(this), id](int answer)
        {
            if (!safe || answer != 1) return;
            safe->stopAll();
            if (safe->scanningProfile == id && safe->scanCancelled) safe->scanCancelled->store(true);
            if (!safe->library.removeProfile(id)) return;
            safe->selection = -1; safe->page = 0; safe->refresh();
            safe->save();
            safe->setStatus("Profilo rimosso. Salvataggio in corso; i file audio restano nelle cartelle originali.");
            if (!safe->scanningFolders && !safe->library.current().folders.empty()) safe->scanProfileFolders(safe->library.current().id);
        }));
}

void MainComponent::renameCurrentProfile()
{
    if (!writable) { setStatus("Libreria non scrivibile: impossibile rinominare profili.", true); return; }
    const auto id = library.current().id;
    auto* dialog = new juce::AlertWindow("Rinomina profilo", "Scegli un nome (massimo 80 caratteri).", juce::MessageBoxIconType::NoIcon, this);
    dialog->setLookAndFeel(&look);
    dialog->addTextEditor("name", library.current().name, "Nome");
    dialog->getTextEditor("name")->setInputRestrictions(80);
    dialog->addButton("Salva", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Annulla", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [safe = juce::Component::SafePointer<MainComponent>(this), dialog, id](int answer)
        {
            if (!safe || answer != 1) return;
            const auto result = safe->library.renameProfile(id, dialog->getTextEditorContents("name"));
            if (result.failed()) { safe->setStatus(result.getErrorMessage(), true); return; }
            safe->refresh();
            safe->save();
            safe->setStatus("Profilo rinominato. Salvataggio in corso.");
        }), true);
}
}
