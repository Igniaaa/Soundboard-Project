# Checklist per il rilascio di DualDeck 1.0

Stato aggiornato al **8 novembre 2026**: preparazione di **1.0.0-rc1**,
cioè una versione candidata da collaudare prima della 1.0 stabile.

## Come usare questa checklist

- **Casella spuntata:** esito già registrato, con i limiti indicati nella voce.
- **Casella vuota:** prova o attività ancora da completare e documentare.
- Per spuntare una prova, annotare quale versione è stata usata, cosa è stato
  fatto e quale risultato si è osservato. I test automatici e le prove fisiche
  hanno esiti distinti.
- Le prove finali devono riguardare la candidata destinata al rilascio. Un
  risultato su una versione precedente resta utile, ma va riconfermato quando
  cambiano le parti interessate.

Le sezioni 2–5 descrivono le prove pratiche. Le sezioni 1 e 6 riguardano anche
chi prepara il codice e il pacchetto da pubblicare.

## 1. Preparazione tecnica già eseguita

Le caselle seguenti registrano la preparazione locale documentata in
[VERIFICA-RELEASE.md](VERIFICA-RELEASE.md). I controlli sul pacchetto finale sono
elencati nella sezione 6.

- [x] **Autore e licenza del progetto indicati.** Copyright e licenza
  AGPL-3.0-only sono documentati.
- [x] **Testi delle licenze presenti.** Inclusi il testo AGPL e gli avvisi di
  JUCE; le dipendenze esterne sono identificate.
- [x] **Versione centralizzata.** Il file VERSION identifica la candidata e i
  metadati dell'eseguibile; le copie 0.7 approvate sono conservate.
- [x] **Procedura di preparazione ripulita.** Il setup standard usa gli
  strumenti documentati e non include il vecchio bootstrap con licenza incerta.
- [x] **Creazione del pacchetto predisposta.** Lo script seleziona i file
  distribuibili, include i sorgenti e verifica l'impronta dell'archivio JUCE.
- [x] **Automazione GitHub predisposta.** Il workflow compila, testa e prepara
  un'anteprima, senza pubblicare automaticamente una release.
- [x] **Compilazione e test locali da una copia pulita dei sorgenti eseguiti.**
  Esiti e limiti sono nel rapporto di verifica della preparazione.
- [x] **Eseguire davvero l'automazione su GitHub.** Dopo commit e push, aprire
  Actions e controllare il workflow **Windows build and preview**. Spuntare
  quando tutti i passaggi riescono e gli archivi di anteprima sono scaricabili;
  annotare il collegamento all'esecuzione e il commit provato.

## 2. Tastiera dedicata: funzionamento e recupero

Usare la candidata e tenere disponibili mouse e tastiera principale. Annotare
versione Windows e modello delle tastiere. Il funzionamento ordinario del
blocco ha una conferma storica dell'utente; le prove sotto restano da registrare
sulla candidata. Queste prove richiedono il backend tastiera già predisposto.

- [x] **La dedicata comanda i pad senza scrivere nelle altre app.** Aprire
  Blocco note in primo piano e attivare la dedicata. I tasti associati devono
  avviare e fermare i pad corretti, senza scrivere in Blocco note; la principale
  deve continuare a scrivere normalmente. Includere un loop tenuto premuto.
- [x] **Disattivare il blocco restituisce la tastiera.** Spegnere
  l'interruttore della dedicata: deve tornare a scrivere in Blocco note e non
  devono restare pad attivati dal tasto ormai rilasciato.
- [x] **Chiudere normalmente DualDeck restituisce la tastiera.** Con il blocco
  attivo, chiudere l'app normalmente. La dedicata deve tornare a scrivere.
- [x] **Scollegare e ricollegare non blocca un'altra tastiera.** Scollegare la
  dedicata durante l'uso: la sessione deve disattivarsi e i suoi suoni fermarsi.
  Dopo il collegamento, controllare l'identità e riattivare manualmente; la
  principale deve rimanere utilizzabile.
- [x] **Recupero dopo arresto inatteso della finestra principale.** È una prova
  tecnica controllata: interrompere il processo principale della candidata e
  verificare che la dedicata torni utilizzabile senza riavviare Windows.
- [ ] **Recupero dopo arresto inatteso del processo ausiliario tastiera.** Il
  processo ausiliario, chiamato broker, gestisce il blocco. In una prova tecnica
  separata, interrompere solo quello e verificare il rilascio della dedicata e
  la segnalazione del problema nell'app. Identificare i processi prima della prova.
- [x] **Riavvio del PC.** Dopo il riavvio entrambe le tastiere devono essere
  utilizzabili normalmente. Aprendo DualDeck, il blocco deve richiedere una
  nuova attivazione manuale. Anche senza riavvio*
- [x] **Componente tastiera assente.** Su una copia o un ambiente di prova
  senza DLL/driver Interception, verificare che la soundboard resti utilizzabile
  e che l'attivazione del blocco mostri un errore comprensibile. Esiste già una
  prova sintetica senza runtime; manca la conferma nell'uso reale.

Se la finestra rimane bloccata ma il suo processo è ancora attivo, il broker può
rilasciare la tastiera autonomamente; lo stop dei suoni già partiti attende la
ripresa della finestra. Annotare separatamente rilascio dei tasti e stop audio.
Dettagli: [TASTIERA-BLOCCO.md](TASTIERA-BLOCCO.md).

## 3. Audio: ascolto prolungato, carico e prestazioni

L'utente riferisce che nell'uso attuale non sembrano esserci problemi audio.
**La sessione di 60 minuti è confermata; la prova delle prestazioni sotto stress
della macchina resta aperta**, come richiesto dall'utente.

- [x] **Almeno 60 minuti senza problemi audio udibili.** Conferma del 4 novembre 2026.
- [ ] **Documentare la configurazione e le misure della sessione.** Annotare
  uscite audio, microfono, percorso verso Discord e ascolto in cuffia. In
  **Audio e uscite**, leggere frequenza di campionamento e buffer in campioni;
  in Gestione attività, rilevare CPU e RAM di DualDeck all'inizio e alla fine.
  Se questi dati non sono stati raccolti nella sessione confermata, registrarli
  in una successiva sessione documentata.
- [x] **Provare le combinazioni più impegnative dentro DualDeck.** Riprodurre
  più pad contemporaneamente, ripetere lo stesso pad in spam, usare loop,
  delay/riverbero alti, scrub avanti/indietro e freeze. Proseguire abbastanza da
  esercitare anche la lettura dei file durante la riproduzione. Spuntare dopo
  aver annotato i casi provati e verificato assenza di scatti, buchi o blocchi.
- [ ] **Provare mentre anche il resto del PC è sotto carico.** Usare DualDeck
  insieme a un'attività impegnativa abituale, per esempio un gioco o un render,
  includendo carico su CPU, memoria e disco. Ripetere le combinazioni audio
  precedenti e annotare attività concorrenti, durata, carico osservato e
  anomalie. Spuntare solo dopo questa prova: il buon esito a PC poco impegnato
  non chiude questa voce.
- [ ] **Scollegare e cambiare l'uscita audio.** Scollegare un'uscita in uso,
  controllare il messaggio, ricollegarla o scegliere un'altra uscita e applicare
  la scelta. L'audio deve riprendere senza bloccare l'app o perdere la libreria.

Per i **buffer piccoli** serve anche una misura tecnica: ogni callback, cioè
ogni elaborazione di un blocco audio, deve finire entro il tempo disponibile.
A 48 kHz, 128 campioni danno circa 2,67 ms; 256 danno 5,33 ms. Nell'app attuale
il buffer è quello predefinito del dispositivo: non c'è un selettore manuale.
Annotare quindi il valore effettivo. Per certificare questo requisito vanno
misurati i tempi dei callback e gli eventuali sforamenti; il valore nel footer
e la CPU media non bastano. La voce sulle prestazioni resta aperta fino al
collaudo sotto carico e alla raccolta delle misure disponibili.

## 4. Libreria: aggiornamento e recupero dei dati

Fare queste prove su una **copia della libreria**. Conservare la versione 0.7
precedente, la cartella data e gli audio necessari per poter confrontare gli esiti.

- [x] **Aggiornare dalla 0.7 alla candidata.** A programma chiuso, salvare una
  copia dei dati, preparare la candidata in una cartella nuova e trasferire la
  copia della libreria mantenendo i percorsi degli audio. Verificare profili,
  cartelle, associazioni dei tasti e impostazioni dei pad. Chiudere e riaprire:
  i dati devono risultare ancora corretti.
- [x] **Ripristinare il backup della libreria.** Nella copia di prova e con
  l'app chiusa, recuperare un library.json valido dal file library.json.bak.
  Riaprire e controllare i dati. Se si prova anche una libreria danneggiata,
  verificare che l'app segnali il problema e conservi il file da recuperare.
- [x] **Gestire cartelle o audio mancanti.** Usare la copia di prova con un
  percorso non più disponibile. Controllare la segnalazione e ricollegare il
  file o la cartella; i pad devono conservare impostazioni e associazioni.
  Conferma dell'utente sul recupero pad del 7 novembre 2026:
  [rapporto](Changelogs/VERIFICHE-PAD-RECOVERY.md).

Procedura e posizione dei file: [GETTING-STARTED.md](GETTING-STARTED.md).

## 5. Altri PC e accessibilità

- [x] **Primo avvio senza strumenti di sviluppo né Interception.** Su un
  account Windows di prova, estrarre tutto il pacchetto in una cartella
  scrivibile e avviare DualDeck.exe. Importazione degli audio, pad e uscite
  devono funzionare senza Python o compilatore. La tastiera dedicata opzionale
  deve segnalare i componenti mancanti come descritto nella sezione 2.
- [x] **Collaudo su un secondo PC Windows x64.** Ripetere primo avvio,
  caricamento della libreria e riproduzione con le uscite disponibili. Annotare
  versione Windows, dispositivi e problemi riscontrati. Il test sul solo PC di
  sviluppo non completa questa voce.
- [x] **Uso con la tastiera fisica.** Usare Tab, Maiusc+Tab, frecce, Invio e
  Maiusc+F10 dove previsti. Verificare focus riconoscibile, attivazione singola
  dei pad, rilascio dei loop e comportamento di Invio già associato a un pad.
  Sulla dedicata provare anche Bloc Num acceso/spento, Invio numerico, Stamp e Pausa.
- [ ] **Uso con un lettore di schermo.** Per esempio con Assistente vocale di
  Windows, raggiungere pad e controlli principali. Verificare che nomi, stato
  e azioni siano comprensibili e utilizzabili; annotare eventuali controlli
  irraggiungibili o non annunciati.

## 6. Controlli finali e pubblicazione

Queste attività riguardano chi prepara il rilascio. Il pacchetto attuale è una
candidata: la pubblicazione della 1.0 stabile viene dopo la chiusura delle
verifiche applicabili e la registrazione degli eventuali limiti dichiarati.

- [x] **Definire come distribuire l'integrazione Interception.** Scelta
  dell'utente dell'8 novembre 2026: conservare la funzione, escludere DLL,
  driver, installer e archivi Interception dal pacchetto e fornire istruzioni
  per download ufficiale, installazione separata e collegamento della DLL x64.
  Procedura in [GETTING-STARTED.md](GETTING-STARTED.md) e nel LEGGIMI generato
  per `keyboard-runtime`. Non richiede installazioni o modifiche al driver locale.
- [ ] **Verificare le condizioni applicabili all'integrazione esterna.**
  Documentare gli obblighi della libreria e l'applicabilità dei termini upstream
  all'accesso diretto SET_FILTER senza redistribuzione di driver e installer.
  La scelta di distribuzione non registra da sola una verifica completa di
  conformità; non è stata accertata una violazione. Vedere
  [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
- [ ] **Verificare la provenienza dei materiali distribuiti.** Controllare
  contributi di terzi, risorse e dipendenze; conservare i rispettivi testi di
  licenza e avvisi nel pacchetto appropriato.
- [ ] **Controllare gli archivi finali.** Creare il pacchetto dallo stesso
  insieme di sorgenti usato per compilare e testare il binario. Verificare
  versione, apertura degli ZIP, sorgenti inclusi, impronte di SHA256SUMS.txt e
  assenza di librerie personali, audio, strumenti e runtime Interception.
  Modifiche ai file dello snapshot dopo la build richiedono una nuova build.
  Procedura: [BUILDING.md](BUILDING.md).
- [ ] **Dichiarare lo stato della firma digitale.** Verificare se l'eseguibile
  finale è firmato. Se resta non firmato, indicarlo nelle istruzioni di download
  e primo avvio. Gli hash servono a confrontare i file e non sono una firma.
- [ ] **Provare e approvare il pacchetto esatto da consegnare.** Estrarlo in una
  cartella nuova e verificare avvio e funzioni principali. Registrare versione,
  impronta dell'eseguibile, data e accettazione dell'utente, senza estendere
  l'approvazione a condizioni non provate.
- [ ] **Preparare e pubblicare la 1.0 stabile.** Solo dopo il collaudo, adeguare
  lo script che oggi accetta soltanto versioni rc, impostare VERSION a 1.0.0,
  ricompilare, rieseguire i controlli e rigenerare i pacchetti. Creare tag e
  GitHub Release sul commit corrispondente, allegando binari, sorgenti e hash.

## Cosa annotare quando si completa una prova

Registrare l'esito nel rapporto pertinente in Changelogs. Per esempio:

```text
Data e versione/build:
PC, versione Windows e dispositivi:
Prova eseguita e durata:
Configurazione audio e attività concorrenti, se pertinenti:
Risultato osservato ed eventuali anomalie:
Misure raccolte o log disponibili:
```

Le prove sintetiche eseguite da Test.ps1 sono documentate in
[BUILDING.md](BUILDING.md); il riscontro d'ascolto e il limite ancora aperto sulle
prestazioni sono in [VERIFICHE-STREAMING.md](Changelogs/VERIFICHE-STREAMING.md).
Questa checklist descrive le attività: non avvia automaticamente prove di crash,
installazioni di driver, push, tag o pubblicazioni.
