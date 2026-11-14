# Changelog

## 1.0.0-rc1 — in preparazione

- Barra dei menu classica e, nella build Foleys, personalizzazione visuale di
  tema, soundboard, tastiere ed editor degli effetti, con salvataggio separato.
- Schermata principale convertita in elementi Foleys reali: contenitori XML,
  pulsanti, campi, cursori, selettori, pad e griglia adattiva modificabili direttamente.
- Confermata la distribuzione senza componenti Interception; completate le
  istruzioni per installazione separata, collegamento della DLL x64, prova,
  risoluzione dei problemi e disinstallazione.
- Le scansioni conservano pad e impostazioni dei file mancanti: rimettendo un
  audio nello stesso percorso, il pad torna disponibile senza perdere tasto,
  nome, preferiti, volume, tagli o effetti, anche dopo un riavvio dell'app.
- Scrub e freeze con il bilanciamento più spettrale della variante A.
  Bilanciamento e recupero pad confermati dall'utente il 7 novembre 2026.
- Percorso open source AGPL-3.0-only e copyright Alessandro Piermatteo.
- ZIP Windows di anteprima con sorgenti corrispondenti, JUCE completo e SHA-256.
- Build locali in Visual Studio 2022 con preset CMake condivisi con la CI;
  setup, prove e packaging gestiti da Python, senza script PowerShell.
- Versione unica, metadati Windows, workflow e guida iniziale.
- Base corrente del repository, incluso streaming audio, profili, pad,
  effetti, editor, microfono e integrazione tastiera opzionale.
- Corretto il benchmark su installazione pulita: l'assenza di una libreria
  personale non fa fallire le prove sintetiche.
- Il pacchetto portatile mantiene i dati nella propria cartella anche se
  estratto all'interno di una checkout di sviluppo.

Limiti: RELEASE-CHECKLIST.md e THIRD-PARTY-NOTICES.md. Questa sezione non dichiara
la pubblicazione o la chiusura delle prove fisiche.
