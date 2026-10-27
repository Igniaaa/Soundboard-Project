# Editor visivo della UI (build Debug)

Esegui `./Build-UiDebug.ps1`, poi apri `DualDeckJuce-UI-Debug.exe`. La build usa una cartella di compilazione separata e non sostituisce l'eseguibile normale.

- I pulsanti in alto cambiano schermata; **Editor suono** apre un'anteprima del pannello degli effetti.
- Clicca un controllo per selezionarlo. Trascinalo per spostarlo; trascina l'angolo in basso a destra per ridimensionarlo.
- Premi **Ctrl+Z** o **Annulla** per annullare l'ultima modifica. Un intero trascinamento conta come un'operazione; puoi annullare anche **Ripristina**. La cronologia conserva fino a 100 operazioni della sessione.
- **Salva layout** scrive gli spostamenti in `data/ui-debug-layout.json`. Il file viene riletto all'avvio. **Ripristina** torna al layout di codice; usa **Salva layout** per rendere permanente il ripristino.
- L'asterisco indica modifiche non ancora salvate. Il file contiene differenze rispetto alle posizioni calcolate da JUCE, così i controlli continuano a seguire il ridimensionamento della finestra.

L'overlay intercetta i clic, quindi i controlli dell'app non attivano suoni mentre sistemi la UI. La build di debug non apre i dispositivi audio e non salva modifiche alla libreria. L'editor modifica la geometria dei componenti JUCE; sfondi e decorazioni dipinti direttamente nel codice restano fuori dall'editor. Il layout JSON appartiene alla build di debug: per incorporare un layout nel programma normale, occorre riportare le differenze nel codice di `MainComponent::resized()` o `PadEditorPanel::resized()`.
