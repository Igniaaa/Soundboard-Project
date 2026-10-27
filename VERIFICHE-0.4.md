# Verifiche — DualDeck JUCE 0.4

Eseguite il 21 ottobre 2026 su Windows x64. Build Release con JUCE 9.0.4, MSVC e CMake portatili del progetto. Eseguite Build.ps1 e Test.ps1 -AudioDevices.

| Verifica | Esito |
|---|---|
| Build Release | Superata; DualDeckJuce-0.4.exe ricreato |
| CTest audio_behaviour | Superata: 104 controlli |
| CTest keyboard_filter | Superata: 16 controlli |
| Libreria corrente | 331 pad, 2 cartelle, 0 riferimenti a file mancanti |
| Catalogo | Caricamento, round trip, JSON non valido, cartelle, deduplicazione, ordinamento e rimozione profili: superati |
| Cache | Aggiornamento revisione file e durata delle voci già avviate: superati |
| Decoder | WAV, MP3 e M4A: superati |
| Interfaccia | Quattro schermate renderizzate: pad, audio, tastiera e tastiera dedicata |
| Uscite reali | Realtek, HyperX, FxSound e CABLE Input aperte riproducendo solo silenzio |
| Uscite simultanee | HyperX insieme a Realtek, FxSound e CABLE Input: superate |
| Dispositivo duplicato | Una sola uscita attiva: superato |

La verifica audio apre i dispositivi con solo silenzio. I 10 ms riportati dai driver non misurano la latenza completa da pressione del tasto fino a Discord. Non è stata eseguita una prova in chiamata né una registrazione della voce.

## Prova manuale ancora necessaria

La cattura Raw Input è stata verificata nella registrazione dei dispositivi, ma non è stata provata con pressioni fisiche. La modalità non blocca i tasti nelle altre applicazioni.

1. Avvia DualDeckJuce-0.4.exe e apri Tastiera dedicata.
2. Premi Riconosci con F12 e premi/rilascia F12 sulla tastiera secondaria; poi rilascia tutti i tasti.
3. Attiva la modalità, associa un pad e prova pressione e rilascio con DualDeck in primo piano e in secondo piano.
4. Controlla che la tastiera principale non attivi lo stesso pad; prova un loop tenuto premuto e il rilascio.
5. Scollega la tastiera selezionata: i suoni devono fermarsi e la modalità disattivarsi. Ricollegala e ripeti il riconoscimento.
6. Disattiva la modalità e verifica che i comandi locali funzionino di nuovo.

I test automatici e i report ripetibili sono in Test.ps1, test-output e build-msvc/Testing/Temporary/LastTest.log. Le cartelle data, sounds, build, output di test, backup e gli eseguibili sono escluse dal controllo versione.
