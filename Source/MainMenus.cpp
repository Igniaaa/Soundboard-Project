#include "MainComponent.h"
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
#include "AudioSettingsPanel.h"
#endif

namespace dualdeck
{
juce::StringArray MainComponent::getMenuBarNames() { return {"File", "Modifica", "Visualizza", "Strumenti", "Aiuto"}; }

juce::PopupMenu MainComponent::getMenuForIndex(int index, const juce::String&)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(&look);
    const bool hasPad = selectedPad() != nullptr;
    switch (index)
    {
        case 0:
            menu.addItem(importFolder, "Collega cartella...", importButton.isEnabled());
            menu.addItem(updateFolders, refreshFolders.getButtonText(), refreshFolders.isEnabled());
            menu.addSeparator();
            menu.addItem(createProfile, "Nuovo profilo");
            menu.addItem(renameProfileAction, "Rinomina profilo...");
            menu.addItem(removeProfile, "Rimuovi profilo...");
            menu.addSeparator(); menu.addItem(saveLibrary, "Salva libreria", writable);
            menu.addSeparator(); menu.addItem(quitApp, "Esci");
            break;
        case 1:
        {
            auto* focus = juce::Component::getCurrentlyFocusedComponent();
            menuTextTarget = nullptr;
            while (focus != nullptr)
            {
                if (auto* text = dynamic_cast<juce::TextEditor*>(focus)) { menuTextTarget = text; break; }
                focus = focus->getParentComponent();
            }
            const auto* text = menuTextTarget.getComponent();
            const bool editable = text != nullptr && !text->isReadOnly();
            const bool selected = text != nullptr && !text->getHighlightedRegion().isEmpty();
            menu.addItem(undoText, "Annulla", editable);
            menu.addItem(redoText, "Ripeti", editable);
            menu.addSeparator(); menu.addItem(cutText, "Taglia", editable && selected);
            menu.addItem(copyText, "Copia", selected);
            menu.addItem(pasteText, "Incolla", editable);
            menu.addItem(selectAllText, "Seleziona tutto", text != nullptr);
            menu.addSeparator(); menu.addItem(editSound, "Audio ed effetti...", hasPad);
            menu.addItem(replaceSound, "Cambia file audio...", hasPad);
            menu.addItem(assignKey, "Associa un tasto...", hasPad);
            menu.addItem(clearKey, "Libera il tasto", hasPad && selectedPad()->key.isNotEmpty());
            break;
        }
        case 2:
            menu.addItem(showBoard, "Soundboard", true, view == 0);
            menu.addItem(showPads, "Vista pad", true, view == 0 && !keyboardMode);
            menu.addItem(showKeyboard, "Tastiera grafica", true, view == 0 && keyboardMode);
            menu.addSeparator(); menu.addItem(showAudio, "Audio e uscite", true, view == 1);
            menu.addItem(showDedicatedKeyboard, "Tastiera dedicata", true, view == 2);
#if defined(DUALDECK_FOLEYS_DESIGNER)
            menu.addSeparator(); menu.addItem(customiseScreen, "Personalizza schermata...");
            menu.addItem(customiseProgram, "Tema e controlli del programma...");
            menu.addItem(saveStyle, "Salva aspetto");
            menu.addItem(finishStyle, "Termina personalizzazione");
            menu.addItem(resetStyle, "Ripristina aspetto...");
#endif
            break;
        case 3:
            menu.addItem(rescanAudio, "Aggiorna dispositivi audio");
            menu.addItem(rescanKeyboards, "Aggiorna tastiere");
            menu.addSeparator(); menu.addItem(stopPlayback, "Ferma tutti i suoni"); break;
        case 4:
            menu.addItem(showHelp, "Guida rapida"); menu.addItem(showAbout, "Informazioni su DualDeck"); break;
        default: break;
    }
    return menu;
}

void MainComponent::menuItemSelected(int action, int)
{
    if (action >= undoText && action <= selectAllText)
    {
        if (auto* text = menuTextTarget.getComponent())
        {
            if (!text->isReadOnly())
                switch (action)
                {
                    case undoText: text->undo(); break;
                    case redoText: text->redo(); break;
                    case cutText: text->cut(); break;
                    case pasteText: text->paste(); break;
                    default: break;
                }
            if (action == copyText) text->copy();
            if (action == selectAllText) text->selectAll();
            text->grabKeyboardFocus();
        }
        return;
    }
    switch (action)
    {
        case importFolder: if (importButton.isEnabled()) chooseAudioFolder(); break;
        case updateFolders: if (refreshFolders.onClick) refreshFolders.onClick(); break;
        case createProfile: if (newProfile.onClick) newProfile.onClick(); break;
        case renameProfileAction: renameCurrentProfile(); break;
        case removeProfile: removeCurrentProfile(); break;
        case saveLibrary:
            if (writable) { save(); setStatus("Salvataggio libreria richiesto."); }
            else setStatus("La libreria non e disponibile in scrittura.", true);
            break;
        case quitApp: if (auto* app = juce::JUCEApplication::getInstance()) app->systemRequestedQuit(); break;
        case editSound: if (selectedPad()) openPadEditor(); break;
        case replaceSound: if (selectedPad()) replaceAudioFile(); break;
        case assignKey: if (bindButton.onClick) bindButton.onClick(); break;
        case clearKey: if (unbindButton.onClick) unbindButton.onClick(); break;
        case showBoard: setView(0); break;
        case showPads: keyboardMode = false; setView(0); break;
        case showKeyboard: keyboardMode = true; setView(0); break;
        case showAudio: setView(1); break;
        case showDedicatedKeyboard: setView(2); break;
#if defined(DUALDECK_FOLEYS_DESIGNER)
        case customiseScreen:
            if (editorViewport.isVisible()) showProgramAppearance();
            else if (view == 1) showAudioUiDesigner();
            else showMainScreenDesigner();
            break;
        case customiseProgram: showProgramAppearance(); break;
        case saveStyle: saveCurrentAppearance(); break;
        case resetStyle: resetCurrentAppearance(); break;
        case finishStyle: hideAppearanceEditors(); break;
#endif
        case rescanAudio: refreshDevices(); setStatus("Dispositivi audio aggiornati."); break;
        case rescanKeyboards: if (scanKeyboards.onClick) scanKeyboards.onClick(); break;
        case stopPlayback: stopAll(); break;
        case showHelp:
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Guida rapida",
                "Collega una cartella dal menu File, seleziona un pad e apri Audio ed effetti dal menu Modifica.\n\n"
                "Usa Visualizza per passare tra pad, tastiera e impostazioni e per personalizzare l'aspetto.", "Chiudi", this); break;
        case showAbout:
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "DualDeck",
                "DualDeck " DUALDECK_VERSION "\nSoundboard e tastiera audio.\n\nJUCE 9.0.4"
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
                "\nFoleys GUI Magic 1.5.0"
#endif
                , "Chiudi", this); break;
        default: break;
    }
    menuItemsChanged();
}
}
