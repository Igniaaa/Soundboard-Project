# Verifiche — DualDeck JUCE 0.6

22 ottobre 2026, Windows x64. Build Release con JUCE 9.0.4 e toolchain portatile del progetto.

## Eseguite

Comandi: `Build.ps1`, `Test.ps1 -AudioDevices -Performance` e `build-msvc/DualDeckKeyboardIsolationProbe.exe 1500`.

| Verifica | Esito |
|---|---|
| Build Release 0.6 | Superata; `DualDeckJuce-0.6.exe` generato |
| AudioCore | 369 controlli superati, inclusi filtro, eco, mix microfono e misura software dell'avvio |
| Filtro tastiera | 16 controlli superati |
| Libreria e schema 6 | Round trip, microfono ed effetti, lettura schemi precedenti: superati |
| Libreria corrente | 331 pad, 0 file mancanti; MP3, WAV e M4A decodificati |
| Ring microfono e ricampionamento | Superato |
| Interazioni sintetiche | Invio sul pad; frecce, Invio e Maiusc+F10 nella tastiera grafica: superate |
| Interfaccia | Schermate pad, audio, tastiera, tastiera dedicata, editor e finestra 960 × 640 renderizzate |
| Uscite reali | Realtek, HyperX, FxSound e CABLE Input aperte riproducendo silenzio; coppie e dispositivo duplicato superati |
| Ingresso microfono | Due ingressi rilevati; Tonor XLR aperto con volume del microfono a 0 durante il test |
| Prototipo isolamento tastiera | Quattro collezioni enumerate; 0 tasti premuti durante i 1.500 ms di acquisizione; nessun tasto bloccato |

## Misure locali

Il report ripetibile è `test-output/performance.txt`. Nell'ultima esecuzione: caricamento della libreria corrente (331 pad) **194 ms**; decodifica a freddo di sei MP3 **2–166 ms**; ricerca e ordinamento su 50.000 pad sintetici **46 ms**; paginazione **25 ms**. Su 20.000 pad sintetici la copia coerente per il worker richiede **1,7 ms**; serializzazione e salvataggio richiedono **4,0 s**, ora fuori dal thread dell'interfaccia. Le modifiche ravvicinate condividono un solo worker e la richiesta pendente viene aggiornata con lo stato più recente. Il ricaricamento richiede **4,0 s**. Questi tempi dipendono dal PC, dai file e dal disco.

Il footer mostra il tempo software fino all'elaborazione nel callback audio dopo una pressione reale. Non misura il percorso hardware né Discord. Il precaricamento continua a riguardare i primi sei pad visibili; un clip non precaricato attende la decodifica.

## Verifiche fisiche ancora necessarie

1. Ascoltare voce e suoni insieme su CABLE Input/Discord e verificare l'ascolto opzionale del microfono in cuffia, anche quando le uscite hanno frequenze di campionamento diverse. Controllare assenza di ritorno o doppia voce.
2. Misurare con un loopback esterno il ritardo completo da tasto a cuffia e a Discord, sia con clip precaricato sia con clip non presente in cache.
3. Con due tastiere fisiche, verificare nuovamente Invio su un pad, loop premuto, frecce/Invio/Maiusc+F10 nella tastiera grafica e comportamento con Invio già assegnato a un pad. Provare un lettore di schermo.
4. Per bloccare la tastiera dedicata nelle altre applicazioni serve la fase driver descritta in `TASTIERA-BLOCCO.md`. Il prototipo attuale non sopprime tasti.

I test scrivono report, anteprime e fixture in `test-output`; non modificano `data/library.json` dell'utente. Non sono state eseguite operazioni Git.
