# Revisione audio dopo il riscontro dell'utente — 27 ottobre 2026

## Stato e build

L'utente riferisce che scrub e freeze suonano ancora metallici, e che delay
più riverbero gracchiano con mix e feedback alti. Conferma invece che la
separazione delle tastiere dentro l'app funziona. Questi riscontri aggiornano
le precedenti conferme: la qualità percettiva dello scrub resta aperta.

Compilazione Release completata e quattro suite CTest superate:

- `audio_behaviour`: 525 controlli.
- `scrub_time_stretch`: 192 controlli.
- `audio_quality`: 10 nuovi controlli.
- `keyboard_filter`: 16 controlli.

La copia finale prevista da `Build.ps1` non riesce perché l'eseguibile
`DualDeckJuce-0.7.exe` è aperto. Nessun processo dell'utente è stato chiuso.
La build compilata è stata copiata in **DualDeckJuce-0.7-audio-review.exe**;
SHA-256 uguale a `build-msvc/bin/DualDeckJuce.exe`:
`3FF977A6101F158C8571B2E281B21800D9D03A798230A48F6C59EDBEE83D04E3`.
Chiudere l'app precedente prima di provare questo eseguibile, che usa la
stessa cartella dati. Non sono state effettuate prove di ascolto fisico.

## Delay, riverbero e ping-pong

Il feedback somma ripetutamente il segnale; il vecchio guadagno a regime
poteva raggiungere `1 / (1 - feedback)` per linea, poi gli effetti sommavano
il wet al dry senza compensazione. Una nota forte risonante raggiungeva il
limiter, deformando la forma d'onda. Ora l'ingresso del feedback è scalato
per `1 - feedback` e il mix è normalizzato per `1 + mix`. Questo conserva
la durata delle ripetizioni, riservando spazio nel livello di uscita.
I mix alti e le prime ripetizioni possono quindi risultare meno forti.
Il limiter rimane necessario per la somma di pad e microfono.

Ulteriori discontinuità corrette:

- La dissolvenza agisce sul segnale che entra negli effetti: prima abbassava
  anche la coda, che tornava improvvisamente al livello pieno al primo
  campione successivo alla fine del file.
- Il delay non viene più tagliato dopo otto ripetizioni con feedback alto:
  la durata arriva al decadimento a -80 dB. Le code degli effetti in serie
  vengono sommate invece di conservare soltanto la più lunga.

Il test `AudioQualityTests.cpp` confronta una nota a 1 kHz di picco 0,09
con la stessa nota a 0,9, a mix e feedback massimi. Verifica la linearità
campione per campione, non solo il rispetto di un massimo numerico.
Sul vecchio codice l'errore massimo è **4,41999** e l'uscita tocca **0,98**;
il nuovo codice ha errore massimo inferiore a **0,00000024**, con picco
non superiore a **0,9**, nei quattro casi delay, riverbero, entrambi e
ping-pong. Verificati anche continuità a fine file e rilascio della coda.
Sono prove sintetiche a 48 kHz: non identificano ogni possibile causa del
gracchiare su hardware o file reali.

## Scrub e freeze

Le regioni spettrali classificate come rumorose non vengono più agganciate
in gruppi a massimi casuali: ciascun bin evolve indipendentemente, mantenendo
la stessa fase tra i due canali. Le regioni tonali conservano il phase
locking. L'obiettivo è ridurre le risonanze artificiali; non è una garanzia
di naturalezza della voce o di qualsiasi materiale reale.

Sul segnale sintetico con rumore più nota, la piattezza spettrale nella banda
2–6 kHz del freeze passa da **0,392179 a 0,457087**. Un valore maggiore qui
indica un rumore meno concentrato in righe; non misura da solo la qualità
percettiva. Le regressioni su pitch, armoniche e stereo continuano a passare.

La ricerca PCM di allineamento usa ora circa 48 candidati iniziali più
raffinamento locale, invece di una scansione con passo fisso di quattro
campioni, che a 192 kHz poteva valutare oltre 2.300 candidati per hop.
Nel probe locale su movimento con aggiornamenti a 30 Hz, incluso un salto,
il calcolo complessivo per secondo audio passa da circa **109 a 61 ms** a
96 kHz e da **227 a 122 ms** a 192 kHz. La misura a 48 kHz è sostanzialmente
simile (54 contro 53 ms).

## Buffer piccoli: limite ancora aperto

Un buffer da 128 campioni dura 2,67 ms a 48 kHz, 1,33 ms a 96 kHz e
0,67 ms a 192 kHz. Il callback deve completare tutto il lavoro prima della
scadenza. Il costo medio basso non basta quando un hop concentra analisi,
sintesi e raccordi in un singolo blocco.

In questa esecuzione i massimi della nuova versione durante il movimento
sono 16,07 / 2,31 / 4,83 ms rispettivamente a 48/96/192 kHz. La misura a
48 kHz ha un massimo isolato molto maggiore del percentile 99 (1,00 ms),
ma il probe non distingue interruzioni del sistema da costo DSP. Anche
l'avvio può richiedere più di un buffer alle frequenze alte. Questi dati
**non certificano assenza di dropout con buffer piccoli** e non misurano
la latenza del dispositivo o di Discord.

Probe ripetibile locale: `test-output/Compile-AudioReviewProbe.ps1`;
report `test-output/audio-review-comparison.txt`. I sorgenti precedenti
sono in `backups/audio-review-2026-10-27`. Queste cartelle sono escluse da Git.
Le regressioni permanenti sono in `Tests/AudioQualityTests.cpp`.

## Restante

Ascoltare la nuova build con gli stessi file che presentano il problema,
sia a effetti disattivati sia con delay e riverbero insieme. La qualità
metallica non viene dichiarata risolta senza questa verifica. Restano le
misure sulle uscite reali e il lavoro necessario per garantire il rispetto
dei tempi con buffer piccoli. Nessun commit o push è stato eseguito.
