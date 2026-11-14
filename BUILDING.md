# Compilare e verificare DualDeck

## Requisiti

- Windows x64.
- Visual Studio 2022 con **Desktop development with C++**, MSVC v143 x64,
  Windows SDK e CMake tools.
- Python 3.10 o successivo.
- Connessione Internet al primo setup per scaricare JUCE 9.0.4.

Il progetto usa CMake e presenta quattro preset condivisi tra Visual Studio e
CI: `vs-release`, `vs-foleys`, `vs-foleys-designer` e `vs-ui-debug`. Ognuno ha
una directory di build `build-vs-*` separata; i primi tre producono
`bin/DualDeckJuce.exe`.
Il preset Debug è destinato all'editor visivo della UI.

## Prima compilazione in Visual Studio

1. Aprire un terminale nella cartella del progetto ed eseguire:

   ```text
   python scripts/dev.py setup
   ```

   Il comando scarica JUCE nell'archivio ufficiale, ne verifica l'hash e
   controlla che Visual Studio 2022 e il workload C++ siano installati. Non
   installa compilatori o driver.
2. In Visual Studio scegliere **File > Open > Folder** e aprire questa cartella.
3. Nella barra CMake selezionare **DualDeck Release (Visual Studio 2022 x64)**.
4. Scegliere il target `DualDeckJuce` e il comando **Build**. L'eseguibile sarà
   in `build-vs-release/bin/DualDeckJuce.exe`.
5. Per eseguire le prove automatiche:

   ```text
   python scripts/test.py --preset vs-release
   ```

Per compilare senza l'interfaccia di Visual Studio si possono usare gli stessi
preset da un terminale con CMake disponibile:

```text
cmake --preset vs-release
cmake --build --preset vs-release --parallel 3
python scripts/dev.py finalize --build-dir build-vs-release
```

`finalize` registra versione, hash dell'eseguibile e impronta di sorgenti e
JUCE. Non sostituisce le copie approvate. Per promuovere esplicitamente la
build a `DualDeckJuce.exe` e `DualDeckJuce-<versione>.exe`, aggiungere
`--promote`.

## Test e report

`scripts/test.py` esegue CTest e le prove applicative di persistenza, streaming,
interazioni e schermate. Termina un processo che supera 60 secondi e, se
disponibile, stampa il rapporto della prova fallita. I file sono salvati in
`test-output`.

Opzioni:

```text
python scripts/test.py --preset vs-release --audio-devices --performance
python scripts/test.py --preset vs-release --skip-keyboard-devices
python scripts/test.py --preset vs-foleys --build-dir build-vs-foleys --output-dir test-output/foleys
```

`--audio-devices` apre davvero le periferiche per una prova silenziosa.
`--performance` aggiunge misure sintetiche. `--skip-keyboard-devices` registra
esplicitamente lo skip delle prove sulle tastiere fisiche; le suite software
di tastiera e bridge in CTest restano obbligatorie.

Su GitHub Actions il runner Windows usa gli stessi preset con CMake CLI.
Le tastiere fisiche vengono saltate, i report sono conservati come artefatto
anche se il job fallisce e il pacchetto di anteprima viene costruito solo dopo
la registrazione della build.

Le regressioni del packaging si possono controllare con:

```text
python -m unittest discover -s Tests -p test_release.py
```

## Preset Foleys

Prima di selezionare il preset Foleys, scaricare e verificare la dipendenza:

```text
python scripts/dev.py setup-foleys
```

In Visual Studio scegliere **DualDeck Foleys** o **DualDeck Foleys Designer**.
Gli eseguibili finiscono rispettivamente in `build-vs-foleys/bin` e
`build-vs-foleys-designer/bin`. Per prove e finalizzazione, passare il preset o la
directory corrispondente agli strumenti Python. Le build Foleys non sostituiscono
la build standard approvata.

## Pacchetti di anteprima

Compilare e registrare lo stesso snapshot dei sorgenti, quindi creare gli
archivi locali:

```text
python scripts/dev.py finalize --build-dir build-vs-release
python scripts/release.py source --output test-output/source-snapshot.zip
python scripts/release.py bundle --binary build-vs-release/bin/DualDeckJuce.exe --output dist/preview-rc1
```

Il packaging rifiuta output già esistenti, sorgenti cambiati dopo la build,
binari modificati e archivi JUCE con hash diverso. Genera gli ZIP sorgenti e
Windows più `SHA256SUMS.txt`; non pubblica nulla. Mantenere `BUILD-INFO.json`
accanto al pacchetto per identificare la distribuzione portatile.

## Interception, solo sviluppo locale opzionale

La soundboard si compila e funziona senza Interception. Per preparare la DLL
opzionale e gli avvisi delle licenze:

```text
python scripts/dev.py setup-keyboard-bridge
```

Il comando scarica e verifica gli archivi ufficiali e copia soltanto la DLL
x64 nella cartella `keyboard-runtime`; non installa o avvia il driver. Prima
dell'uso leggere [Avvisi sulle dipendenze](THIRD-PARTY-NOTICES.md),
[Tastiera dedicata](TASTIERA-BLOCCO.md) e la [guida al primo avvio](GETTING-STARTED.md).

Per l'ispezione statica dei driver inclusi nell'archivio ufficiale:

```text
python scripts/dev.py inspect-driver --archive deps/Interception-v1.0.1.zip
```

Richiede `signtool.exe` dal Windows SDK. Estrae le risorse e verifica le firme
senza eseguire l'installer o i driver. Una firma valida non garantisce che
Windows autorizzi il caricamento del driver.
