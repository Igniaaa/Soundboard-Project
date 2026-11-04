# Changelog

## 1.0.0-rc1 — in preparazione

- Le scansioni conservano pad e impostazioni dei file mancanti: rimettendo un
  audio nello stesso percorso, il pad torna disponibile senza perdere tasto,
  nome, preferiti, volume, tagli o effetti, anche dopo un riavvio dell'app.
- Percorso open source AGPL-3.0-only e copyright Alessandro Piermatteo.
- ZIP Windows di anteprima con sorgenti corrispondenti, JUCE completo e SHA-256.
- Build con Visual Studio/Build Tools 2022; toolchain portatile locale ancora
  utilizzabile, senza redistribuire il precedente bootstrap.
- Versione unica, metadati Windows, workflow e guida iniziale.
- Base corrente del repository, incluso streaming audio, profili, pad,
  effetti, editor, microfono e integrazione tastiera opzionale.
- Eseguibili 0.7 approvati invariati fino alla nuova prova utente.
- Corretto il benchmark su installazione pulita: l'assenza di una libreria
  personale non fa fallire le prove sintetiche.
- Il pacchetto portatile mantiene i dati nella propria cartella anche se
  estratto all'interno di una checkout di sviluppo.

Limiti: RELEASE-CHECKLIST.md e THIRD-PARTY-NOTICES.md. Questa sezione non dichiara
la pubblicazione o la chiusura delle prove fisiche.
