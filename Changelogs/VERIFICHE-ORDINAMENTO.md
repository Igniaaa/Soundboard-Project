# Numerazione secondo l'ordinamento — 27 ottobre 2026

Il numero mostrato sui pad ora corrisponde alla posizione del suono nell'ordinamento selezionato sull'intero profilo. Si applica a nome, data di modifica, dimensione, tipo di file e ordine di aggiunta, in entrambe le direzioni.

Per esempio, ordinando per nome i suoni `Suono 10`, `Suono 2`, `Suono 1` diventano `#1 Suono 1`, `#2 Suono 2`, `#3 Suono 10`. Invertendo la direzione, `#1` passa a `Suono 10`. L'ordine naturale conserva `Suono 2` prima di `Suono 10` in direzione crescente.

## Comportamento

- Numeri coerenti sulle tile, nel pannello del suono selezionato, sulla tastiera grafica e nelle descrizioni di accessibilità.
- Ricerca `#numero` coerente con il numero mostrato dopo il riordino.
- Numerazione continua tra le pagine, calcolata prima di applicare ricerca e preferiti.
- Le pagine restano divise per cartella; ogni suono conserva il numero nell'ordine completo del profilo. Per questo una cartella o un insieme filtrato può mostrare numeri non consecutivi.
- La rinomina di un suono aggiorna anche la numerazione quando l'ordine dipende dal nome.

`padDisplayNumbers()` calcola il rango visualizzato; `PadTile::index` continua a individuare il pad memorizzato per selezione e riproduzione, mentre `displayNumber` contiene il numero mostrato. L'ordine scelto è salvato con le impostazioni già presenti nel profilo; non serve una migrazione del JSON.

## Verifiche eseguite

- `Build.ps1`: compilazione Release completata; test CTest audio e filtro tastiera superati.
- `--self-test`: criteri e direzioni, parità stabili, ricerca per ciascun numero, preferiti, pagine per cartella, profilo vuoto e persistenza superati. Verificata anche la lettura della libreria esistente di 329 pad.
- `--interaction-smoke`: cambio del selettore Nome, inversione della direzione, aggiornamento del suono selezionato e della tastiera, seconda pagina da `#25`, ricerca `#25`, filtro preferiti, rinomina e pressione/rilascio del pad corretto dopo il riordino superati.

Report in `test-output/pad-order/library-tests.txt` e `test-output/pad-order/ui-interactions.txt`. Copia dei sorgenti precedenti in `backups/pad-order-before-2026-10-27/`.

## Conferma dell'utente — 27 ottobre 2026

Dopo la consegna dell'eseguibile aggiornato, l'utente ha confermato: «bene, l'ordinamento funziona correttamente».

L'ordinamento con numerazione concorde al criterio selezionato è quindi confermato anche nell'uso dell'utente. Questo riscontro si aggiunge alle verifiche automatiche sopra riportate; non specifica quali singoli criteri, filtri o dispositivi siano stati provati manualmente.
