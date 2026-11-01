# Tastiera italiana completa — 31 ottobre 2026

Build di prova: `DualDeckJuce-0.7-full-keyboard.exe`.

La versione con il taglio automatico a 180 secondi è approvata: entrambe
le copie principali e `old-builds/DualDeckJuce-0.7-180s-test.exe` hanno
SHA-256 `147EDBCF25400887A0EF309CDB967C4C279CDC96A3032AB70F10A5BB7E9BF827`.
I sorgenti di quel riferimento sono conservati in
`backups/180s-approved-20261031-164335.zip`.

## Cambiamenti

- Layout ISO italiano con 105 codici distinti, inclusi tastierino,
  navigazione, modificatori destri e tasti di sistema.
- Separazione fisica di numeri, Invio e navigazione tramite scan code.
  La cattura dedicata resta esclusiva del broker; i comandi locali
  richiedono il focus e rispettano i campi di testo e i controlli grafici.
- Bloc Num normalizzato; Pausa/Break emette un impulso con rilascio
  interno, senza produrre input artificiale per Windows.
- Tasti su due righe, spazi senza azioni e scorrimento orizzontale
  sotto 800 pixel. I codici delle associazioni precedenti sono invariati.
- Esc, F12 e il tasto DJ restano riservati.

## Verifiche eseguite

- Compilazione Release riuscita; CTest: **6/6 superati**.
- `--self-test`: 105 tasti distinti, hitbox senza sovrapposizioni,
  clic destro e Maiusc+F10 su ogni tasto, salvataggio e rilettura delle
  associazioni; nessun doppio avvio con Invio sui controlli grafici.
- Test degli scan code: distinzione tastierino/navigazione/Invio,
  ripetizioni, pressione da più dispositivi, reset, varianti di Bloc Num,
  sequenza E1 di Pausa, Stamp e rilascio degli impulsi del broker.
- Verifica di regressione del taglio a 180 secondi e dei file originali:
  superata. Libreria esistente di 329 pad e decodifica MP3/WAV/M4A: superate.
- `--interaction-smoke`: superato, inclusi numerazione e ordinamento pad.
- `--keyboard-smoke`: registrazione Raw Input e quattro dispositivi
  rilevati; cattura dedicata lasciata disattivata.
- Anteprime della tastiera a 1380 × 880 e 960 × 640 ispezionate.
- Revisione del codice completata; correzione di Invio verificata con
  un test prima fallito e poi superato. `git diff --check` senza errori.

Report locali: `test-output/keyboard-full-final`;
anteprime: `test-output/keyboard-full-green/keyboard*.png`.

## Prova fisica ancora richiesta

Provare i nuovi tasti sulla tastiera dedicata reale, soprattutto Bloc Num
acceso/spento, Invio numerico, Stamp e Pausa. Le simulazioni non certificano
il comportamento di ogni firmware o dispositivo. Nei comandi locali,
con Bloc Num spento, Windows può anche spostare il focus grafico tramite
il tastierino; questo comportamento è mantenuto intenzionalmente.
