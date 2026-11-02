# Verifiche — DualDeck JUCE 0.2

21 ottobre 2026, Windows x64. JUCE 9.0.4, MSVC 19.44, CMake 4.4.4, build Release.

| Verifica | Esito |
|---|---|
| Compilazione nella cartella rinominata `DualDeck` | Superata; cache CMake rigenerata |
| Motore audio | 88 controlli superati, inclusa la coda concorrente con 100.000 eventi |
| Filtro tastiera | 16 controlli superati: dispositivo scelto, ripetizioni, rilascio, disattivazione, cambio tastiera, modificatori estesi |
| Windows Raw Input | Registrazione e selezione riuscite per 4 dispositivi logici; acquisizione lasciata disattivata |
| Libreria | Importazione, round trip, JSON non valido, identificativo tastiera, percorsi relativi, spostamento cartella e recupero per nome esatto: superati |
| Dati correnti | 645 pad caricati, 326 file disponibili e 319 riferimenti mancanti preesistenti |
| Decoder | Un file reale per ciascuno dei formati MP3, WAV e M4A decodificato |
| Uscite reali | Realtek, HyperX, FxSound e VB-CABLE aperte con solo silenzio |
| Due uscite | Realtek insieme a ciascuna delle altre tre uscite: superato |
| Duplicazione uscita | Stesso dispositivo scelto due volte: una sola uscita attiva |
| Interfaccia | Pad, tastiera grafica, audio e tastiera dedicata renderizzati e ispezionati |

La libreria dell'utente `data/library.json` ha conservato lo SHA-256:
`B5C0626768D73C555FD3A8A2C3E38B1FC97573F679F01CC086ED680B709AE4CC`.
I test scrivono i fixture in `test-output/fixtures`, senza salvare la libreria dell'utente.

Il profilo attivo **Profilo 2** contiene i 326 file disponibili. Il precedente **Principale** contiene 319 riferimenti a file assenti nella vecchia cartella `DualDeck-2`. Non sono stati rimossi né riassegnati a file diversi. È disponibile **Ricollega** per scegliere un file conservando le impostazioni del pad.

Gli ID Windows rappresentano dispositivi logici: quattro voci non significano necessariamente quattro tastiere fisiche.

## Prova manuale ancora da eseguire

1. Avvia `DualDeckJuce-0.2.exe`, apri **Tastiera dedicata**, premi **Riconosci con F12** e premi/rilascia F12 sulla tastiera secondaria.
2. Rilascia i tasti e attiva la modalità. Associa un pad a un tasto di quella tastiera.
3. Verifica pressione/rilascio, assenza di ripetizioni automatiche e assenza di attivazioni dello stesso pad dalla tastiera principale. Ripeti con DualDeck in secondo piano.
4. Prova un loop premuto e la pausa DJ. Scollegando la tastiera, i suoni devono fermarsi e la modalità disattivarsi; dopo il collegamento è necessaria una nuova attivazione.

Queste prove fisiche non sono state sostituite da eventi sintetici. Non sono stati eseguiti ascolti in chiamata, misure della latenza completa o registrazioni della voce. La modalità Raw Input non blocca i tasti nelle altre applicazioni.

Report: `test-output/library-tests.txt`, `test-output/keyboard-devices.txt`, `test-output/audio-devices.txt`, `build-msvc/Testing/Temporary/LastTest.log`.

Anteprime: `test-output/board.png`, `test-output/keyboard.png`, `test-output/audio.png`, `test-output/dedicated-keyboard.png`.

Il rapporto `VERIFICHE.md` documenta la precedente versione 0.1 e lo stato della libreria prima del cambio di cartella.
