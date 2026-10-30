# Verifiche — DualDeck JUCE 0.7

## Freeze con range ampliato — 30 ottobre 2026

Dopo la prova della consolidata, l'utente riferisce un audio più gradevole
e chiede un range di campioni leggermente più ampio a tempo zero.
Il range PCM del freeze stabile è aumentato del 25%, a circa 53–58 ms.
Build Release e cinque suite CTest superate, inclusi nuovi controlli sul
materiale incluso/escluso; 32 confronti di movimento continuo identici
alla consolidata. Disponibile `DualDeckJuce-0.7-freeze-range.exe`;
conservata la consolidata precedente. Dopo la prova l'utente conferma:
«Questa versione va bene». La variante è il nuovo riferimento approvato
all'ascolto. Dettagli in [VERIFICHE-SCRUB.md](VERIFICHE-SCRUB.md).

## Scrub di riferimento consolidato — 29 ottobre 2026

L'utente preferisce lo scrub di `DualDeckJuce-0.7-audio-review.exe`.
Ripristinato quel motore, conservando le altre correzioni; disponibile
`DualDeckJuce-0.7-consolidated.exe`. Ricompilazione Release e cinque suite
CTest superate. Il confronto con i sorgenti precedenti produce campioni
identici nei 24 casi sintetici verificati. Restano la conferma d'ascolto
della nuova compilazione e i limiti con buffer piccoli.
Dettagli e archivio dell'esperimento successivo in
[VERIFICHE-SCRUB.md](VERIFICHE-SCRUB.md).

## Revisione audio successiva — 27 ottobre 2026

L'utente segnala ancora timbro metallico nello scrub/freeze e gracchiare
con delay e riverbero a valori alti; conferma la separazione delle tastiere
dentro l'app. La [revisione audio](VERIFICHE-AUDIO-REVIEW.md) documenta le
correzioni, quattro suite CTest superate e la build separata
`DualDeckJuce-0.7-audio-review.exe`. Restano da confermare l'ascolto e la
stabilità con buffer piccoli. Le conferme storiche sotto non chiudono
questi nuovi riscontri.

## Aggiornamento — 27 ottobre 2026

| Verifica | Esito |
|---|---|
| Build Release aggiornata | Completata; `DualDeckJuce-0.7.exe` e `DualDeckJuce.exe` aggiornati |
| AudioCore | 482 controlli superati; test CTest audio e filtro tastiera superati |
| Ordinamento e numerazione | Test automatici su criteri, direzioni, ricerca per numero, filtri, pagine, dettagli e tastiera grafica superati |
| Libreria corrente | 329 pad letti; MP3, WAV e M4A decodificati; 0 riferimenti mancanti nel self-test |
| Scrub, riscontro dell'utente | Confermato: «Bene lo scrub ora si sente bene» |
| Ordinamento, riscontro dell'utente | Confermato: «bene, l'ordinamento funziona correttamente» |

Dettagli e limiti delle verifiche in [VERIFICHE-SCRUB.md](VERIFICHE-SCRUB.md) e [VERIFICHE-ORDINAMENTO.md](VERIFICHE-ORDINAMENTO.md). I riscontri dell'utente sono distinti dai test automatici: non specificano tutti i casi o dispositivi provati. Questo aggiornamento registra gli esiti già ottenuti e le conferme ricevute; non introduce una nuova esecuzione dei test.

## Verifica storica — 22 ottobre 2026

22 ottobre 2026, Windows x64. Build Release con JUCE 9.0.4.

## Eseguite

Comandi: `Build.ps1` e `Test.ps1 -AudioDevices -Performance`. Le anteprime sono in `test-output`.

| Verifica | Esito |
|---|---|
| Build 0.7 | Superata; `DualDeckJuce-0.7.exe` generato |
| AudioCore | 373 controlli superati, inclusi mix del filtro, tempo, feedback e mix del delay |
| Tastiera | 16 controlli superati |
| Libreria | Schema 7 salvato e riletto; migrazione da schema 6 con vecchi valori filtro/eco verificata; schemi 3–5 ancora letti |
| Libreria corrente | 331 pad, 0 file mancanti; MP3, WAV e M4A decodificati; file dell'utente rimasto nello schema 6 durante i test |
| Interfaccia | Anteprima dell'editor con card Audio e tagli, Filtro passa basso e Delay; verificata anche a 960 × 640 con scorrimento verticale |
| Interazioni sintetiche | Invio sul pad; frecce, Invio e Maiusc+F10 nella tastiera grafica: superate |
| Dispositivi audio | Quattro uscite e l'ingresso Tonor XLR aperti durante prove silenziose; combinazioni di uscite e gestione degli errori superate |

## Misure locali

Il report ripetibile è `test-output/performance.txt`: caricamento della libreria attuale **191 ms**; ricerca e ordinamento su 50.000 pad sintetici **46 ms**; paginazione **24 ms**. Su 20.000 pad sintetici: copia per il worker **2 ms**, salvataggio **4,1 s** fuori dal thread dell'interfaccia e ricaricamento **4,0 s**. I valori dipendono dal PC e dal disco.

## Da ascoltare sul dispositivo

I test DSP verificano i campioni prodotti dal filtro e dal delay. Serve ancora ascoltare gli effetti su cuffie e Discord, soprattutto con impostazioni alte di mix/feedback, e verificare l'uso con tastiera fisica e lettore di schermo. La soppressione dei tasti della tastiera dedicata nelle altre applicazioni resta la fase separata descritta in `TASTIERA-BLOCCO.md`.

I test non hanno modificato `data/library.json` dell'utente. Non sono state eseguite operazioni Git.
