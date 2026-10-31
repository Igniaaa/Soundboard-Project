# Revisione audio dopo il riscontro dell'utente — 27 ottobre 2026

## Mix approvato all'ascolto — 30 ottobre 2026

Dopo aver provato `DualDeckJuce-0.7-clean-mix.exe`, l'utente conferma:
«Bene funziona benissimo». Questa è la nuova versione di riferimento
approvata per il mix, con il freeze precedentemente approvato conservato.
Aggiornate anche `DualDeckJuce-0.7.exe` e `DualDeckJuce.exe`, ora identiche
alla copia provata: SHA-256
`FC8276F8BA2F98788E09842B54C6581A55015B38F50164499B649F37DEDFFEE5`.
Su richiesta dell'utente, le copie di prova `DualDeckJuce-0.7-clean-mix.exe`
e `DualDeckJuce-0.7-freeze-range.exe` sono poi state spostate in `old-builds`,
verificandone gli hash. In radice restano i due eseguibili approvati.
Questa archiviazione va ripetuta dopo ogni futura approvazione, come
registrato in `AGENTS.md`; `old-builds` resta esclusa da Git.
Nessun processo è stato chiuso. Il riscontro riguarda l'uso provato;
non specifica tutti i file, dispositivi o buffer. La registrazione e la
copia degli eseguibili non modificano il codice né introducono nuovi test.

## Mix meno attenuato, opzione audio pulito — 30 ottobre 2026

L'utente conferma che più pad insieme funzionano, ma segnala una riduzione
di volume eccessiva sia con pad diversi sia ripetendo lo stesso pad.
Ha scelto «Audio pulito»: bilanciamento meno aggressivo e recupero più
rapido, conservando la protezione finale. La tastiera viene riconosciuta
ma continua a inviare i tasti alle altre applicazioni, come già documentato
in `TASTIERA-BLOCCO.md`; nessuna modifica a quella funzione in questa build.

Il bilanciamento delle copie applica metà della compensazione precedente
in dB. Con copie costanti coerenti a 0,2, due copie si stabilizzano a circa
0,283 e tre a 0,346, anziché riportare sempre il gruppo a 0,2. I pad diversi
continuano a sommarsi senza bilanciamento comune prima del limiter.
Quando resta una sola copia, il target del gruppo torna direttamente a
uno con la transizione esistente di 10 ms, senza seguire l'energia delle
copie ormai terminate. I boost da cancellazione parziale sono ancora
rimossi immediatamente quando resta una sola copia.

Il limiter stereo resta a 0,98, senza nuova saturazione o lookahead.
Il recupero ha costante di tempo 40 ms invece di 100 ms; il mantenimento
passa da 20 a 25 ms per coprire i semiperiodi a 20 Hz. La riduzione
necessaria durante una somma forte continua rimane: questi interventi
riducono il bilanciamento aggiuntivo e il tempo del calo dopo il picco.

Probe sul motore reale a 48 kHz: tre impulsi di 10 ms a 0,8 sopra un pad
costante a 0,2. Il tempo è misurato dall'inizio degli impulsi:

| Caso | Tempo | Prima | Dopo |
| --- | --- | ---: | ---: |
| Pad diversi | 50 ms | 0,09799 | 0,11440 |
| Pad diversi | 100 ms | 0,13813 | 0,17548 |
| Pad diversi | 200 ms | 0,17724 | 0,19799 |
| Copie dello stesso pad | 50 ms | 0,10775 | 0,13330 |
| Copie dello stesso pad | 100 ms | 0,14405 | 0,18089 |
| Copie dello stesso pad | 200 ms | 0,17942 | 0,19843 |

**Verifiche:** baseline con cinque suite superate; prima della modifica
i nuovi controlli falliscono sui livelli delle copie e sul recupero a
50/100/200 ms. Dopo la modifica tutte le cinque suite CTest passano:
554 controlli audio, 192 di time stretching, 151 di qualità audio,
32 del riverbero e 16 della tastiera. Le prove di recupero includono
44,1/48/96/192 kHz, pad distinti e copie, blocchi da 64/512 campioni con
uscite identiche, recupero monotono e continuo, picchi finiti entro 0,98
e rapporto stereo conservato. Le sinusoidi stabili in sovraccarico a
20/25/50 Hz mantengono residuo rispetto alla fondamentale circa 3e-8.
Verificati anche 32 copie deboli, recupero della copia superstite,
cancellazione perfetta e i precedenti controlli su effetti e freeze.
I sorgenti `SpectralScrub.h` e `PhantomScrub.h` sono invariati.
Revisione indipendente senza rilievi.

Compilazione e test completati; `Build.ps1` si è poi fermato sulla copia
finale perché `DualDeckJuce-0.7.exe` era aperto. Nessun processo è stato
chiuso. I nomi standard rimangono alla versione precedente; è disponibile
`DualDeckJuce-0.7-clean-mix.exe`, identico a `build-msvc/bin/DualDeckJuce.exe`,
SHA-256 `FC8276F8BA2F98788E09842B54C6581A55015B38F50164499B649F37DEDFFEE5`.
Chiudere l'app precedente prima di avviare questa copia, che usa gli
stessi dati. Il riferimento approvato `DualDeckJuce-0.7-freeze-range.exe`
è conservato; backup dei sorgenti precedenti in
`backups/clean-mix-2026-10-30/`.

Log prima/dopo e probe in `test-output/clean-mix/`; script del probe in
`test-output/mix-level-probe/Probe.ps1`. Design e piano in `docs/superpowers`.
Nessun commit o push automatico. La successiva conferma d'ascolto e
l'aggiornamento dei nomi standard sono registrati sopra. I test sintetici
non certificano assenza di distorsione su ogni segnale o dispositivo.

## Prima eco del delay a volume pieno — ultima revisione

L'utente segnala una seconda copia a volume originale dopo il tempo del
delay. La riproduzione con impulso stereo misura originale 0,8 e prima eco
0,8 con Mix 100% e Feedback 0. L'ingresso normalizzato per `1 - feedback`
non attenuava la prima eco in questa condizione.

Il solo ritorno wet del delay viene ora attenuato di 6,02 dB (fattore 0,5).
Il diretto mantiene il proprio volume; Mix regola le eco e Feedback
continua a regolarne il decadimento. Nessuna modifica a ping-pong,
riverbero, dati salvati o durata delle code. A Feedback 0 rimane una sola
eco, già più debole; a Mix 0 il delay resta spento.

La regressione è stata osservata fallire prima della correzione. Il test
del motore reale usa impulsi stereo a 44,1/48/96 kHz, Mix 0/50/100% e
Feedback 0/35/80%: verifica volume del diretto, prima e seconda eco,
polarità stereo, tempo esatto e assenza di partenze aggiuntive.
Con originale 0,8 e Mix 100%, le prime due eco attese sono 0,4/0
(Feedback 0), 0,26/0,091 (35%) e 0,08/0,064 (80%).

`Build.ps1` completato: cinque suite CTest superate, inclusi 87 controlli
di qualità audio. L'ascolto con i suoni e le impostazioni dell'utente resta
la verifica del risultato percepito.

La successiva segnalazione di gracchiare con il solo riverbero ha portato
alla [revisione della sua struttura](VERIFICHE-RIVERBERO.md): aggiunti
diffusori e smorzamento, conservando il livello del diretto e il limiter
descritti qui. La build corrente passa cinque suite CTest; sotto sono
documentate le revisioni precedenti.

## Correzione del livello iniziale — revisione successiva

L'utente segnala che la prima revisione attenuava troppo il suono già
all'avvio. Ha scelto e approvato code più forti con limiter sul totale,
senza attenuazione fissa del diretto. Le sezioni precedenti conservate sotto
descrivono la storia: la normalizzazione dell'intero mix è stata rimossa.

**Diagnosi riprodotta prima della correzione:** un impulso a 0,8 usciva a
0,4 con delay o riverbero e a 0,2 con entrambi al 100%. La divisione per
`1 + mix` agiva prima di ogni ripetizione; ora il primo campione resta 0,8
in tutti e quattro i casi (incluso ping-pong), conservando anche il destro
negativo a -0,4.

Rimane la stabilizzazione dell'ingresso del feedback per `1 - feedback`.
Le code vengono aggiunte al diretto. Se la somma raggiunge il limite,
il limiter stereo interviene sul totale: attacco rapido, mantenimento di
circa 20 ms e recupero di circa 100 ms. Non vengono introdotti lookahead,
buffer aggiuntivi o latenza. La forma d'onda può comunque essere modificata
dalla protezione su transienti o sovraccarichi: non è una garanzia universale
di assenza di distorsione.

**TDD e verifiche:**

- Regressione dell'attacco osservata fallire prima e passare dopo.
- A livelli sotto il limite, mix e feedback massimi mantengono la
  proporzionalità del segnale; delay più riverbero producono picco circa
  0,36 per una nota sorgente di picco 0,09, senza la precedente riduzione.
- Un basso sinusoidale a 50 Hz in sovraccarico misurava residuo rispetto
  alla fondamentale di 0,0172371 (circa 1,72%) con il vecchio limiter. Il
  mantenimento riduce quel residuo a circa 3e-8 dopo l'assestamento a
  44,1/48/96/192 kHz. Picchi entro 0,98 e relazione stereo conservata.
- Il test di recupero ha rilevato un arresto prematuro dell'accumulatore
  float a 96 kHz: 0,0999714 invece di circa 0,1 dopo un secondo. Il guadagno
  in doppia precisione converge a circa 0,0999973 alle quattro frequenze.
- `Build.ps1` completato: tutte le quattro suite CTest passano. AudioCore
  mantiene 525 controlli, time stretching 192, tastiera 16; la suite di
  qualità audio passa da 10 a 27 controlli.

Aggiornati `DualDeckJuce-0.7.exe`, `DualDeckJuce.exe` e la copia
`DualDeckJuce-0.7-audio-review.exe`. La verifica percettiva va effettuata
con gli stessi file che presentavano il problema. Design e piano sono in
`docs/superpowers`; report locali in `test-output/attack-regression-before.txt`,
`attack-regression-after.txt`, `limiter-hold-before.txt` e nel ledger locale
del piano. Nessuna modifica allo scrub in questa correzione.

La revisione indipendente non ha trovato problemi critici o importanti e
ha rieseguito i 27 controlli di qualità. Rimandati: asserzioni temporali
intermedie del recupero e approfondimento dei sub-bassi sotto 25 Hz, per i
quali un mantenimento di 20 ms non elimina ogni modulazione. A 20 Hz il
replay numerico del revisore misura residuo circa 0,00103. Le misure sopra
riguardano il caso a 50 Hz, non tutti i segnali bassi.

## Stato e build della prima revisione (storico)

L'utente riferisce che scrub e freeze suonano ancora metallici, e che delay
più riverbero gracchiano con mix e feedback alti. Conferma invece che la
separazione delle tastiere dentro l'app funziona. Questi riscontri aggiornano
le precedenti conferme: la qualità percettiva dello scrub resta aperta.

Compilazione Release completata e quattro suite CTest superate:

- `audio_behaviour`: 525 controlli.
- `scrub_time_stretch`: 192 controlli.
- `audio_quality`: 10 nuovi controlli.
- `keyboard_filter`: 16 controlli.

Inizialmente la copia finale prevista da `Build.ps1` era impedita
dall'eseguibile aperto. La compilazione finale ha completato anche la copia
di **DualDeckJuce-0.7.exe** e **DualDeckJuce.exe**. Nessun processo dell'utente
è stato chiuso durante la revisione. È disponibile anche la copia
**DualDeckJuce-0.7-audio-review.exe**. Tutti hanno SHA-256 uguale a
`build-msvc/bin/DualDeckJuce.exe`:
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
dei tempi con buffer piccoli. Questa revisione non ha eseguito comandi di
commit o push; al controllo finale lo stato esterno del repository risulta
aggiornato al commit `2daa603`, con `origin` configurato. Vedere [GITHUB.md](GITHUB.md).
