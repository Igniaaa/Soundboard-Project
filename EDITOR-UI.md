# Editor visivo della UI (build Debug)

Lo script `Build-UiDebug.ps1` è stato rimosso. L'editor opzionale si può ancora
compilare dai sorgenti con CMake, usando gli strumenti Microsoft descritti in
[BUILDING.md](BUILDING.md). Dopo il setup, eseguire dalla cartella del progetto:

```powershell
.\tools\cmake-4.4.4-windows-x86_64\bin\cmake.exe -S . -B build-ui-debug -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug -DDUALDECK_UI_EDITOR=ON -DBUILD_TESTING=OFF
.\tools\cmake-4.4.4-windows-x86_64\bin\cmake.exe --build build-ui-debug --config Debug --target DualDeckJuce --parallel 3
```

Con questo generatore, l'eseguibile è `build-ui-debug/bin/Debug/DualDeckJuce.exe`.
La cartella è separata da quella Release. Se `build-ui-debug` contiene già una
configurazione Ninja, scegliere una nuova cartella per entrambi i comandi.
Questa build di sviluppo è distinta dal pacchetto pubblico di anteprima.

- I pulsanti in alto cambiano schermata; **Editor suono** apre un'anteprima del pannello degli effetti.
- Clicca un controllo per selezionarlo. Trascinalo per spostarlo; trascina l'angolo in basso a destra per ridimensionarlo.
- Premi **Ctrl+Z** o **Annulla** per annullare l'ultima modifica. Un intero trascinamento conta come un'operazione; puoi annullare anche **Ripristina**. La cronologia conserva fino a 100 operazioni della sessione.
- **Salva layout** scrive gli spostamenti in `data/ui-debug-layout.json`. Il file viene riletto all'avvio. **Ripristina** torna al layout di codice; usa **Salva layout** per rendere permanente il ripristino.
- L'asterisco indica modifiche non ancora salvate. Il file contiene differenze rispetto alle posizioni calcolate da JUCE, così i controlli continuano a seguire il ridimensionamento della finestra.

L'overlay intercetta i clic, quindi i controlli dell'app non attivano suoni mentre sistemi la UI. La build di debug non apre i dispositivi audio e non salva modifiche alla libreria. L'editor modifica la geometria dei componenti JUCE; sfondi e decorazioni dipinti direttamente nel codice restano fuori dall'editor. Il layout JSON appartiene alla build di debug: per incorporare un layout nel programma normale, occorre riportare le differenze nel codice di `MainComponent::resized()` o `PadEditorPanel::resized()`.
