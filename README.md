# DualDeck — JUCE Edition

Soundboard nativa C++17/JUCE per Windows x64, con profili, cartelle audio
collegate, pad, tastiera dedicata, uscite principale e cuffie, mix microfono,
editor della forma d'onda ed effetti per suono.

Questa è una versione di sviluppo **0.7**. Scrub/freeze e qualità degli effetti
sono ancora in revisione
La tastiera dedicata controlla i pad anche in secondo piano, ma non sopprime
i propri tasti nelle altre applicazioni.

## Compilazione su Windows

Servono PowerShell, Python 3 e una connessione per il primo setup.
Da questa cartella:

```powershell
./Setup.ps1
./Build.ps1
./Test.ps1
```

Il setup scarica JUCE 9.0.4 e gli strumenti portatili con verifica SHA-256.
Per il compilatore Microsoft vengono presentati i termini della toolchain.
La build esegue i test CTest e genera `DualDeckJuce-0.7.exe`.
Al primo avvio collega le cartelle dei tuoi suoni e configura le uscite.

## Documentazione

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

Le note del progetto attribuiscono il codice originale di DualDeck alla
licenza MIT. Questa copia non contiene ancora un file `LICENSE` del codice
originale: recuperare il testo e gli avvisi di copyright dalla fonte prima
di una pubblicazione pubblica. Non inventare autore o titolare dei diritti.
Per JUCE e le relative dipendenze, vedere [JUCE-LICENSE.md](JUCE-LICENSE.md).
La provenienza del bootstrap Microsoft è descritta in [LEGGIMI.md](LEGGIMI.md).
