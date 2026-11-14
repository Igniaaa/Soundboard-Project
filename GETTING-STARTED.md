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
Lo ZIP contiene già `keyboard-runtime/LEGGIMI.txt`, con i passaggi per scaricare
e aggiungere la DLL corretta e verificarne l'hash. La cartella contiene solo
istruzioni: questa anteprima non distribuisce né installa DLL, driver o installer.

### 1. Scaricare Interception dal produttore

Chiudere DualDeck. Dalla [release ufficiale v1.0.1 di Interception](https://github.com/oblitum/Interception/releases/tag/v1.0.1)
scaricare **Interception.zip** nella sezione Assets, quindi estrarlo in una
cartella separata. Non scegliere gli archivi Source code. Conservare la cartella
`licenses` del produttore e consultarne le condizioni; gli avvisi sulle dipendenze
sono in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

### 2. Installare il driver

Se il driver è già installato e funzionante, passare al collegamento della DLL.
Altrimenti aprire il **Prompt dei comandi come amministratore**, entrare nella
cartella `Interception\command line installer` estratta e visualizzare le istruzioni:

```text
install-interception.exe
```

Per installare il driver:

```text
install-interception.exe /install
```

Controllare l'esito e, dopo un'installazione riuscita, **riavviare Windows**.
L'installazione è un'operazione separata da DualDeck: l'app non la esegue.
Se Windows rifiuta il driver, conservare il messaggio e usare la soundboard
senza blocco dedicato; non disattivare le protezioni di Windows per aggirarlo.

### 3. Collegare la DLL a DualDeck

Con DualDeck chiuso, copiare il file
`Interception\library\x64\interception.dll` dell'archivio ufficiale nella
cartella `keyboard-runtime` accanto all'eseguibile. La disposizione deve essere:

```text
DualDeck/
  DualDeck.exe
  keyboard-runtime/
    LEGGIMI.txt
    interception.dll
```

Usare la DLL **x64 v1.0.1**, anche se l'archivio contiene una cartella x86.
Non occorre copiarla nelle cartelle di sistema o registrarla con regsvr32.
Dalla cartella di DualDeck verificarla con il Prompt dei comandi:

```text
certutil -hashfile keyboard-runtime\interception.dll SHA256
```

Il valore atteso è:
`AB88164C11B1B48488772D4C3BFAA4509D5B0AE9DBC5A691DC4F96F0260443C8`.
DualDeck trova automaticamente la DLL in questo percorso; non c'è un percorso
da selezionare nell'app. Copiare la DLL non installa il driver.

### 4. Riconoscere e provare la tastiera

Aprire DualDeck senza privilegi amministrativi. In **Tastiera dedicata** premere
**Riconosci con F12**, poi premere e rilasciare F12 sulla tastiera da dedicare.
Rilasciare tutti i tasti, attivare il blocco e attendere lo stato attivo.
Provare con Blocco note in primo piano: la dedicata deve comandare i pad senza
scrivere, mentre la principale deve scrivere normalmente. Disattivare il blocco
e controllare che anche la dedicata torni a scrivere.

Dopo avvio o cambio dispositivo l'attivazione resta manuale. La compatibilità
non è garantita per tutte le versioni Windows e periferiche.

### Se il collegamento non funziona

- **DLL mancante o non leggibile:** controllare il percorso accanto
  all'eseguibile effettivamente avviato e riaprire DualDeck.
- **DLL diversa da quella verificata:** confrontare versione, architettura
  x64 e hash; usare il file della release ufficiale indicata.
- **Driver non disponibile o occupato:** controllare l'esito dell'installer,
  riavviare dopo l'installazione e chiudere altre applicazioni che usano
  Interception prima di riprovare.
- **Tastiera non riconosciuta:** verificare il collegamento e ripetere
  **Riconosci con F12** sulla dedicata.

### Disinstallare Interception

Disattivare il blocco e chiudere DualDeck e le altre app che usano Interception.
Dal Prompt dei comandi come amministratore, nella cartella dell'installer originale:

```text
install-interception.exe /uninstall
```

Dopo la disinstallazione riuscita riavviare Windows. Rimuovere la sola DLL dalla
cartella di DualDeck non disinstalla il driver.

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
