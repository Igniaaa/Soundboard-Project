# Verifiche scrub e velocità zero — 27 ottobre 2026

## Nuovo riscontro e revisione audio

Il successivo riscontro dell'utente segnala ancora un timbro metallico
durante scrub e freeze. La [revisione audio](VERIFICHE-AUDIO-REVIEW.md)
separa l'evoluzione dei bin rumorosi e riduce il costo della ricerca PCM.
I test automatici passano; naturalezza all'ascolto e buffer piccoli restano
aperti. I precedenti riscontri positivi riportati sotto sono storici.

## Time stretching durante il trascinamento — 27 ottobre 2026

Il phase vocoder stereo esistente ora contribuisce durante il movimento, oltre che nel freeze. Prima il mix passava interamente ai grani PCM durante gli aggiornamenti del cursore e per 35 ms dopo l'ultimo movimento. La nuova versione mantiene il mix adattivo fra sintesi spettrale e grani anche in questa fase; la precedenza al PCM resta per i 50 ms degli attacchi rilevati.

Il cursore determina direttamente la posizione di analisi a ogni hop, con avanzamento positivo, negativo o nullo. L'hop di sintesi e l'avanzamento degli oscillatori restano indipendenti dalla velocità del cursore. Rimosso l'inseguimento della posizione con dimezzamento della distanza e limite di 30 ms per hop. I salti superiori a 150 ms mantengono il raccordo esistente. Nessuna nuova dipendenza o allocazione nel callback audio.

Build Release completata con `Build.ps1`; aggiornati `DualDeckJuce-0.7.exe` e `DualDeckJuce.exe`. Tutte le suite CTest superate:

- `audio_behaviour`: 522 controlli esistenti, inclusi freeze, armoniche durante il movimento, attacchi, arresti, salti, stereo e materiale rumoroso.
- `scrub_time_stretch`: 192 nuovi controlli su una nota in glissando a velocità 0,125×, 0,5×, 1×, 2×, 4×, −0,5×, −2× e −4×, mouse a 30/60 Hz, sorgente a 48 kHz e uscite a 44,1/48/96/192 kHz. Verificati intonazione rispetto al punto sorgente, livello e stereo in opposizione di fase.
- `keyboard_filter`: 16 controlli.

Il nuovo test misura gli attraversamenti dello zero su finestre da 200 ms (risoluzione di 5 Hz), con tolleranza per un ritardo spettrale fino a 50 ms sul glissando. Lo scarto massimo misurato è 25 Hz nel movimento inverso veloce; questo non è una misura dell'errore su una nota costante o della latenza del dispositivo. Il test distingue il mantenimento dell'intonazione dalla trasposizione dovuta a un cambio di playback rate. Non dimostra un miglioramento percettivo rispetto ai grani precedenti. Resta da ascoltare il risultato su materiale reale; non è stato eseguito ascolto fisico.

Copia dei sorgenti precedenti in `backups/time-stretch-before-2026-10-27/`; test ripetibile in `Tests/ScrubTimeStretchTests.cpp`.

## Revisione del freeze dopo il riscontro di fastidio — 27 ottobre 2026

Il successivo riscontro dell'utente distingue il trascinamento, già apprezzato, dal suono a cursore fermo, percepito come un suono di errore. Questa revisione riguarda il secondo caso.

### Verifica tecnica prima della modifica

Il codice precedente analizzava e risintetizzava già tutti i bin dello spettro locale. Il problema non era una ricostruzione limitata alla fondamentale: un singolo spettro rumoroso, mantenuto con oscillatori stabili, produceva righe tonali persistenti. La testina phantom veniva esclusa dal mix una volta terminato il movimento.

Le [fonti ufficiali AudioStretch](https://help.audiostretch.com/hc/en-us/articles/360033876253-What-is-AudioStretch) documentano LiveScrub e riproduzione a velocità zero, ma non descrivono l'algoritmo interno né confermano una testina phantom. Il [phase vocoder di Dan Ellis](https://www.ee.columbia.edu/~dpwe/resources/matlab/pvoc/) chiarisce il ruolo dello spettro locale e della continuità di fase. Il [tutorial di Cycling '74 sul freeze](https://cycling74.com/articles/the-phase-vocoder-part-ii) descrive il carattere meccanico del freeze invariabile e l'uso di variazioni locali di posizione e fase. L'implementazione qui resta autonoma: non è una replica verificata di AudioStretch.

### Modifica

- Il phantom rimane nel freeze con un contributo adattivo alla trama del segnale.
- I grani vengono riposizionati entro la finestra locale, riservando spazio ai due lettori del raccordo. Il freeze non prosegue liberamente nella frase seguente.
- La componente spettrale usa diffusione di fase nelle bande rumorose; le bande tonali mantengono fasi coordinate. La variazione è condivisa fra sinistra e destra.
- In presenza di note e rumore insieme, il contributo phantom è limitato per non disperdere la nota. Nessun bin viene eliminato; la classificazione è euristica, non una separazione perfetta.
- Il trascinamento e le pause tra eventi del mouse mantengono il precedente allineamento. La nuova lettura locale entra dopo l'arresto.

### Misure ripetibili

Il confronto `test-output/Compile-FreezeTextureProbe.ps1` usa i sorgenti salvati in `backups/freeze-texture-before-2026-10-27/`. Produce `test-output/freeze-texture-comparison.txt` e WAV sorgente/prima/dopo per rumore a banda larga (0), rumore filtrato (1), rumore filtrato più nota a 233 Hz (2). I WAV non sono normalizzati separatamente: i livelli sono quelli del motore. Nessuno di questi segnali sintetici è una registrazione vocale o musicale reale.

Piattezza spettrale su FFT Hann da 16384 campioni, banda 2–6 kHz, uscita 48 kHz, a partire da 500 ms. È il rapporto fra media geometrica e aritmetica della potenza: valori vicini a zero indicano forti righe e vuoti spettrali, non una misura diretta di gradevolezza.

| Segnale | Sorgente | Freeze prima | Freeze dopo |
|---|---:|---:|---:|
| Rumore a banda larga | 0,5266 | 0,000036 | 0,4716 |
| Rumore filtrato | 0,5251 | 0,000036 | 0,4736 |
| Rumore filtrato + nota | 0,5251 | 0,000036 | 0,3894 |

Il livello RMS del caso misto passa da 0,1302 a 0,0908; l'ampiezza coerente a 233 Hz passa da 0,0853 a 0,0682 (sorgente nominale 0,1). Il metodo preserva una componente intonata e riduce la trasformazione del rumore in toni; non mantiene esattamente il livello o la forma d'onda sorgente.

La suite audio comprende ora **522 controlli**, di cui 40 nuovi su trama spettrale, livelli del freeze prolungato, stereo in opposizione di fase e nota immersa nel rumore, con uscite 44,1/48/96/192 kHz. Sono mantenuti i controlli precedenti su bordi, bassi, silenzio prima della nota, accordi, attacchi, cambi di direzione, salti e file corti. La suite tastiera comprende 16 controlli.

`test-output/Compile-FreezeRegressionProbe.ps1` confronta anche movimento, freeze tonale e costo di calcolo; il report è `test-output/freeze-regression-comparison.txt`. Il phantom continuo aumenta il costo del freeze tonale rispetto alla sola sintesi. I tempi sono misure locali su una voce e non certificano due uscite simultanee o assenza di dropout con tutti i buffer/dispositivi.

Build Release e CTest verificati. Nessuna prova di ascolto fisico o comparazione diretta con AudioStretch viene dichiarata. La verifica percettiva sul materiale che ha originato il riscontro resta da fare.

## Conferma precedente dell'utente — 27 ottobre 2026

Dopo la consegna delle correzioni, l'utente ha confermato: «Bene lo scrub ora si sente bene». La resa dello scrub è quindi confermata anche dal suo riscontro d'ascolto. Non sono stati indicati materiale audio, dispositivo o impostazioni usati; il riscontro non dimostra equivalenza percettiva con AudioStretch né certifica tutte le condizioni misurate nei test sintetici.

## Correzioni del 27 ottobre

Il riferimento funzionale resta [LiveScrub e Zero-Speed di AudioStretch](https://www.audiostretch.com/): ascoltare trascinando e sostenere il punto scelto tenendo ferma la forma d'onda. Non è stata eseguita una comparazione d'ascolto diretta con l'app; i risultati seguenti misurano il motore locale su segnali sintetici.

- **Freeze più aderente al cursore.** In precedenza ogni pressione avviava 50 ms di PCM in avanti, anche sul silenzio. Ora questa protezione è riservata agli attacchi rilevati; la dissolvenza verso il freeze dura 12 ms. Tra eventi del mouse, la testina locale viene riancorata al punto scelto quando non sta riproducendo un attacco.
- **Bordi del file.** Le finestre di analisi vengono spostate all'interno del clip, quando sufficientemente lungo. All'inizio la misura di fase usa una finestra successiva. Si evita così di riempire metà finestra con copie del primo/ultimo campione, che causavano attenuazione e una componente continua indesiderata.
- **Uscite ad alta frequenza.** FFT/hop 2048/512 a 44,1/48 kHz, 4096/1024 a 96 kHz e 8192/2048 a 192 kHz: la durata resta circa 43–46 ms, invece di scendere a 21/11 ms e degradare i bassi. Le allocazioni restano in `prepare()`.
- **Posizione precisa.** Eliminata la soglia di 24 campioni che poteva lasciare il freeze sulla vecchia posizione e la tolleranza di 2 ms al rilascio del mouse.

### Misure ripetibili

`test-output/ScrubHoldProbe.cpp` e `test-output/Compile-ScrubHoldProbe.ps1` producono le misure del freeze. I risultati prima/dopo sono in `test-output/scrub-hold-before.txt` e `test-output/scrub-hold-after.txt`. La copia dei sorgenti precedenti è in `backups/scrub-before-2026-10-27/`.

| Caso sintetico | Prima | Dopo |
|---|---:|---:|
| Cursore fermo nel silenzio, nota a +30 ms; RMS fra 20 e 100 ms | 0,08871 | 0 |
| Nota 55 Hz, inizio file, uscita 48 kHz; RMS stabile | 0,07775 | 0,14116 |
| Nota 55 Hz, fine file, uscita 48 kHz; RMS stabile | 0,07715 | 0,14214 |
| Nota 55 Hz, inizio file, uscita 96 kHz; RMS stabile | 0,07228 | 0,14116 |
| Nota 55 Hz, fine file, uscita 96 kHz; RMS stabile | 0,07615 | 0,14214 |

La sinusoide sorgente ha ampiezza 0,2, quindi RMS atteso circa 0,14142. Il livello stabile è misurato fra 250 e 500 ms. Dopo l'arresto del trascinamento su una nota a 233 Hz, con velocità −1×, −0,5×, 0,02×, 0,5× e 1× e mouse a 60 Hz, la finestra peggiore di 20 ms mantiene RMS circa 0,13937.

I test audio contengono **482 controlli**, inclusi 40 nuovi controlli su bordi, bassi e intonazione a 44,1/48/96/192 kHz, silenzio prima di una nota, arresto del trascinamento, piccoli movimenti e assenza di cancellazioni durante il passaggio al freeze. Restano i test precedenti su attacchi, scrub inverso, mouse a 30/60 Hz, accordi, stereo, file corti e PCM non valido.

`Build.ps1` ha completato la build Release e i test CTest `audio_behaviour` e `keyboard_filter`. Le copie `DualDeckJuce-0.7.exe`, `DualDeckJuce.exe` e `build-msvc/bin/DualDeckJuce.exe` hanno lo stesso SHA-256. `git diff --check` non segnala errori di whitespace. Non sono stati eseguiti test GUI o di ascolto fisico in questa verifica.

`test-output/Compile-ScrubComparison27.ps1` confronta i sorgenti prima/dopo il 27 ottobre; risultati in `test-output/scrub-comparison-27.txt`. Sul segnale a 220/913/1761 Hz, la quota di energia nelle bande originali durante lo scrub inverso con aggiornamenti a 30 Hz passa da **99,4863% a 99,9420%**. I risultati dei movimenti continui restano equivalenti. Le durate misurate dipendono dal carico del sistema: nel run finale a 48 kHz, il freeze richiede circa 24,5 ms di calcolo per secondo audio, lo scrub circa 83–170 ms. Il blocco peggiore da 128 campioni supera il suo budget sia prima (3,45 ms) sia dopo (5,45 ms, budget 2,67 ms); queste misure non certificano il funzionamento senza dropout con buffer molto piccoli. La durata FFT maggiore alle alte frequenze comporta inoltre più lavoro per hop.

La finestra spettrale considera comunque un intorno del cursore: non separa eventi più brevi della sua risoluzione temporale. I transienti rilevati conservano una breve riproduzione PCM. Il riscontro positivo dell'utente riportato sopra conferma la resa nell'uso provato; restano da documentare prove specifiche su voce, musica e percussioni e misure della latenza del dispositivo.

## Verifica precedente — 26 ottobre 2026

Lo scrub combina il motore spettrale esistente con una testina phantom a velocità locale 1×, allineamento per somiglianza dell'onda e raccordi a coseno. Il freeze usa fasi coordinate intorno ai picchi spettrali e condivise tra i canali stereo. Il passaggio al suono fermo non riavvia le fasi. Gli attacchi conservano il percorso PCM originale.

Il riferimento funzionale è il trascinamento con mantenimento del suono a cursore fermo descritto da [AudioStretch](https://www.audiostretch.com/); questa implementazione è autonoma e non riproduce un algoritmo proprietario documentato.

## Verifiche automatiche

- Build Release tramite `Build.ps1`; eseguibili aggiornati `DualDeckJuce-0.7.exe` e `DualDeckJuce.exe`.
- Test audio: 442 controlli, inclusi pitch a velocità zero e con sample rate differenti, attacchi, salti, file corti e valori non validi.
- Nuove regressioni: energia delle armoniche su tutto il segnale, inclusi i raccordi, con velocità 1×, 0,5×, −0,5×, 0,02× e −1×; mouse a 30/60 Hz; cambi di direzione e arresto; relazioni stereo in opposizione di fase; accordo congelato e note diverse nei canali sinistro e destro.
- Test filtro tastiera: superato.

Il precedente controllo del movimento escludeva i raccordi e confrontava il PCM con i vecchi grani corti. È sostituito da una misura delle frequenze effettivamente udibili, includendo tutti i campioni dei raccordi.

## Confronto ripetibile

Il probe locale `test-output/ScrubComparison.cpp` confronta il motore attuale con la copia precedente in `backups/scrub-before-2026-10-26/`. Si esegue con `test-output/Compile-ScrubComparison.ps1`; output in `test-output/scrub-comparison.txt`. Sono disponibili anche `scrub-before.wav` e `scrub-after.wav` per lo scrub inverso con aggiornamenti a 30 Hz.

Segnale sintetico stereo a 48 kHz: somma di 220, 913 e 1761 Hz. Misura su 0,75 s dopo l'avvio, finestra Hann, energia nei sette bin intorno a ciascuna frequenza originale; la quota residua indica energia fuori da queste bande.

| Movimento | Energia nelle bande originali, prima | Dopo |
|---|---:|---:|
| 1× | 100,00% | 100,00% |
| 0,5× | <0,001% | 99,87% |
| −0,5× | <0,001% | 99,90% |
| Fermo | 100,00% | 100,00% |
| −0,5×, mouse a 30 Hz | 7,83% | 99,49% |

Nella misura locale finale il calcolo richiede circa 95–98 ms per un secondo di scrub continuo e 15 ms per un secondo di freeze, per una sola uscita. Il blocco più lento del probe a 128 campioni, con un salto incluso, è stato di circa 1,43 ms rispetto a un intervallo di 2,67 ms; l'avvio circa 0,97 ms. Questi numeri dipendono dal PC e non sono garanzie di latenza del dispositivo. È stata mantenuta la FFT originale da 2048 campioni dopo aver confrontato una variante da 4096, più costosa senza vantaggi misurati in questi segnali.

## Limiti della verifica

I risultati misurano segnali sintetici e non dimostrano equivalenza percettiva con AudioStretch. Resta da ascoltare il risultato su voce, musica e percussioni con l'uscita abituale. Nessuna prova di ascolto fisico viene dichiarata come eseguita.


