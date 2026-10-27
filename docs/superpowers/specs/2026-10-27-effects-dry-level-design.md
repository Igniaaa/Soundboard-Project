# Mix effetti senza attenuazione dell'attacco

## Intento

Alzare il mix di delay e riverbero non deve abbassare il primo suono.
L'utente vuole code più presenti, con protezione graduale del totale quando
la somma raggiunge il limite di uscita.

## Diagnosi

Ogni effetto divideva l'intera somma per `1 + mix`. Con un impulso a 0,8,
il test di regressione misura 0,4 con un effetto e 0,2 con delay e riverbero.
L'attenuazione avviene prima che sia presente qualsiasi riflessione.

## Approcci

1. **Scelto dall'utente:** suono diretto più code, con limiter stereo sul
   totale quando necessario. Conserva l'attacco pieno; se il livello si
   accumula, la protezione può abbassare tutto il segnale in quel momento.
2. Ridurre solo le code per lasciare sempre pieno il diretto. Scartato perché
   l'utente preferisce un effetto più marcato.

## Modifica limitata

- Eliminare la divisione dell'intero segnale per `1 + mix` in delay,
  ping-pong e riverbero. Conservare la stabilizzazione già presente
  dell'ingresso del feedback e le correzioni delle code a fine file.
- Il limiter stereo rimane a 0,98; intervento rapido sui picchi, mantenimento
  del guadagno per circa 20 ms e recupero graduale di circa 100 ms. Il breve
  mantenimento evita di recuperare guadagno a ogni semionda di un suono
  sostenuto. Nessun lookahead, buffer aggiuntivo o nuova latenza.
- Nessun nuovo controllo, parametro salvato, cambio di schema, intervento
  sullo scrub o pubblicazione GitHub.

## Verifica

Prima della correzione: osservare il fallimento del test sull'attacco.
Poi verificare diretto pieno con effetti singoli e combinati, proporzionalità
dei livelli finché c'è spazio, uscita limitata sui sovraccarichi, distorsione
del limiter su bassi sostenuti, code e relazione stereo. Eseguire tutte le
suite CTest e compilare la build Release. L'ascolto reale resta separato
dalle misure automatiche.

Stato: approccio e documento approvati dall'utente; autorizzata la correzione
con test nella sessione corrente.
