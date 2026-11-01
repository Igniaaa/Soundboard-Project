# Verifica della preparazione 1.0.0-rc1

1 novembre 2026. Stato: **anteprima locale per il collaudo; non 1.0 stabile**.
Codice originale: Alessandro Piermatteo, AGPL-3.0-only. Nessun push, tag o
pubblicazione GitHub eseguiti durante la preparazione.

## Risultati effettivamente osservati

| Verifica | Risultato |
| --- | --- |
| Baseline 818643d | Sei suite CTest superate prima delle modifiche |
| Build Windows x64 da snapshot dei sorgenti distribuibili | Riuscita con MSVC portatile locale, senza toolchain o dati inclusi nello snapshot |
| JUCE 9.0.4 | ZIP SHA-256 fissato; confronto di tutti i file estratti prima e dopo la build |
| Sei suite CTest finali | 6/6 superate, nessun fallimento |
| Persistenza, cartelle, profili, associazioni | Self-test superati |
| Lettore Windows, seek, streaming, overlap e loop | Self-test superati |
| Ring microfono e ricampionamento | Test sintetico superato; non prova udibile |
| Interazioni pad e tastiera grafica | Prove sintetiche superate |
| Anteprime normali e 960 x 640 | Generate; viste soundboard e tastiera dedicata controllate visivamente |
| Packaging e primo avvio portatile | 12 test Python: 11 superati, 1 saltato per privilegio symlink Windows non disponibile |
| Benchmark senza libreria personale | PASS; assenza dati saltata esplicitamente, prove sintetiche eseguite |
| Dati di checkout nella cartella superiore | Il pacchetto con BUILD-INFO.json non eredita la libreria del padre; regressione superata |
| Eseguibili locali approvati e data/library.json | Impronte confrontate prima/dopo: invariati |

Le regressioni del benchmark senza libreria e della selezione del progetto
padre sono state osservate fallire prima delle rispettive correzioni.
Anche le regressioni dei controlli JUCE e packaging sono state eseguite prima
della correzione e risultano positive dopo di essa.

L'eseguibile sottoposto ai test ha SHA-256:
`C5E4043EA2946C2AAE56B8B9AE1F1224091C037B8C090676456E8BAF5E7401BF`.
Versioni del prodotto/file Windows: 1.0.0-rc1. Copyright: Alessandro Piermatteo.
I documenti di esito vengono aggiunti allo snapshot finale senza modificare il
codice C++; l'identità del binario deve essere riconfermata prima del packaging.
BUILD-INFO.json e SHA256SUMS.txt registrano le impronte degli archivi effettivi.

## Misure sintetiche sul PC di sviluppo

Ricerca/ordinamento su 50.000 pad: 189,94 ms; paginazione: 32,16 ms.
Copia di 20.000 pad: 2,20 ms; salvataggio: 4.015,24 ms; ricaricamento: 3.373,59 ms.
Sono misure di una singola esecuzione locale, non limiti garantiti. Non misurano
CPU/RAM durante una sessione reale, latenza del dispositivo o ritardo Discord.

## Revisione indipendente e scelte

Una revisione in sola lettura ha trovato un rilievo importante: i sorgenti JUCE
compilati non erano legati allo ZIP distribuito. Corretto verificando l'albero
effettivo, imponendo JUCE_DIR esplicito e registrando l'identità della dipendenza.
Il packaging verifica anche che i sorgenti archiviati coincidano con lo snapshot
compilato e rifiuta modifiche durante la creazione dell'archivio.
Documenti/script referenziati sono inclusi nell'allowlist.

Miglioria minore rinviata: lo script generico controlla MZ, ma non interpreta
l'intero header PE/AMD64. La procedura di build seleziona x64; il tipo macchina
del binario effettivo viene verificato nel controllo locale degli artefatti.

Scelte applicate: riutilizzo della checkout su ramo dedicato; nessun commit
automatico; eseguibili approvati conservati; vecchio bootstrap senza licenza
esplicita conservato solo nei backup; Interception esterno e non incluso.
Costo: una nuova macchina richiede la toolchain Microsoft standard per compilare
e la cattura selettiva resta soggetta ai requisiti esterni documentati.

La revisione non ha giudicato prove hardware, qualità udibile o sufficienza
legale dell'integrazione Interception: sono requisiti aperti, non successi
dedotti dai test automatici. Non viene dichiarata una seconda revisione.

## Requisiti ancora aperti prima della 1.0

- Prova reale su secondo PC e account senza strumenti di sviluppo.
- Recupero fisico della tastiera dopo crash, riconnessione e riavvio.
- Sessione prolungata con ascolto Discord/microfono/cuffie, CPU, RAM e buffer.
- Aggiornamento e recupero di una copia reale della libreria dell'utente.
- Navigazione fisica/accessibilità e lettore di schermo.
- Chiarire l'integrazione diretta Interception SET_FILTER o escluderla dalla
  build pubblica; tenerla esterna non risolve automaticamente i termini upstream.
- Eseguire il workflow su GitHub: è predisposto ma non è ancora stato lanciato.

Dettagli operativi: RELEASE-CHECKLIST.md. Il pacchetto è non firmato digitalmente;
gli hash non sono una firma. Log locali in test-output/release-*-final.txt e
test-output/release-final-checks, esclusi dagli archivi pubblici e da Git.
