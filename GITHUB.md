# Preparare DualDeck per GitHub

La radice del repository è **questa cartella `juce-version`**, che contiene
già `.git`. Non occorre inizializzare un altro repository nella cartella
superiore `DualDeck-2`. Il ramo attuale è `main`; al momento della verifica
non era configurato alcun remote.

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
3. Usa **Publish repository**, scegli nome e visibilità. Per una prima copia
   personale puoi mantenere il repository privato.

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
