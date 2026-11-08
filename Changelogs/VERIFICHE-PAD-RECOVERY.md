# Recupero dei pad con file mancanti

## Conferma dell'utente — 7 novembre 2026

L'utente conferma: «anche la pad recovery funziona bene».
Registrato il riscontro positivo nell'uso della funzione che conserva pad,
associazioni e impostazioni quando un file manca e li recupera al suo ritorno.

La funzione è già inclusa negli eseguibili standard `DualDeckJuce.exe` e
`DualDeckJuce-1.0.0-rc1.exe`, approvati per la variante spettrale A, SHA-256:
`4041B8177667356699121A79EFDEC1D179D928924770FB37C2BBAAEA9B2DC15A`.
Il relativo commit `fbf0ef3` precede la base corrente; i test di libreria
includono scansioni ripetute, salvataggio e riavvio durante l'assenza del file,
ritorno del file e conservazione delle impostazioni.

La copia `DualDeckJuce-1.0.0-rc1-pad-recovery-test.exe` è archiviata con verifica
dell'hash in `old-builds/approvazione-pad-recovery-2026-11-07/`.
`approvazione.json` conserva il riscontro e le impronte della prova e
dell'eseguibile standard. Questa registrazione riguarda la conferma dell'utente
e l'archiviazione della prova; non richiede una nuova compilazione.
