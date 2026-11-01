# DualDeck: primo avvio su Windows x64

Questa è un'anteprima 1.0.0-rc1, non la versione 1.0 stabile. Leggere
RELEASE-CHECKLIST.md per le verifiche ancora aperte. Il pacchetto non è firmato
digitalmente; SHA256SUMS.txt permette il confronto degli archivi della release.

1. Estrarre tutto lo ZIP in una cartella scrivibile, per esempio DualDeck nei
   propri Documenti. Non avviare direttamente dentro lo ZIP.
2. Avviare DualDeck.exe. Per le normali funzioni della soundboard non servono
   Python, compilatore o privilegi amministrativi.
3. Usare **Collega cartella** per i propri audio: gli originali restano al loro
   posto. Configurare **Audio e uscite** e premere **Applica uscite**.
4. Associare i tasti ai pad. Esc ferma i suoni quando ricevuto dall'app.

## Tastiera dedicata opzionale

I comandi locali funzionano senza driver aggiuntivi. Il blocco selettivo della
dedicata richiede Interception, installato separatamente dall'utente, e la DLL
ufficiale x64 v1.0.1 in keyboard-runtime accanto a DualDeck.exe.
Questa anteprima non distribuisce né installa quei componenti.

Sorgente e istruzioni del produttore: https://github.com/oblitum/Interception .
Installare un driver è distinto dall'avvio dell'app e richiede privilegi
amministrativi. Non modificare le impostazioni di sicurezza Windows per aggirare
un rifiuto. La compatibilità non è garantita per tutte le versioni e periferiche.
Le condizioni upstream e l'uso diretto SET_FILTER rimangono da chiarire prima
della release stabile: vedere THIRD-PARTY-NOTICES.md.

Predisposti i requisiti, riconoscere la dedicata con F12, rilasciare tutti i tasti
e attivare il blocco. Provare nel Blocco note: la dedicata comanda i pad senza
scrivere, la principale scrive. Disattivare e verificare il ritorno della
scrittura. L'attivazione dopo avvio o cambio dispositivo deve restare manuale.

## Discord e microfono

Il pacchetto non comprende un cavo audio virtuale. Se ne utilizza uno già
installato, scegliere la sua uscita in DualDeck e il corrispondente ingresso
in Discord, con le cuffie come monitor. Se la voce passa attraverso DualDeck,
evitare un secondo percorso della stessa voce verso Discord.

## Dati, aggiornamento e rimozione

Conservare BUILD-INFO.json: identifica il pacchetto come app portatile e impedisce
di ereditare i dati di un'eventuale checkout nella cartella superiore.
Impostazioni e associazioni sono in data/library.json accanto all'eseguibile;
il backup automatico è library.json.bak. Gli audio esterni non sono copiati.
Prima di aggiornare, chiudere normalmente l'app e copiare data e gli audio
necessari. Estrarre l'aggiornamento in una cartella nuova e trasferire data e
gli eventuali audio interni, preservando i percorsi. Conservare la copia
precedente. Se la libreria è corrotta, l'app conserva il file e sospende i
salvataggi: recuperare una copia valida a programma chiuso.

Per rimuovere la soundboard basta eliminare la sua cartella dopo aver salvato
i dati desiderati. Questo non disinstalla Interception o cavi virtuali: per
essi seguire le istruzioni del rispettivo produttore.

Obiettivo iniziale: Windows x64. Il collaudo su altro PC e delle diverse
configurazioni Windows è aperto. Non sono offerte build macOS, Linux o ARM64.
