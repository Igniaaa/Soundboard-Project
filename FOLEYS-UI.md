# Aspetto di DualDeck con Foleys GUI Magic

La build Foleys consente di personalizzare soundboard, navigazione, tastiera
grafica, impostazioni della tastiera dedicata ed editor audio ed effetti.
La build normale resta separata e include anch'essa la barra dei menu classica.

## Usare i menu

- **File**: cartelle audio, profili, salvataggio della libreria e uscita.
- **Modifica**: annulla/ripeti, appunti per il campo di testo attivo e comandi del
  suono selezionato.
- **Visualizza**: schermate, vista pad/tastiera e personalizzazione dell'aspetto.
- **Strumenti**: aggiornamento di dispositivi/tastiere e arresto dei suoni.
- **Aiuto**: guida rapida e informazioni sulla versione.

Alt+F, Alt+M, Alt+V, Alt+S e Alt+A aprono i rispettivi menu.

## Modificare la schermata principale senza terminale

1. Nella soundboard scegli **Visualizza > Personalizza schermata**.
2. Seleziona direttamente un controllo oppure cercalo nell'albero Foleys:
   **Button**, **ComboBox**, **Slider**, **ToggleButton**, **TextEditor**, **Label**,
   **Pad**, **Keyboard** e **Meter** sono elementi collegati ai controlli reali.
3. Modifica colori, caratteri, dimensioni e margini. I contenitori **View**
   dispongono gli elementi con FlexBox; **PadGrid** gestisce colonne, spaziatura e
   dimensione minima dei pad. I cambiamenti agiscono subito sulla schermata.
4. Scegli **Visualizza > Salva aspetto**, poi **Termina personalizzazione**.
   I comandi restano nella barra dei menu, fuori dal layout modificabile.

Durante l'editing diretto i clic selezionano gli elementi. Al termine, gli stessi
controlli riprendono l'uso normale con valori, selezioni e comandi conservati.
Il layout iniziale è in **Assets/MainScreen.xml**; quello personale viene
salvato in **data/foleys-main-ui.xml** e ricaricato agli avvii successivi.

Il menu **File** del designer principale offre **Importa layout**, **Salva layout**
e **Ripristina layout**. L'importazione verifica il file prima di applicarlo e
non lo sovrascrive: usa Salva per conservarlo nella cartella dell'app.

## Tema comune, pagina audio ed editor degli effetti

**Visualizza > Tema e controlli del programma** apre il tema comune: seleziona
**AppTheme (programTheme)** per palette, scala dei caratteri e raggi dei controlli.
L'editor degli effetti mantiene l'anteprima di personalizzazione: apri prima
**Audio ed effetti**, poi **Visualizza > Personalizza schermata**. In questa
anteprima i controlli sono immagini selezionabili e le proprietà **AppControl**
agiscono sui controlli dell'editor; gli offset restano relativi al suo layout.

Nella pagina **Audio e uscite**, **Personalizza schermata** apre il designer del
layout audio già integrato. Tema/editor e pagina audio conservano rispettivamente
**data/foleys-program-ui.xml** e **data/foleys-audio-ui.xml**.

**Salva aspetto** e **Ripristina aspetto** agiscono sull'editor attivo: layout
principale, pagina audio oppure tema/editor. Il ripristino dal menu chiede
conferma. Non servono terminale o ricompilazione e i file dell'aspetto sono
separati da suoni, associazioni e impostazioni audio.

Alla prima conversione si recuperano colori, caratteri, stili per ID e riferimenti
alla palette dall'aspetto precedente. La disposizione riparte dal nuovo layout
FlexBox/griglia; i vecchi offset vengono conservati come archivio e il file
precedente resta intatto. Mantieni gli ID degli elementi per conservare i
collegamenti alle loro funzioni.

## Compilazione per lo sviluppo

Con gli strumenti già predisposti da Setup.ps1:

```powershell
.\Setup-Foleys.ps1
.\Build.ps1 -FoleysAudioUi -NoPromote
.\build-foleys\bin\DualDeckJuce.exe
.\Test.ps1 -BuildDirectory build-foleys -OutputDirectory test-output/foleys
```

L'eseguibile normale non viene sostituito. La libreria e le impostazioni
restano quelle del progetto: le modifiche effettuate nell'app vengono salvate
normalmente. I controlli e gli snapshot automatici usano invece la modalità di
test, senza aprire dispositivi o salvare la libreria personale.

## Layout incorporato e designer

Il layout iniziale è in **Assets/AudioSettings.xml**, incorporato da CMake. Il
file personalizzato nella cartella data ha precedenza quando è valido.

La build `-FoleysAudioUi` include il designer e i comandi nell'interfaccia.
Per mantenere anche una variante nella cartella di sviluppo separata:

```powershell
.\Build.ps1 -FoleysDesigner -NoPromote
.\build-foleys-designer\bin\DualDeckJuce.exe
```

Chiudere eventuali altre istanze prima di aprire una build diversa. Il parametro
opzionale `--foleys-editor` apre direttamente il designer; non è necessario per
usare i comandi nell'interfaccia. La build normale senza Foleys resta separata.

## Integrazione

MainScreenPanel possiede MagicGUIState e MagicGUIBuilder. Le fabbriche tipizzate
creano GuiItem che ospitano i controlli esistenti senza acquisirne la proprietà.
MainComponent conserva stato e callback; il layout XML dispone i controlli reali.
PadGrid è un Container Foleys specializzato per la griglia adattiva. La tastiera e
il misuratore sono componenti dedicati ospitati da GuiItem. La pagina audio è
annidata come AudioPage e mantiene il suo AudioSettingsPanel.

ProgramAppearance conserva il tema comune e l'anteprima dell'editor degli effetti.
Gli elementi della schermata principale escono dal suo albero attivo e vengono
archiviati durante la migrazione, evitando che due editor ne controllino lo stile.
UiTheme e DeckLookAndFeel applicano il tema ai controlli e ai disegni dedicati.

I file vengono verificati prima dell'importazione e del salvataggio, scritto con
sostituzione atomica. Servono tutti gli ID registrati con i tipi corretti. Un file
non valido carica il layout iniziale, segnala l'errore e conserva l'originale; un
salvataggio fallito conserva l'ultima personalizzazione valida. Menus e comandi
di recupero restano esterni al layout modificabile.

## Dipendenza

Repository: https://github.com/ffAudio/foleys_gui_magic

Commit: `d53d998e2bc3fb3ca394f1ee5b0c2f3886a67331` (modulo 1.5.0).

Archivio: https://codeload.github.com/ffAudio/foleys_gui_magic/zip/d53d998e2bc3fb3ca394f1ee5b0c2f3886a67331

SHA-256: `2B7797725272193E54C9BEC660E30918A6171C00B510979FA7CE86E21AAA3DDF`.

Il setup verifica l'archivio prima di estrarlo. Il modulo viene compilato dalla
cartella deps, senza modifiche locali. La licenza BSD 3-Clause è conservata in
Assets/Foleys-LICENSE.txt e copiata accanto all'eseguibile della prova.
