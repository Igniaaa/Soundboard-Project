# DualDeck

DualDeck è una **soundboard per Windows x64**: organizzi i tuoi file audio in
profili e cartelle e li riproduci con un clic o con i tasti associati. Ogni suono
ha un pulsante, chiamato **pad**, con impostazioni ed effetti propri.

Puoi inviare i suoni a un'app come Discord, ascoltarli in cuffia e aggiungere
la voce del microfono. Puoi anche dedicare una seconda tastiera ai suoni,
continuando a usare quella principale normalmente.

**Versione attuale: 1.0.0-rc1.** È una candidata al rilascio, cioè una versione
di anteprima da collaudare prima della 1.0 stabile. Lo stato delle prove e le
attività ancora aperte sono nella [checklist di rilascio](RELEASE-CHECKLIST.md).

## Cosa puoi fare

- **Organizzare i suoni:** profili multipli, cartelle collegate, ricerca e
  preferiti. I file audio restano nella loro posizione, senza essere copiati.
- **Riprodurli come preferisci:** riavvio, sovrapposizione di più copie o loop,
  con impostazioni generali ed eccezioni per ogni pad.
- **Modificarne l'ascolto:** volume, velocità, tagli e dissolvenze, senza
  alterare i file originali.
- **Usare l'editor audio:** forma d'onda, anteprima, loop tra due punti e
  ascolto trascinando il cursore sulla forma d'onda.
- **Aggiungere effetti:** filtri, delay, riverbero, compressore, tremolo,
  distorsione, ping-pong e chorus.
- **Gestire le uscite:** uscita principale e ascolto in cuffia con volumi
  separati, più il mix opzionale del microfono.

Sono supportati WAV, MP3, FLAC, OGG e AIFF. M4A/AAC e altri formati dipendono
dai codec disponibili in Windows. Per ogni file vengono usati al massimo i
**primi 180 secondi**; l'originale rimane intatto.

## Avvio rapido

Se hai il **pacchetto ZIP Windows già compilato**:

1. Estrai tutto lo ZIP in una cartella scrivibile, per esempio nei Documenti.
   Non avviare il programma direttamente dallo ZIP.
2. Apri **DualDeck.exe**. Per usare la soundboard non servono Python,
   compilatori o privilegi amministrativi.
3. Premi **Collega cartella** e scegli una cartella con i tuoi audio. Puoi
   anche trascinare una cartella nella finestra.
4. Apri **Audio e uscite**, scegli l'uscita principale e, se vuoi, le cuffie.
   Premi **Applica uscite**.
5. Attendi che termini **Preparazione suoni**, poi premi un pad per ascoltarlo.

Se hai scaricato soltanto i sorgenti, segui la sezione
[Compilare il progetto](#compilare-il-progetto).

L'eseguibile dell'anteprima non è firmato digitalmente. Per aggiornamento,
recupero dei dati e rimozione consulta la [guida al primo avvio](GETTING-STARTED.md).

## Comandi essenziali

| Azione | Come fare |
| --- | --- |
| Riprodurre un suono | Clic sinistro sul pad |
| Aprire le azioni di un pad | Clic destro sul pad |
| Associare un tasto | Seleziona il pad, premi **Associa un tasto**, poi premi il tasto desiderato |
| Fermare tutti i suoni | Premi **STOP TUTTO** oppure **Esc**, quando il tasto viene ricevuto dall'app |
| Mettere in pausa temporaneamente | Tieni premuto **Pausa DJ** o il tasto DJ, inizialmente **Spazio**; rilascia per riprendere |
| Modificare tagli ed effetti | Apri **Audio ed effetti** del pad e usa **Salva impostazioni** per conservarli |

Le modalità di riproduzione cambiano il comportamento del pad:

| Modalità | Comportamento |
| --- | --- |
| **Normale** | Ogni pressione avvia il suono dall'inizio, interrompendo la copia precedente |
| **Spam** | Ogni pressione aggiunge una copia del suono in riproduzione |
| **Loop on/off** | Una pressione avvia il loop, la successiva lo ferma |
| **Loop premuto** | Il suono si ripete finché tieni premuto il pad o il tasto associato |

Senza la tastiera dedicata attiva, i comandi da tastiera funzionano quando
DualDeck è in primo piano. Per i dettagli sulle funzioni consulta
[Uso e funzioni](LEGGIMI.md), che contiene anche la cronologia delle versioni.

## Inviare suoni e voce a Discord

Per inviare l'audio a Discord serve un **cavo audio virtuale**, da predisporre
separatamente: DualDeck non ne include uno.

Con un cavo che espone i dispositivi **CABLE Input** e **CABLE Output**:

1. In DualDeck scegli **CABLE Input** come uscita principale.
2. Scegli le tue cuffie come uscita di ascolto e premi **Applica uscite**.
3. In Discord scegli **CABLE Output** come dispositivo di ingresso.
4. Se vuoi inviare anche la voce, seleziona e attiva il microfono fisico in
   **Audio e uscite** di DualDeck.

Se la voce passa da DualDeck, evita di inviarla anche attraverso un secondo
percorso: verrebbe duplicata. Il volume del microfono è separato dai volumi
dei suoni. Le due uscite possono avere latenze diverse; non è garantita una
sincronizzazione precisa tra dispositivi audio distinti.

## Usare una tastiera dedicata

Questa funzione permette di comandare i pad anche con un'altra applicazione
in primo piano, impedendo alla tastiera dedicata di scrivere nelle altre app.
La tastiera principale rimane disponibile per l'uso normale.

Il blocco selettivo richiede **Interception**, con driver installato separatamente
e DLL ufficiale x64 v1.0.1 nella cartella `keyboard-runtime` accanto all'eseguibile.
**La soundboard funziona anche senza questi componenti.**

Il pacchetto di anteprima non include né installa DLL, driver o installer
Interception. Contiene `keyboard-runtime/LEGGIMI.txt` con le istruzioni per
predisporre la DLL. I file del produttore sono nelle
[release ufficiali di Interception](https://github.com/oblitum/Interception/releases/latest).

Una volta predisposti i componenti:

1. Apri **Tastiera dedicata** e premi **Riconosci con F12**.
2. Premi e rilascia **F12 sulla tastiera che vuoi dedicare ai suoni**.
3. Rilascia tutti i tasti, attiva l'interruttore e attendi lo stato attivo.
4. Prova con Blocco note in primo piano: la dedicata deve comandare i pad
   senza scrivere, mentre la principale deve continuare a scrivere.
5. Disattiva l'interruttore per restituire alla dedicata l'uso normale.

Dopo l'avvio o un cambio di dispositivo, l'attivazione deve essere manuale.
La compatibilità e l'integrazione Interception hanno ancora verifiche aperte
prima della versione stabile. Consulta la [guida al primo avvio](GETTING-STARTED.md),
i [dettagli del blocco tastiera](TASTIERA-BLOCCO.md) e gli
[avvisi sulle dipendenze](THIRD-PARTY-NOTICES.md).

## Dati e backup

Nel pacchetto portatile, profili, impostazioni e associazioni sono salvati in
**`data/library.json`**, accanto all'eseguibile. Il backup automatico è
**`data/library.json.bak`**. Gli audio collegati restano nelle cartelle originali:
il solo backup della libreria non contiene i file audio.

Prima di aggiornare, chiudi DualDeck e conserva una copia della cartella `data`
e degli audio necessari. Mantieni `BUILD-INFO.json` insieme al pacchetto:
permette all'app di riconoscere la distribuzione portatile. La procedura completa
di aggiornamento e recupero è nella [guida al primo avvio](GETTING-STARTED.md).

## Compilare il progetto

DualDeck è scritto in **C++17 con JUCE 9.0.4**. Per compilare su Windows x64
servono PowerShell, Python 3.10+, Visual Studio o Build Tools 2022 con
**Desktop development with C++**, MSVC v143 x64 e Windows SDK. Il primo setup
richiede una connessione Internet.

Dalla cartella del progetto:

```powershell
.\Setup.ps1
.\Build.ps1 -Toolchain Installed -NoPromote -SkipTests
.\Test.ps1
python -m unittest discover -s Tests -p test_release.py
```

Il setup scarica JUCE, CMake e Ninja con verifica SHA-256; non installa
compilatori o driver. La build genera `build-msvc/bin/DualDeckJuce.exe`.
Con `-NoPromote` le copie locali approvate non vengono sostituite.

Per toolchain portatili già disponibili, test aggiuntivi e creazione dei
pacchetti consulta [Compilazione e packaging](BUILDING.md).
Il workflow GitHub compila, testa e prepara archivi di anteprima; non pubblica
automaticamente una release.

## Altra documentazione

| Documento | Contenuto |
| --- | --- |
| [Primo avvio](GETTING-STARTED.md) | Requisiti, configurazione, aggiornamento e recupero dei dati |
| [Uso e funzioni](LEGGIMI.md) | Dettagli dei controlli e cronologia delle versioni |
| [Checklist di rilascio](RELEASE-CHECKLIST.md) | Prove completate e attività ancora aperte per la 1.0 |
| [Novità](CHANGELOG.md) | Modifiche delle versioni |
| [Documentazione del codice](DOCUMENTAZIONE-CODICE.md) | Architettura, sorgenti e flussi dell'app |
| [Contribuire](CONTRIBUTING.md) | Come segnalare problemi e proporre modifiche |
| [Preparazione GitHub](GITHUB.md) | Repository e procedura di rilascio |
| [Editor visivo Debug](EDITOR-UI.md) | Strumenti di sviluppo dell'interfaccia |

## Licenza

Copyright (C) 2026 Alessandro Piermatteo. Il codice originale è distribuito
con licenza **AGPL-3.0-only**, senza garanzia. Consulta il [testo della licenza](LICENSE)
e le [informazioni su ambito e provenienza](COPYRIGHT.md).

JUCE e Interception hanno condizioni proprie, riportate negli
[avvisi sulle dipendenze](THIRD-PARTY-NOTICES.md) e nella
[documentazione della licenza JUCE](JUCE-LICENSE.md).
