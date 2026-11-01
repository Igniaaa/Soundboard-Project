# Preparazione open source 1.0

Data: 1 novembre 2026. Richiesta: procedere con la preparazione per la
distribuzione open source Windows x64. Copyright del codice originale indicato
dall'utente: Alessandro Piermatteo. Percorso proposto: AGPL-3.0-only.

## Design e vincoli

Preparare una candidata 1.0.0-rc1 con funzioni attuali, senza cambiare DSP,
streaming, dati personali o eseguibili approvati. Usare la checkout corrente sul
ramo release/1.0-preparation (preferenza della piattaforma: riuso della checkout).
Non installare driver, pubblicare release, fare push o certificare prove fisiche
non eseguite. La build pubblica deve poter funzionare senza Interception.

Alternative considerate: distribuire insieme i componenti Interception, oppure
tenerli esterni. Scelta di preparazione: dipendenza esterna, per non attribuire
ai driver chiusi la licenza dell'app e non anticipare la verifica dei termini
relativi alla chiamata diretta SET_FILTER. MIT con licenza JUCE separata rimane
un'alternativa, ma non è il percorso gratuito open source proposto all'utente.

## Piano di implementazione

Esecuzione inline: writing-plans, test-driven-development,
verification-before-completion. Un controllo indipendente finale esamina i
file modificati. Nessun commit automatico: il risultato resta ispezionabile.

- [x] Licenze: testo AGPL, copyright, avvisi JUCE e inventario delle dipendenze.
  Conservare il vecchio bootstrap non licenziato solo in backups, escluso da Git;
  usare una toolchain Microsoft già installata o quella portatile locale.
- [x] Build: VERSION unica, metadati Windows, compilazione senza promozione,
  test su eseguibile e directory espliciti. Setup non installa toolchain/driver.
- [x] Distribuzione: script Python di packaging con allowlist e test di
  esclusione dati personali, rifiuto link e file mancanti, controllo SHA-256
  delle dipendenze; sorgenti dell'app e archivio JUCE esatto insieme ai binari.
- [x] GitHub: compilazione e test su Windows da checkout pulita; solo artefatti
  di anteprima, nessuna pubblicazione automatica. README, guida iniziale,
  contributi, note di versione e matrice delle prove manuali.
- [x] Verifica: build pulita locale da snapshot distribuibile con JUCE verificato,
  sei suite CTest, self-test/streaming/interazioni/anteprime, test del pacchetto,
  avvio da estrazione fuori dal progetto e confronto hash dei binari approvati.

## Criteri di accettazione e rischi da controllare

Nessun data/library.json, sounds, credenziali, log personali, toolchain o driver
nel pacchetto. Non sovrascrivere un output esistente. Ogni binario deve avere
i sorgenti esatti e le dipendenze necessarie a ricompilarlo. Non confondere una
candidata con 1.0 stabile. Backup e dati utente invariati. Un errore di build o
di verifica deve terminare la procedura con codice diverso da zero.

Le prove su altro PC, recupero fisico tastiera/crash/riavvio, ascolto prolungato
Discord e latenza reale rimangono aperte finché non esiste evidenza effettiva.
La compatibilità/redistribuzione Interception richiede verifica separata.

## Registro di esecuzione

- Baseline: commit 818643d, sei suite CTest superate prima delle modifiche.
- Regressioni packaging: test prima senza implementazione, poi positivi.
  Confronto sorgenti JUCE e cambiamenti durante packaging aggiunti dalla revisione.
- Installazione pulita: riprodotto FAIL del benchmark senza libreria personale;
  corretto per saltare solo la parte personale ed eseguire comunque i casi sintetici.
- Isolamento portatile: riprodotta lettura della libreria di una checkout padre;
  BUILD-INFO.json ora interrompe la risalita del rilevamento della root.
- Scelte: usare checkout corrente su ramo dedicato; conservare eseguibili
  approvati e bootstrap precedente nei backup; tenere Interception esterno.
  Costo: le prove hardware e i termini dell'integrazione restano requisiti aperti.
- Revisione indipendente: risolto il rilievo importante JUCE; inclusi i documenti
  referenziati e verificato il contenuto dell'archivio contro lo snapshot compilato.
  Rimandato il controllo PE/AMD64 nello script generico: l'architettura del binario
  effettivo viene controllata nella verifica finale, la build seleziona x64.
- Esito: sei suite CTest positive, self-test/interazioni/benchmark positivi,
  12 test Python eseguiti (11 positivi, 1 skip per privilegio symlink Windows).
  Le prove fisiche e il workflow remoto restano separati e aperti.
