# Riproduzione con lettura su richiesta

Build del 31 ottobre 2026, approvata dall'utente il 1 novembre 2026.
Riscontro: risposta dei pad quasi sempre sotto 10 ms, senza artefatti
percepibili durante la riproduzione. Questo è un riscontro d'uso, non una
misura garantita della latenza su tutti i dispositivi.

## Riscontro sulla stabilità — 4 novembre 2026

L'utente riferisce che il punto sulla stabilità audio con buffer piccoli sembra
non generare problemi nell'uso attuale. Richiede di lasciare non spuntata la
verifica delle prestazioni: resta da eseguire il collaudo in condizioni di
stress della macchina. Con un successivo riscontro conferma anche una sessione
di 60 minuti senza problemi audio: la relativa verifica d'ascolto è completata.
Configurazione Discord/microfono/cuffie, CPU, RAM, buffer, campionamento e tempi
dei callback non sono dettagliati nel riscontro. La verifica delle prestazioni
sotto stress resta aperta in [RELEASE-CHECKLIST.md](../RELEASE-CHECKLIST.md).

## Versione approvata

`DualDeckJuce-0.7.exe` e `DualDeckJuce.exe` sono copie identiche della prova,
archiviata in `old-builds/DualDeckJuce-0.7-streaming-test.exe`.
Hash SHA256 verificato su tutte le copie:

`612659E0C245DC753F5797EAF0634410532C3F1934C772BF8B0BD4D472BA4E2C`

Conservate anche le precedenti prove di tastiera completa e primo avvio,
senza ricompilare o modificare la build approvata.

## Comportamento

- Preparazione di 500 ms dal taglio iniziale di ogni suono, fino a 96 MiB
  complessivi di prefissi; lettura successiva durante la riproduzione.
- Quattro lettori in background dividono un pool di pagine da 64 MiB.
  Ogni decoder resta sul proprio thread; il callback audio non legge file.
- Richieste distribuite una pagina alla volta, con anticipo di 500 ms,
  per evitare che un suono accumuli letture davanti alle altre voci.
- Limite a 180 secondi, file originali intatti, tastiera completa, effetti,
  sovrapposizioni, loop e uscite separate conservati.
- L'editor mantiene il caricamento completo per forma d'onda e scrub,
  con cache separata da 384 MiB. Aprirlo può aumentare la memoria occupata.
  Il contatore audio non rappresenta tutta la memoria del processo.

## Verifiche automatiche della build provata

Report in `test-output/streaming-release/`:

- `library-tests.txt`: zero errori; limite di durata, file originali,
  tastiera, preparazione, reader Windows, seek, contenuto dei campioni,
  trim, loop, effetti, pagine protette e ricambio del pool.
- `interactions.txt`: zero errori nelle interazioni simulate di pad,
  tastiera grafica, ordinamento, paginazione e filtri.
- `probe.txt`: 329/329 suoni pronti, 3/3 associati; preparazione 2402 ms,
  prefissi 58,62 MiB. Sei richieste a suoni preparati: 0,003–0,012 ms.
  Queste misure escludono tastiera, dispositivi audio e Discord.
- Stress software con 32 suoni su due mixer a 48 e 44,1 kHz per 10 secondi:
  zero attese dei dati, incluso il ricambio delle pagine; audio in memoria
  122,62 MiB. Tempo medio per la coppia di callback 0,665 ms, massimo
  osservato 43,747 ms: il test non certifica le scadenze dei dispositivi reali.
- Sei suite CTest superate: comportamento audio, scrub, qualità audio,
  riverbero, filtro tastiera e comunicazione con il broker tastiera.

La revisione ha controllato proprietà delle pagine, riferimenti deboli dei
lavori accodati, recupero da file indisponibili, budget e affinità dei decoder.
La soluzione non presume di riprodurre l'architettura interna di Soundpad.
