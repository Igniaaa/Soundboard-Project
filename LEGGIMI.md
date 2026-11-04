# DualDeck — preparazione 1.0.0-rc1

La candidata open source è descritta in [README.md](README.md),
[GETTING-STARTED.md](GETTING-STARTED.md) e [BUILDING.md](BUILDING.md).
Codice originale: Copyright (C) 2026 Alessandro Piermatteo, AGPL-3.0-only;
vedere LICENSE, COPYRIGHT.md e THIRD-PARTY-NOTICES.md. Le sezioni cronologiche
sotto conservano i riscontri della 0.7, che resta la versione locale approvata.
Il pacchetto di anteprima non contiene DLL o driver Interception.

## Lettura durante la riproduzione — approvata il 1 novembre 2026

Avvia **DualDeckJuce-0.7.exe** oppure **DualDeckJuce.exe**. La versione prepara mezzo secondo
per ogni suono; il resto viene letto durante la riproduzione, senza caricare
tutto il file. Attendi che scompaia **Preparazione suoni** per avere pronti
anche i suoni sulle altre pagine. Riscontro dell'utente: risposta dei pad
quasi sempre sotto 10 ms e nessun artefatto percepito durante la riproduzione.

Il layout completo e il limite di 180 secondi sono conservati; i file
originali restano intatti. La memoria indicata dall'app include i campioni
audio, mentre Gestione attività include anche il resto del programma e i
decoder. Aprire l'editor carica ancora il suono completo, fino a 180 secondi,
per la forma d'onda e lo scrub, e può quindi aumentare la memoria occupata.
Le due copie principali sono identiche alla build provata, ora conservata in
**old-builds/DualDeckJuce-0.7-streaming-test.exe**. Anche le precedenti prove
di primo avvio e tastiera completa sono archiviate in **old-builds**.
Dettagli: [VERIFICHE-STREAMING.md](Changelogs/VERIFICHE-STREAMING.md).

## Preparazione del primo avvio — versione di prova, 31 ottobre 2026

Prova precedente, ora in **old-builds/DualDeckJuce-0.7-fast-start.exe**. I suoni del profilo venivano
preparati anche nella vista Tastiera: prima quelli associati ai tasti,
poi i pad visibili e infine gli altri che trovano spazio nella cache.
Attendi che scompaia **Preparazione suoni** prima della prova del primo avvio.
Il caricamento usa un worker separato e conserva il limite di 384 MiB;
un suono non preparato o espulso dalla cache deve ancora essere decodificato.
La libreria attuale supera quel limite: nella prova sono risultati pronti
93 suoni su 329, inclusi tutti e tre quelli associati ai tasti.
Il limite a 180 secondi, il layout completo e le associazioni sono conservati.
Misure e verifiche: [VERIFICHE-PRIMO-AVVIO.md](Changelogs/VERIFICHE-PRIMO-AVVIO.md).

## Tastiera completa al 100% — versione di prova, 31 ottobre 2026

Prova precedente conservata in **old-builds/DualDeckJuce-0.7-full-keyboard.exe**. Il layout italiano
ISO completo di 105 tasti: tastierino numerico, frecce, navigazione,
Backspace, ù, Shift destro, Win, Menu, Stamp, Scroll e Pausa.
I numeri e Invio del tastierino sono distinti dai tasti principali;
Bloc Num non cambia le associazioni fisiche. Nelle finestre strette puoi
scorrere la tastiera orizzontalmente. Clic destro o Maiusc+F10 associa
il tasto al pad selezionato; le frecce saltano gli spazi del layout.
Esc, F12 e il tasto Pausa DJ (Spazio per impostazione predefinita)
restano riservati. Pausa/Break produce una pressione e un rilascio immediati.
Con la tastiera dedicata disattivata i comandi richiedono il focus nell'app;
i normali effetti dei tasti in Windows restano attivi. Quando un pad o la tastiera grafica ha il focus, entrambi i tasti Invio attivano quel controllo senza avviare anche un altro pad associato; usa Associa un tasto per registrarli. Con Bloc Num spento il tastierino può anche spostare il focus grafico secondo il normale comportamento Windows. La cattura dedicata distingue i tasti senza queste azioni locali.

Il precedente riferimento con il solo taglio a 180 secondi è conservato in
**old-builds/DualDeckJuce-0.7-180s-test.exe**. La versione principale attuale
include anche il layout completo e conserva le associazioni già salvate.
Dettagli delle prove: [VERIFICHE-TASTIERA-COMPLETA.md](Changelogs/VERIFICHE-TASTIERA-COMPLETA.md).

## Taglio automatico a 180 secondi — 31 ottobre 2026

I file più lunghi di 180 secondi vengono caricati e riprodotti limitatamente
ai primi 3 minuti, anziché essere rifiutati. Il taglio vale anche nell'editor
e per i formati letti tramite i codec Windows. I file originali sul disco
rimangono intatti; la parte successiva ai 180 secondi non viene caricata.
Versione precedente approvata e conservata in **old-builds/DualDeckJuce-0.7-180s-test.exe**; il comportamento è incluso nella versione principale attuale.

## Mix meno attenuato — 30 ottobre 2026

Il bilanciamento delle copie meno aggressivo e il recupero più rapido
dopo i picchi sono approvati all'ascolto dall'utente. Avvia
**DualDeckJuce-0.7.exe** oppure **DualDeckJuce.exe**: entrambe le copie
sono aggiornate e identiche alla copia provata
**old-builds/DualDeckJuce-0.7-clean-mix.exe**.
Il freeze approvato è invariato; la copia
**old-builds/DualDeckJuce-0.7-freeze-range.exe** resta il riferimento precedente.
Dopo ogni approvazione, le copie di prova vengono spostate in `old-builds`;
in radice restano i due eseguibili principali della versione approvata.
Dettagli e limiti in [VERIFICHE-AUDIO-REVIEW.md](Changelogs/VERIFICHE-AUDIO-REVIEW.md).

## Revisione audio — 27 ottobre 2026 (storico)

La copia **DualDeckJuce-0.7-audio-review.exe** è conservata in `old-builds`
e usa gli stessi dati delle versioni successive.
Delay, ping-pong e riverbero mantengono il livello del suono diretto anche
al 100% di mix: il mix aggiunge le code senza dimezzare l'attacco.
La stabilizzazione del feedback rimane; quando la somma è troppo forte
interviene il limiter stereo, con breve mantenimento e recupero graduale
del volume. Non è aggiunto ritardo audio. Corrette le discontinuità della coda a fine file e il taglio
anticipato del delay. Rivista la componente rumorosa di scrub/freeze e
ridotto il costo dei raccordi. Il riverbero ora diffonde le riflessioni
e smorza gli acuti nelle ripetizioni, per ridurre la coda rada e ronzante
segnalata anche con il solo effetto. Cinque suite CTest passano; il timbro
metallico segnalato dall'utente resta da verificare all'ascolto e i buffer
piccoli non sono certificati senza dropout.
Dettagli in [VERIFICHE-AUDIO-REVIEW.md](Changelogs/VERIFICHE-AUDIO-REVIEW.md).
Ultima revisione del riverbero e confronto su Bonk in [VERIFICHE-RIVERBERO.md](Changelogs/VERIFICHE-RIVERBERO.md).
Per preparare i sorgenti per GitHub, vedere [GITHUB.md](GITHUB.md).

Per l'architettura, i flussi di esecuzione e i criteri per estendere i sorgenti, consulta la [documentazione del codice](DOCUMENTAZIONE-CODICE.md).

Soundboard nativa C++/JUCE per Windows x64. Chiudi eventuali versioni precedenti e avvia **DualDeckJuce-0.7.exe** in questa cartella. Il progetto usa JUCE **9.0.4**.

## Effetti audio aggiunti

Nell'editor **Audio ed effetti** di ogni pad sono disponibili riverbero (durata e mix), compressore (soglia e rapporto), tremolo (velocità e profondità), distorsione (drive), ping-pong (tempo, feedback e mix) e chorus (velocità, profondità e mix). I filtri passa basso e passa alto e l'eco/delay erano già presenti. Scorri l'editor per raggiungere tutti gli effetti e il pulsante **Salva impostazioni**. Le modifiche si applicano alle nuove riproduzioni; gli effetti aggiunti sono spenti nei profili esistenti.

Il salvataggio usa lo **schema 10** e legge anche gli schemi 3–9. Una versione precedente dell'app non può leggere una libreria salvata con lo schema 10; il salvataggio conserva il backup `.bak`.

Per spostare e ridimensionare i controlli direttamente nella GUI, usa la [build Debug con editor visivo](EDITOR-UI.md).

## Novità 0.7: card Audio, Filtro e Delay

- **Audio ed effetti** apre l'editor del pad. La card **Audio e tagli** mostra la forma d'onda e i marcatori per inizio e fine; il file originale non viene modificato.
- La card **Filtri** offre un **passa basso** per tagliare gli acuti e un **passa alto** per tagliare i bassi. Entrambi hanno **Frequenza** (100–18.000 Hz) e **Mix** (0–100%). A mix 0 il filtro è spento; spostare la frequenza lo attiva automaticamente al 100%. Per ascoltare la modifica, salva e riavvia il suono.
- La card **Delay** separa **Tempo** (50–1.000 ms), **Feedback** (0–80%) e **Mix** (0–100% delle ripetizioni). Il segnale originale mantiene il suo volume; le eco sono attenuate di altri 6 dB, così anche con Mix 100% e Feedback 0 la prima eco ha metà ampiezza rispetto all'originale. A Mix 0 il delay è spento; Feedback 0 produce una sola eco.
- Le impostazioni vengono salvate per pad e si applicano alle nuove riproduzioni. Lo schema 9 ha introdotto le card filtro e delay; i vecchi valori di filtro ed eco vengono convertiti nei loro controlli, mentre il passa alto parte spento. La precedente scelta globale dei volumi viene trasferita a ciascun pad.

## Novità 0.6: editor, effetti, microfono e librerie più grandi

- La forma d'onda viene calcolata in background solo quando apri l'editor.
- In **Audio e uscite** puoi scegliere un microfono fisico, unirlo ai suoni nell'uscita principale e decidere se ascoltarlo anche nell'uscita cuffie. Il microfono parte disattivato; il livello è indipendente dai master dei suoni. Se selezioni CABLE Input come uscita principale, scegli CABLE Output come ingresso in Discord. Verifica che la voce non arrivi anche da un secondo percorso esterno per evitare duplicazioni; evita il monitor su diffusori per non creare ritorno.
- Il salvataggio conserva il backup `.bak`. I salvataggi ordinari sono eseguiti in background con una copia coerente della libreria; modifiche ravvicinate aggiornano una sola richiesta pendente. Rimozione e rinomina dei profili aggiornano subito la vista e salvano in background. La chiusura attende il salvataggio finale; se fallisce, mostra l'errore e permette di riprovare senza chiudere l'app.
- La scansione arriva a 100.000 audio o 1.000.000 file visitati, segnalando scansioni parziali. Sono stati ridotti i lavori ripetuti sulla libreria per selezione dei pad, pressione dei tasti e trascinamento dei cursori. La ricerca è differita di 150 ms nei profili sopra 5.000 pad. La vista a 960 × 640 adatta il numero di colonne e scorre verticalmente.
- Il footer mostra il tempo software dall'azione sul pad all'elaborazione nel callback audio. Non misura l'uscita fisica o la latenza fino a Discord. `Test.ps1 -Performance` misura decodifica a freddo, ricerca, paginazione e persistenza su librerie sintetiche grandi.
- `DualDeckKeyboardIsolationProbe.exe` misura separatamente gli eventi Raw Input e quelli di un hook Windows senza bloccare tasti. Il blocco della sola tastiera dedicata nelle altre app richiede ancora un componente per dispositivo: vedi [TASTIERA-BLOCCO.md](TASTIERA-BLOCCO.md).

## Correzioni nella copia DualDeck-2

- Lo **scrub con time stretching** usa la sintesi spettrale anche durante il trascinamento: la velocità del mouse cambia il tempo attraversato mantenendo l'intonazione, in avanti e all'indietro. A cursore fermo continua il freeze locale. Gli attacchi rilevati conservano una breve lettura originale e il rumore mantiene un contributo a grani. L'elaborazione è automatica quando tieni premuta la forma d'onda.
- La barra del titolo nativa usa lo stesso colore dello sfondo dell'app su Windows 11. Dove il colore personalizzato non è supportato, viene richiesta la barra scura di Windows.
- Nell'editor **Audio ed effetti**, **Riproduci file** avvia il suono con gli effetti attualmente regolati nel pannello, anche prima del salvataggio; **Ferma** arresta solo l'anteprima dell'editor. Tieni premuta la forma d'onda per ascoltare continuamente il punto scelto; trascina in entrambe le direzioni per spostare l'ascolto e rilascia per continuare da quel punto se il file era in riproduzione. Anche lo scrub usa gli effetti correnti. I pulsanti **+**, **-** e **Tutto** ingrandiscono, riducono e ripristinano la vista; la rotella ingrandisce intorno al puntatore e Maiusc+rotella scorre la vista ingrandita. Trascina i marcatori gialli per impostare **A** e **B**; **Ascolta loop A-B** ripete il tratto nell'uscita audio configurata. **Ripeti A-B premendo il pad** salva quel pad in modalità Loop on/off con i marcatori scelti. Chiudere l'editor o premere STOP TUTTO ferma l'anteprima. L'ascolto richiede che il file sia stato caricato e che l'uscita sia configurata.

- In modalità **Spam**, le copie dello stesso pad restano tutte in riproduzione, ma il mixer ne bilancia gradualmente l'energia prima di sommarle agli altri pad. Questo riduce i salti di volume con suoni ripetuti, soprattutto bassi e campioni forti. Una cancellazione di fase perfetta non può essere recuperata dal solo bilanciamento.
- La chiusura di un dispositivo audio rilascia anche i clip ancora nella coda dei comandi, così la cache può espellerli senza attendere un nuovo callback.
- Le pressioni ripetute durante il caricamento di un pad restano accorpate fino al completamento; un caricamento precedente a STOP TUTTO non altera lo stato di una richiesta successiva.
- Se l'uscita salvata non e piu disponibile, l'app la segnala e attende una nuova selezione in **Audio e uscite**. Non passa automaticamente all'uscita predefinita di Windows. Una configurazione delle cuffie fallita lascia entrambe le uscite chiuse, evitando uno stato parziale.
- I pad premuti hanno un worker di decodifica separato dal precaricamento della pagina. La preparazione del profilo viene aggiornata quando cambiano pagina, associazioni o revisione dei file, senza ripetersi a ogni modifica di uno slider.
- La finestra puo essere ridotta fino a 960 × 640; le barre di scorrimento permettono di raggiungere i controlli quando lo schermo e piu piccolo. I pad visibili possono ricevere il focus con Tab ed essere attivati tenendo premuto Invio; espongono nome, stato e azioni ai lettori di schermo. Nella vista Tastiera, usa le frecce per scegliere un tasto, Invio per usarlo e Maiusc+F10 per associarlo al pad selezionato.

## Novità 0.5: cache LRU e precaricamento

La cache PCM da 384 MiB elimina automaticamente i clip meno usati quando serve spazio. Non elimina i clip presenti in una coda o in una riproduzione; se la memoria è tutta occupata da suoni attivi, segnala di fermarli e riprovare. I suoni associati ai tasti, i pad visibili e gli altri file del profilo vengono preparati in background, nei limiti della cache. La prima pressione su un clip ancora in caricamento viene accodata una sola volta.

## Novità 0.4: controllo audio e profili

- Nella vista **Pad**, i suoni sono divisi per cartella: ogni pagina contiene fino a 24 suoni della stessa cartella. Il selettore permette di visualizzare una cartella oppure **Tutte le cartelle**, sempre in pagine separate. Le sottocartelle sono gruppi distinti; il percorso completo distingue cartelle con lo stesso nome. La numerazione segue l'ordinamento scelto sull'intero profilo; ricerca, preferiti e selezione della cartella mantengono quei numeri, quindi nei risultati filtrati possono esserci salti.
- **Master / Microfono** e **Master / Cuffie** sono sempre disponibili nella barra laterale. Il primo regola i suoni inviati all'uscita principale (ad esempio il cavo del microfono virtuale); il secondo regola l'ascolto aggiuntivo in cuffia. Non regolano il volume della voce del microfono fisico.
- Nella card del suono, **Usa il volume del suono** decide se quel pad sostituisce i master. Se la scelta è disattivata, il suono usa il master della propria uscita. Se è attivata, il valore del pad sostituisce entrambi i master senza moltiplicarli: un pad all'80% suona all'80% su entrambe le uscite anche se un master è a 0%. La scelta è indipendente per ogni suono.
- I master restano sempre regolabili. Quando un pad usa i master, il suo cursore di volume è disabilitato ma conserva il valore salvato. La scelta del pad si applica alle nuove riproduzioni.
- Nella barra laterale della **Soundboard**, **Attiva pitch globale** abilita il cursore da -12 a +12 semitoni sulle due uscite. Doppio clic sul cursore ripristina 0. Il pitch funziona tramite ricampionamento: cambia anche velocità e durata e si combina con la velocità impostata sul pad. Disattivandolo si torna al pitch originale, conservando il valore per la prossima attivazione. Master e pitch aggiornano anche i suoni già avviati, con una breve transizione per evitare salti bruschi.
- **Rinomina**, accanto al profilo, permette di scegliere un nome di 1–80 caratteri. Nomi vuoti e duplicati vengono rifiutati; restano invariati identificativo, cartelle, suoni e associazioni.
- Il **clic destro su un pad** apre un menu con modifica del nome e impostazioni, aggiunta/rimozione dai preferiti, associazione/liberazione del tasto e cambio/ricollegamento del file audio. Non avvia il suono.
- Rimossa l'importazione delle preferenze di DualDeck 2, incluso il recupero automatico al primo avvio. La libreria JUCE già presente continua a essere letta.

## Novità 0.3: profili, cartelle e ordinamento

- **Rimuovi**, accanto al selettore profili, elimina il profilo dopo conferma. I file audio restano sul disco. Se elimini l'ultimo profilo, viene creato un profilo vuoto.
- Ogni suono mostra il proprio **indice #1, #2…**, anche nell'ispettore e nella tastiera grafica. Il numero corrisponde alla posizione nell'ordinamento selezionato: per **Nome**, ad esempio, il primo suono in ordine alfabetico naturale è `#1`; passando a **Decrescente**, `#1` diventa il primo del nuovo ordine. I numeri proseguono tra le pagine. Filtri e preferiti conservano il numero assegnato nell'ordine completo del profilo. Puoi cercare il numero mostrato scrivendo ad esempio `#25`. Le associazioni ai tasti seguono il rispettivo suono.
- **Collega cartella** sostituisce l'aggiunta con copia dei singoli file. Scegli la cartella che contiene gli audio, anche quella usata con Soundux o SoundPad: vengono lette anche le sottocartelle. Puoi trascinare una o più cartelle nella finestra.
- Le cartelle appartengono al profilo selezionato. Il pannello laterale ne mostra il numero e la prima cartella; passando il mouse sul riepilogo vedi tutti i percorsi.
- **Aggiorna cartelle** rileva nuovi file, file eliminati e variazioni di dimensione/data. La scansione viene eseguita anche all'avvio del profilo e quando lo selezioni. Si svolge in sottofondo e può essere annullata.
- Lo stesso percorso non crea un secondo pad nel profilo, anche collegando cartelle sovrapposte. File distinti con lo stesso nome rimangono distinti. Non viene fatta deduplicazione del contenuto di file diversi.
- Se un file manca da una cartella collegata, il pad rimane come **File non trovato** e conserva nome, tasto, preferiti, volume, tagli ed effetti. Quando rimetti il file nello stesso percorso e premi **Aggiorna cartelle** (o riapri il profilo), il pad torna disponibile con le sue impostazioni, anche dopo un riavvio dell'app. Se il percorso è cambiato, usa **Ricollega**. La scansione non cancella automaticamente i pad mancanti.
- Ordinamento per **indice, nome, data di modifica, dimensione e tipo di file**, crescente o decrescente. Il nome usa l'ordine naturale (suono 2 prima di suono 10). La preferenza viene salvata per profilo.

Le cartelle collegate sono riferimenti ai normali file audio: non vengono importati database, playlist o impostazioni interne delle altre applicazioni. I file già copiati dalle versioni precedenti restano disponibili. Collegare una cartella esterna non sposta né cancella questi file.

La versione 0.4 ha introdotto lo **schema 5**; la 0.6 lo schema 6. Questa copia salva con schema 10 e continua a leggere gli schemi 3–9.

La scansione non segue collegamenti simbolici e si ferma a 100.000 audio o 1.000.000 file visitati, segnalando una scansione parziale. Se un file viene modificato esternamente, premi **Aggiorna cartelle**: la riproduzione successiva usa una nuova versione in cache; eventuali voci già in corso possono finire con il contenuto precedente.

## Novità 0.2

- Scheda **Tastiera dedicata**, riconoscimento con F12 e comandi in secondo piano dalla sola tastiera selezionata.
- Filtro delle ripetizioni automatiche; rilascio dei loop e stop al cambio dei dispositivi. Attivazione manuale dopo ogni avvio o cambio dei dispositivi.
- Pulsante **Ricollega** sui suoni mancanti e **Cambia file** sugli altri: conserva nome, tasto e impostazioni del pad.
- Percorsi relativi per i file dentro `sounds`, applicati al prossimo salvataggio. Se un vecchio percorso assoluto non esiste, il caricamento cerca lo stesso nome di file nella cartella `sounds` attuale. Non vengono associati file diversi in base al nome del pad.
- Compilazione che rigenera la configurazione se si sposta la cartella del progetto; test separati dall'app eventualmente aperta.

## Tastiera dedicata

La versione approvata **`DualDeckJuce-0.7.exe`** (anche `DualDeckJuce.exe`) include il blocco della tastiera dedicata. Mantieni la cartella `keyboard-runtime` accanto agli eseguibili. Usa Interception già installato su questo PC; non installa o modifica driver. Il funzionamento è stato confermato dall'utente dopo la correzione dell'avvio.

Apri la versione approvata. Nella scheda **Tastiera dedicata**, premi **Riconosci con F12**, quindi premi e rilascia F12 sul dispositivo da dedicare ai suoni. Rilascia tutti i tasti e attiva l'interruttore. Attendi lo stato attivo prima di provare i pad con un'altra applicazione in primo piano.

Per verificare una nuova selezione, controlla che i tasti dedicati avviino i pad senza scrivere nel Blocco note, mentre la tastiera principale continua a scrivere normalmente. Disattiva l'interruttore e verifica che anche quella dedicata torni a scrivere.

Identificazione non univoca, libreria assente, cambio dispositivi o errore interrompono la cattura e richiedono una nuova attivazione manuale. Un processo separato rilascia il filtro se l'interfaccia non risponde per circa 1,5 secondi. Se l'interfaccia resta bloccata, i suoni già in riproduzione vengono fermati quando essa riprende; il rilascio della tastiera non dipende da questa ripresa.

Nei campi di testo di DualDeck i comandi sono sospesi, salvo durante l'associazione. F12 è riservato al riconoscimento per le nuove associazioni. Per tornare ai comandi locali, disattiva l'interruttore.

Dettagli, verifiche e limiti: [TASTIERA-BLOCCO.md](TASTIERA-BLOCCO.md).

## Funzioni di questa versione

- Vista con 24 pad per pagina, ricerca nell'intero profilo e filtro preferiti.
- Vista della tastiera compatta, con suoni assegnati e associazione tramite clic destro.
- Profili multipli con creazione, rinomina e rimozione.
- Riproduzione Normale, Spam, Loop on/off e Loop premuto; modalità generale ed eccezioni per suono.
- 32 voci contemporanee per uscita; oltre il limite viene sostituita la voce più vecchia.
- Volume, velocità 0,5–2×, tagli e dissolvenze per suono. I file originali non vengono alterati. La velocità cambia anche l'intonazione: il pitch indipendente non è ancora implementato.
- Stop generale e pausa DJ mentre si tiene premuto il pulsante o il tasto DJ (Spazio come valore iniziale).
- Uscita principale e cuffie con volumi indipendenti. Lo stesso dispositivo viene aperto una sola volta.
- Collegamento delle cartelle audio tramite pulsante o trascinamento nella finestra, senza copie.
- Lettura WAV, MP3, FLAC, OGG, AIFF; M4A/AAC e altri formati Windows tramite Media Foundation, secondo i codec presenti.

## Primo avvio e dati

Se `data/library.json` non esiste, l'app crea un profilo vuoto. Collega le cartelle dei tuoi suoni con **Collega cartella**. I file esterni rimangono nella loro posizione; quelli interni al progetto sono salvati con percorsi relativi. Per spostare la libreria locale conserva insieme eseguibile, `data` e le cartelle audio interne.

Le impostazioni sono in `juce-version/data/library.json`, con backup `.bak`. Seleziona le uscite nella scheda **Audio e uscite** e premi **Applica uscite**. Se il file della libreria è corrotto, l'app segnala l'errore e sospende i salvataggi, conservando il file per il recupero.

## Comandi

- Clic sinistro su un pad: riproduci. Per Loop premuto, tieni premuto e rilascia.
- Clic destro su un pad: apre il menu delle azioni senza riprodurre.
- Per associare: seleziona un pad, premi **Associa un tasto**, poi premi il tasto desiderato. In alternativa, clic destro su un tasto della vista Tastiera.
- Esc ferma tutti i suoni. Il tasto DJ (Spazio come valore iniziale) congela e riprende la riproduzione mentre viene tenuto premuto.
- Le modifiche individuali al suono valgono dalle riproduzioni successive; master, precedenza dei volumi e pitch globale si applicano anche durante la riproduzione. Fine taglio = 0 significa fine del file.
- A tastiera dedicata disattivata, le scorciatoie funzionano solo con l'app in primo piano. Attivandola, vengono ricevuti anche in secondo piano i tasti del solo dispositivo selezionato.

## Audio e limiti dell'anteprima

Per inviare i suoni a Discord puoi scegliere CABLE Input come uscita principale, le cuffie come monitor, e CABLE Output come ingresso in Discord. Puoi attivare il mix del microfono fisico nella stessa uscita. Se la voce passa da DualDeck, elimina eventuali altri percorsi della stessa voce verso Discord.

Le due uscite usano callback e clock separati. Il motore riproduce gli stessi comandi su entrambe, ma non offre sincronizzazione precisa tra schede audio diverse. Le latenze mostrate sono quelle riferite dal driver, non misure del ritardo fino a Discord.

Decodifica in sottofondo, caricamento dei soli primi 3 minuti di ogni file (gli audio più lunghi vengono tagliati automaticamente senza modificare il file originale) e cache PCM LRU da 384 MiB. La cache espelle il clip meno usato che non è trattenuto da una voce o da un comando audio in coda; i riferimenti vengono rilasciati dal callback audio, senza liberare memoria nel thread audio. Il profilo viene preparato in background anche nella vista Tastiera, dando precedenza alle associazioni e ai pad visibili; gli altri file non espellono i clip già pronti. La prima pressione su un file non ancora caricato attende la decodifica; ulteriori pressioni durante quel caricamento vengono accorpate. Se tutti i clip in memoria sono ancora in uso, l'app chiede di fermare i suoni e riprovare. Lo stop rende espellibili le voci al successivo callback audio; un loop rilasciato prima del caricamento non parte in ritardo.

Il mixer usa una coda di comandi e un numero fisso di voci; il callback non alloca memoria e non prende mutex. In Spam, il livello delle copie dello stesso pad viene bilanciato con metà compensazione in decibel, lasciando un aumento moderato del volume: due copie coerenti risultano circa 1,41 volte una copia, tre circa 1,73 volte, prima del limiter. La transizione resta breve e una copia superstite recupera il proprio livello; pad diversi non vengono normalizzati insieme. Il limiter finale protegge i picchi del mix complessivo. Le copie in opposizione di fase possono ancora cancellarsi completamente.

Il blocco della tastiera secondaria è incluso nella versione approvata e richiede Interception già installato. Non sono ancora presenti: pitch indipendente, registrazione su file, timeline, piano, azioni OBS, esportazione/importazione ZIP, sincronizzazione cloud e menu nell'area di notifica.

## Compilazione e verifiche

L'esito verificato della versione 0.7 è in [VERIFICHE-0.7.md](Changelogs/VERIFICHE-0.7.md). I rapporti [VERIFICHE-0.6.md](Changelogs/VERIFICHE-0.6.md) e [VERIFICHE-0.5.md](Changelogs/VERIFICHE-0.5.md) restano come cronologia.

Sul PC di sviluppo restano utilizzabili gli strumenti portatili già in `tools`.
Per una nuova checkout usare Visual Studio/Build Tools 2022 con C++ x64 e Windows
SDK; vedere BUILDING.md. Setup non installa compilatori o driver.

```powershell
.\Build.ps1
.\Test.ps1 -AudioDevices -Performance
```

Per ricompilare questa copia usando gli strumenti portatili gia presenti nella cartella DualDeck adiacente, passa il percorso di quella cartella a `-ToolRoot` in entrambi gli script. I binari e i report vengono sempre scritti in questa copia.

Su una nuova copia eseguire Setup.ps1: scarica JUCE, CMake e Ninja nelle cartelle
del progetto, con SHA-256 fissati. Compilatore Microsoft e Windows SDK vanno
predisposti separatamente attraverso gli strumenti ufficiali Microsoft.

`Test.ps1` verifica il DSP, il filtro dei tasti, gli effetti, il mix microfono, la libreria e le interazioni sintetiche dei pad. Decodifica un file esistente per formato presente nella libreria corrente, conta separatamente i riferimenti mancanti e salva le anteprime dell'interfaccia. Controlla la registrazione Raw Input senza attivare i comandi. Con `-AudioDevices` apre le uscite reali con il microfono silenziato nel test. Con `-Performance` misura anche una libreria sintetica fino a 50.000 pad. Non modifica la libreria dell'utente. L'ascolto della voce, la verifica fisica delle due tastiere e i lettori di schermo richiedono anche una prova manuale.

## Struttura e prossime tappe

- `Source/AudioCore.h`: voci, comandi, loop, tagli, pausa, protezione dei picchi.
- `Source/AudioEngine.*`: uscite JUCE/WASAPI e cache dei file.
- `Source/WindowsDecoder.cpp`: decoder Media Foundation per i formati Windows.
- `Source/Library.*`, `Source/LibraryCatalog.cpp`: libreria, cartelle di riferimento, ordinamenti e persistenza.
- `Source/MainComponent.*`: interfaccia, modifica dei pad e comandi.
- `Source/PadEditor.*`: visualizzazione della forma d'onda e controlli di taglio ed effetti.
- `Source/RawKeyboard.*`, `Source/KeyboardFilter.h`: dispositivi Windows e filtro della tastiera dedicata.
- `Tests/AudioTests.cpp`: verifiche del comportamento audio e della coda concorrente.
- `Tests/KeyboardTests.cpp`: selezione, pressione, rilascio, ripetizioni e disattivazione.
- `Tests/LibraryFeatureTests.cpp`: cartelle, duplicati, file mancanti, ordinamenti, cancellazione scansioni e rimozione profili.
- `Tests/KeyboardIsolationProbe.cpp`: confronto sperimentale fra Raw Input e hook, senza blocco dei tasti.

Le prossime tappe sono le ulteriori prove di recupero della tastiera dopo crash e riavvio, la verifica udibile del microfono e il test fisico delle interazioni accessibili. Timeline e piano vengono dopo il consolidamento di questa base.

## Licenze e provenienza

La preparazione corrente usa AGPL-3.0-only per il codice originale: vedere
LICENSE e COPYRIGHT.md per l'ambito e le precedenti citazioni MIT.
JUCE e le sue dipendenze mantengono condizioni proprie, documentate in
JUCE-LICENSE.md e THIRD-PARTY-NOTICES.md.

Il precedente bootstrap Microsoft derivato dal gist di mmozeiko è conservato
solo nei backup locali e rimosso dalla preparazione distribuibile, perché non
è stata trovata una licenza esplicita. SoundLord resta un riferimento funzionale;
questa app non ne incorpora codice, immagini o marchi.

