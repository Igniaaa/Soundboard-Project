# Compilare e verificare DualDeck

## Windows x64 da una checkout pulita

Servono PowerShell, Python 3.10+, Git per ottenere la checkout e una connessione
durante il primo setup. Installare dagli strumenti ufficiali Microsoft Visual
Studio 2022 o Build Tools 2022 con **Desktop development with C++**, MSVC v143
x64 e Windows SDK. Accettare direttamente i termini del prodotto Microsoft.
Questi strumenti non sono redistribuiti nel pacchetto.

```powershell
.\Setup.ps1
.\Build.ps1 -Toolchain Installed -NoPromote -SkipTests
.\Test.ps1
python -m unittest discover -s Tests -p test_release.py
```

Setup scarica solo JUCE 9.0.4, CMake 4.4.4 e Ninja 1.13.2 da fonti ufficiali,
controllando SHA-256 anche per gli archivi già presenti; verifica l'albero JUCE
estratto prima e dopo la compilazione. Non installa compilatori o driver. Build usa il generatore
Visual Studio 2022 e produce build-msvc/bin/DualDeckJuce.exe.
Test esegue CTest, persistenza, streaming, interazioni sintetiche e anteprime.
Non attiva cattura dei tasti. Il runner GitHub non installa Interception.

## Strumenti portatili già disponibili

La toolchain MSVC portatile predisposta localmente continua a funzionare:

```powershell
.\Build.ps1 -Toolchain Portable -NoPromote
```

È possibile passarne la cartella con `-ToolRoot C:\percorso\checkout`.
Il precedente bootstrap derivato da un gist privo di licenza esplicita non è
incluso. Per nuove installazioni usare il percorso Microsoft descritto sopra.

## Directory separate

Build accetta `-BuildDirectory`, `-JuceDirectory` e `-ToolRoot`.
Test accetta `-Executable`, `-BuildDirectory`, `-OutputDirectory` e `-ToolRoot`.
Permettono di compilare uno snapshot e testarlo senza usare le copie approvate
o i dati personali. `-AudioDevices` apre davvero le periferiche per una prova
silenziosa; `-Performance` aggiunge misure sintetiche. Non equivalgono a un
collaudo udibile con Discord o alla misura della latenza fino al destinatario.

## Anteprima con sorgenti corrispondenti

```powershell
python scripts/release.py source --output test-output/source-snapshot.zip
python scripts/release.py bundle --binary build-msvc/bin/DualDeckJuce.exe --output dist/preview-rc1
```

Il secondo comando genera ZIP sorgenti, ZIP Windows di anteprima e
SHA256SUMS.txt. Rifiuta output esistenti e archivi JUCE diversi dall'hash previsto.
Non pubblica nulla. Il pacchetto Windows include sorgenti e JUCE 9.0.4 completo.
Per ricompilare offline JUCE, estrarre il suo ZIP in deps, ottenendo
deps/JUCE-9.0.4; gli strumenti di compilazione vanno predisposti separatamente.

Eseguire il packaging **sullo stesso snapshot da cui è stato compilato il
binario**, dopo i test. Build scrive un file .build.json accanto al binario;
il packaging ne verifica versione, impronta del binario, albero sorgenti e JUCE.
Un cambiamento dopo o durante la build richiede una nuova compilazione.
BUILD-INFO.json registra queste impronte nell'archivio. Un hash documenta il
contenuto; non sostituisce provenienza o firma.

## Interception, solo sviluppo locale opzionale

Setup-KeyboardBridge.ps1 prepara DLL e avvisi da archivi ufficiali verificati.
Non installa il driver e non fa parte del setup standard. Leggere prima
THIRD-PARTY-NOTICES.md e i termini upstream. L'app compila senza DLL; il pacchetto
pubblico di anteprima non deve contenere keyboard-runtime.
