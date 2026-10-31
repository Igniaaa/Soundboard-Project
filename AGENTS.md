# Gestione delle build approvate

Preferenza esplicita dell'utente: quando una versione di prova viene
approvata, aggiornare gli eseguibili principali e spostare subito le copie
di prova nella cartella `old-builds`, già esclusa dal `.gitignore`.

- In radice lasciare `DualDeckJuce-0.7.exe` e `DualDeckJuce.exe` come copie
  della versione approvata corrente; verificare che gli hash coincidano
  con la build provata prima di archiviare le copie di prova.
- Conservare in `old-builds` le build di prova e quelle superate, anche
  quando una copia di prova è identica alla versione appena approvata.
- Non sovrascrivere build diverse già archiviate: in caso di collisione,
  aggiungere una data o un suffisso distintivo al nome archiviato.
- Se un file in uso impedisce copia o spostamento, non chiudere forzatamente
  l'applicazione; segnalare quale operazione resta da completare.

# Blocco della tastiera dedicata

L'utente richiede solo soluzioni gratuite per il blocco selettivo.
Per lo stato del lavoro, le verifiche simulate e i limiti del candidato
driver gratuito, leggere `TASTIERA-BLOCCO.md`.
