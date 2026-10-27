# Verifiche — DualDeck JUCE 0.5

Eseguite il 21 ottobre 2026 alle 23:05 su Windows x64. Build Release con JUCE 9.0.4, MSVC e CMake portatili del progetto. Comandi: Build.ps1 e Test.ps1 -AudioDevices.

| Verifica | Esito |
|---|---|
| Build Release 0.5 | Superata; DualDeckJuce-0.5.exe ricreato |
| CTest audio_behaviour | Superata: 364 controlli |
| CTest keyboard_filter | Superata: 16 controlli |
| Libreria corrente | 331 pad, 2 cartelle, 0 riferimenti a file mancanti |
| Catalogo | Caricamento, round trip, JSON non valido, cartelle, deduplicazione, ordinamento e rimozione profili: superati |
| Decoder | WAV, MP3 e M4A: superati |
| Cache versioni file | Revisione aggiornata separata dalla precedente: superata |
| Protezione dei clip | Clip trattenuto da una voce non espulso; diventa espellibile dopo lo stop: superato |
| Ordine LRU | Espulso il clip meno recente quando serve spazio: superato |
| Interfaccia | Quattro schermate renderizzate: pad, audio, tastiera e tastiera dedicata |
| Uscite reali | Realtek, HyperX, FxSound e CABLE Input aperte riproducendo solo silenzio |
| Uscite simultanee | HyperX insieme a Realtek, FxSound e CABLE Input: superate |
| Dispositivo duplicato | Una sola uscita attiva: superato |

La cache è limitata a 384 MiB e il precaricamento decodifica in background i primi sei pad visibili. I clip in coda, prenotati per la riproduzione o usati da voci attive restano protetti dall'espulsione. Il callback audio rilascia i riferimenti senza allocare o prendere mutex.

Le prove sulle uscite riproducono solo silenzio. I 10 ms riportati dai driver non misurano la latenza completa da pressione del tasto fino a Discord. Non è stata eseguita una prova in chiamata né una registrazione della voce. Il precaricamento è integrato nella vista Pad; il test automatico dell'interfaccia usa schermate isolate e non misura i tempi percepiti all'avvio.

## Prova manuale ancora necessaria

La cattura Raw Input è stata verificata nella registrazione dei dispositivi, ma non è stata provata con pressioni fisiche. La modalità non blocca i tasti nelle altre applicazioni.

1. Avvia DualDeckJuce-0.5.exe e apri Tastiera dedicata.
2. Premi Riconosci con F12 e premi/rilascia F12 sulla tastiera secondaria; poi rilascia tutti i tasti.
3. Attiva la modalità, associa un pad e prova pressione e rilascio con DualDeck in primo piano e in secondo piano.
4. Controlla che la tastiera principale non attivi lo stesso pad; prova un loop tenuto premuto e il rilascio.
5. Scollega la tastiera selezionata: i suoni devono fermarsi e la modalità disattivarsi. Ricollegala e ripeti il riconoscimento.
6. Disattiva la modalità e verifica che i comandi locali funzionino di nuovo.

Report automatici e anteprime: test-output. La libreria utente e i file audio sono esclusi dal controllo versione.

## Correzioni nella copia DualDeck-2

- Build Release ricreata e CTest superato: 364 controlli audio e 16 controlli della tastiera.
- `Test.ps1 -AudioDevices` superato con 331 pad, 0 riferimenti mancanti e decodifica WAV, MP3 e M4A. Il test audio aggiunto conferma che, se l'uscita cuffie fallisce, entrambe le uscite restano chiuse.
- Avvio con una libreria di prova che indica un'uscita inesistente: la schermata `../device-check/missing-output.png` mostra il dispositivo mancante e il messaggio di scelta obbligatoria. Lo SHA-256 della libreria di prova è rimasto invariato prima e dopo l'avvio.
- La schermata `test-output/board-small.png` verifica la finestra a 960 × 640 con barre di scorrimento. Le altre quattro schermate restano renderizzate correttamente.

Restano necessarie prove manuali con pressione fisica della tastiera dedicata, ascolto udibile e chiamata Discord. La separazione dei worker riduce l'attesa dovuta ai precaricamenti accodati, ma il ritardo percepito alla prima pressione non è stato misurato.
Va inoltre controllato con una tastiera fisica che Tab/Invio su un pad e frecce/Invio/Maiusc+F10 nella vista Tastiera funzionino anche con i loop premuti e con un tasto Invio già assegnato a un pad.

## Aggiornamento del 22 ottobre 2026

L'utente ha confermato che l'audio è udibile sia in cuffia sia su Discord e che la separazione dell'input tra le due tastiere funziona con tasti fisici. Resta da verificare il blocco della tastiera associata alla soundboard nelle altre applicazioni; la versione corrente non lo implementa.

La build Release è stata ricreata dopo due correzioni: rilascio dei riferimenti ai clip quando l'uscita audio si chiude senza un ulteriore callback, e accorpamento delle pressioni mentre un pad viene decodificato, anche dopo STOP TUTTO. CTest supera 366 controlli audio e 16 della tastiera. `Test.ps1 -AudioDevices` supera inoltre le prove di libreria, decoder WAV/MP3/M4A, registrazione Raw Input, rendering delle schermate e apertura delle uscite reali con solo silenzio. Il nuovo caso audio verifica una voce attiva e un comando accodato alla chiusura dell'uscita.
