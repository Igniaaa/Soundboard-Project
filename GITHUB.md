# Preparare il repository e la release

La radice Git è juce-version. Non inizializzare un secondo repository in
DualDeck-2.

Includere Source, Tests, scripts Python, preset CMake, Assets, workflow .github,
VERSION e documenti di licenza/release.
Tests non è più esclusa globalmente: nuovi test necessari non devono sparire
da una checkout pulita.

Non includere data, sounds, deps, tools, build, dist, old-builds, backup o report
locali. Gli archivi della distribuzione vengono generati separatamente da
scripts/release.py, con una allowlist; Git non è il pacchetto binario.

Prima di creare un commit esaminare git status --short e git diff. Per i nuovi
file controllare esplicitamente il contenuto: non basta il diff dei file già
tracciati. Vedere BUILDING.md per compilazione e verifiche, RELEASE-CHECKLIST.md
per i requisiti ancora aperti e THIRD-PARTY-NOTICES.md per le dipendenze.

Il workflow Windows compila, verifica e prepara un'anteprima. Non crea tag o
GitHub Release. L'esecuzione remota va verificata dopo aver caricato il ramo;
il fatto che il file workflow esista non significa che sia già passato.

Prima della pubblicazione stabile: risolvere i requisiti aperti, provare su un
altro PC, confermare l'accettazione, controllare dati esclusi e sorgenti
corrispondenti; solo allora preparare il tag 1.0.0 e gli allegati della release.
La preparazione corrente non esegue commit, push o pubblicazione automatica.
