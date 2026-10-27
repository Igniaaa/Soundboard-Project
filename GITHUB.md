# Preparare DualDeck per GitHub

La radice del repository è **questa cartella `juce-version`**, che contiene
già `.git`. Non occorre inizializzare un altro repository nella cartella
superiore `DualDeck-2`. Il ramo attuale è `main`. Al controllo finale del
27 ottobre il remote `origin` è già collegato a
`https://github.com/Igniaaa/Soundboard-Project.git` e il commit locale
`2daa603` coincide con il riferimento locale `origin/main`.

Per gli aggiornamenti successivi non occorre ricreare il repository:

```powershell
git status --short
git diff
git add .
git commit -m "Update audio review documentation"
git push origin main
```

Usare un messaggio di commit che descriva le modifiche effettivamente
presenti. Se `git status` non mostra modifiche, il commit non serve.
Le istruzioni di prima pubblicazione sotto valgono per un repository nuovo.

## Cosa includere

Sorgenti in `Source`, test in `Tests`, risorse in `Assets`, gli script
`scripts`, i file PowerShell, `CMakeLists.txt`, `.gitignore`, icone originali
e documentazione. I nuovi file devono entrare nel prossimo commit: limitarsi
ai file già tracciati lascerebbe fuori parti necessarie alla compilazione.

Il `.gitignore` esclude build, strumenti, dipendenze scaricate, eseguibili,
backup, report di prova, `data` e `sounds`. Non cancellare queste cartelle
locali: Git le esclude senza rimuoverle dal PC. Per copiare manualmente i
file tramite il sito di GitHub, queste esclusioni vanno rispettate a mano;
conviene usare Git o GitHub Desktop.

## Controllo prima del commit

```powershell
git status --short
git add --dry-run .
```

Controllare l'elenco, inclusi nuovi sorgenti, test e `Assets/DualDeck.ico`.
Per verificare perché un percorso personale viene escluso:

```powershell
git check-ignore data/library.json sounds/esempio.wav
```

Questa copia non contiene ancora il file `LICENSE` originale, pur citando
MIT nelle note. Recuperare la licenza e gli avvisi originali prima di rendere
pubblico il progetto; conservare anche `JUCE-LICENSE.md`.

## GitHub Desktop

1. Scegli **File → Add local repository** e seleziona `juce-version`.
2. Esamina le modifiche e crea un commit con una descrizione del contenuto.
3. Per questo repository usa **Push origin** per caricare nuovi commit.
   **Publish repository** serve invece quando un repository non è ancora
   collegato a GitHub; in quel caso scegli nome e visibilità.

## Alternativa da terminale

Dopo aver controllato i file, crea il commit locale:

```powershell
git add .
git commit -m "Update DualDeck JUCE 0.7"
```

Sul sito GitHub crea un repository vuoto, senza generare README, licenza o
gitignore. Copia l'URL effettivo, sostituisci il segnaposto e poi esegui:

```powershell
git remote add origin https://github.com/TUO-ACCOUNT/NOME-REPOSITORY.git
git push -u origin main
```

Il push richiede l'autenticazione al tuo account. Se in seguito `origin`
esiste già, controllalo con `git remote -v` prima di aggiungerlo di nuovo.
La preparazione locale non crea né pubblica un repository su GitHub.

Riferimenti: [guida ufficiale GitHub per un repository locale](https://docs.github.com/en/migrations/importing-source-code/using-the-command-line-to-import-source-code/adding-locally-hosted-code-to-github)
e [documentazione Git sulle esclusioni](https://git-scm.com/docs/gitignore).
