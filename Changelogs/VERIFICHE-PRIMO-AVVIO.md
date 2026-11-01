# Ritardo della prima riproduzione — 31 ottobre 2026

Build di prova: `DualDeckJuce-0.7-fast-start.exe`.

L'utente segnala circa 100 ms alla prima pressione di ciascun suono e
0,7–10 ms alle successive. Il vecchio precaricamento preparava soltanto
sei pad visibili e veniva escluso nella vista Tastiera. La misurazione
iniziale della libreria ha rilevato decodifiche tra 1,77 e 83,49 ms sui
primi sei file: una causa compatibile con il ritardo riferito.

## Correzione

Il profilo attivo viene preparato in sottofondo in tutte le viste, dando
precedenza alle associazioni e alla pagina visibile. Un solo job contiene
le richieste; un cambio di piano annulla il precedente tra un file e il
successivo. Generazione e avanzamento sono aggiornati insieme.

I suoni associati restano prenotati durante la preparazione, così quelli
visibili non possono espellerli. Le prenotazioni sono rilasciate anche
quando il lavoro viene annullato. Il resto del profilo non espelle altri
clip e si interrompe sotto 64 KiB liberi. Anche Media Foundation rispetta
lo spazio disponibile, senza accorciare silenziosamente un clip per farlo
entrare. La normale riproduzione mantiene la cache LRU da 384 MiB.

## Risultati

- Compilazione Release e CTest: **6/6 superati**.
- Test prima fallito nella vista Tastiera, poi superato: tutti i 30 suoni
  sintetici sono in cache prima della pressione, incluso un pad fuori pagina.
- Cache ridotta: la preparazione secondaria non espelle un suono associato;
  alla fine le prenotazioni sono rilasciate e la normale riproduzione può
  sostituirlo. Verificati anche i budget zero e limitato di Media Foundation.
- Self-test completo, limite a 180 secondi, originali intatti, tastiera
  completa, libreria di 329 pad e interazioni grafiche: superati.
- Libreria reale: preparazione in **5,57 s**, **93/329** suoni pronti,
  associazioni pronte **3/3**, PCM **383,98 MiB**.
- Sui primi sei file preparati: richiesta iniziale al motore software
  **0,001–0,009 ms**. È una prova senza dispositivi: esclude ricezione del
  tasto, attesa del callback reale, buffer del driver e Discord.
- Revisione del codice completata; nessun problema bloccante residuo.

Report: `test-output/first-start-baseline/performance.txt` e
`test-output/first-start-final/`.

Non è garantito l'avvio immediato dei suoni fuori cache o premuti mentre
sono ancora in preparazione. Verificare il comportamento percepito sulla
tastiera e sulle uscite effettive dell'utente.
