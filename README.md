# DualDeck — anteprima open source 1.0.0-rc1

Soundboard nativa C++17/JUCE per Windows x64, con profili, cartelle audio
collegate, pad, tastiera dedicata, uscite principale e cuffie, mix microfono,
editor della forma d'onda ed effetti per suono.

**1.0.0-rc1 in preparazione**, sotto AGPL-3.0-only. La 1.0 stabile non è ancora
pubblicata: restano prove fisiche, collaudo su altri PC e verifica dell'integrazione
Interception. La tastiera dedicata può sopprimere i tasti nelle altre applicazioni
tramite Interception esterno opzionale. DLL, driver e installer non sono inclusi
nel pacchetto di anteprima; le normali funzioni soundboard non li richiedono.

Per lo ZIP Windows vedere [Primo avvio](GETTING-STARTED.md).
Il pacchetto comprende sorgenti corrispondenti, JUCE completo, licenze e hash.

## Compilazione su Windows

Servono PowerShell, Python 3.10+, Visual Studio/Build Tools 2022 con C++ x64 e
Windows SDK, e una connessione per il primo setup. Vedere [BUILDING.md](BUILDING.md).
Da questa cartella:

```powershell
./Setup.ps1
./Build.ps1 -Toolchain Installed -NoPromote -SkipTests
./Test.ps1
python -m unittest discover -s Tests -p test_release.py
```

Il setup scarica JUCE 9.0.4 e gli strumenti portatili con verifica SHA-256.
Il setup non installa compilatori o driver. La build genera
`build-msvc/bin/DualDeckJuce.exe`; i test CTest sono eseguiti da Test.ps1.
Le copie approvate rimangono invariate. La toolchain portatile già presente
localmente resta utilizzabile, senza distribuire il precedente bootstrap.
Al primo avvio collega le cartelle dei tuoi suoni e configura le uscite.

## Documentazione

- [Compilazione e packaging](BUILDING.md)
- [Contributi](CONTRIBUTING.md)
- [Requisiti ancora aperti per 1.0](RELEASE-CHECKLIST.md)
- [Novità](CHANGELOG.md)

Il workflow GitHub verifica una checkout pulita e prepara artefatti di anteprima,
senza installare driver, catturare tasti o pubblicare release automaticamente.

- [Uso e funzioni](LEGGIMI.md)
- [Architettura e sorgenti](DOCUMENTAZIONE-CODICE.md)
- [Verifiche della versione](VERIFICHE-0.7.md)
- [Ultima revisione audio](VERIFICHE-AUDIO-REVIEW.md)
- [Diffusione del riverbero e verifiche](VERIFICHE-RIVERBERO.md)
- [Verifiche dello scrub](VERIFICHE-SCRUB.md)
- [Preparazione e caricamento su GitHub](GITHUB.md)
- [Editor visivo Debug](EDITOR-UI.md)
- [Limiti del blocco della tastiera](TASTIERA-BLOCCO.md)

## Dati locali e dipendenze

Il repository esclude `data`, `sounds`, `deps`, `tools`, build, backup,
report locali ed eseguibili. Il setup ricrea le dipendenze; la libreria
personale viene creata al primo avvio. Gli asset richiesti dalla build
sono in `Assets` e devono essere inclusi nei commit.

## Licenze e provenienza

Copyright (C) 2026 Alessandro Piermatteo. Codice originale sotto
[GNU AGPL versione 3](LICENSE), senza garanzia. Vedere [ambito e provenienza](COPYRIGHT.md)
e [avvisi delle dipendenze](THIRD-PARTY-NOTICES.md). JUCE e Interception conservano
condizioni proprie; i driver Interception non sono presentati come open source.
