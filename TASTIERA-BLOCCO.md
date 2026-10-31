# Blocco della tastiera dedicata: risultato del prototipo

## Versione approvata — 31 ottobre 2026

Dopo la correzione dell'avvio l'utente conferma: «Bene, funziona».
La build provata è ora il riferimento approvato, copiata e verificata in
`DualDeckJuce-0.7.exe` e `DualDeckJuce.exe`.
SHA-256 di entrambe: `0F56C4911C7474A8ADF9C9584726AEA835CA59CAC4EC3C22A62A62FA21F0A48D`.
La copia di prova `DualDeckJuce-0.7-keyboard-startup.exe` e le copie principali
precedenti sono conservate in `old-builds`, esclusa da Git. Nessuna app è stata
chiusa forzatamente. Tenere `keyboard-runtime` accanto agli eseguibili.

Il riscontro conferma il funzionamento riferito dall'utente; non equivale a
una verifica separata di crash, riavvio o di tutte le combinazioni di tasti.
Le sezioni seguenti documentano le prove e i limiti della fase di sviluppo.

## Correzione dell'avvio con filtro letto a zero — 31 ottobre 2026

Build provata e poi approvata: **`old-builds/DualDeckJuce-0.7-keyboard-startup.exe`**.
SHA-256: `0F56C4911C7474A8ADF9C9584726AEA835CA59CAC4EC3C22A62A62FA21F0A48D`.
Le copie approvate e il relativo audio sono invariati.

L'utente ha riprodotto «slot 1, letto 0x0000, richiesto 0xFFFF».
Il probe in sola lettura ha distinto il filtro zero da una risposta vuota:
`GET_FILTER` ritorna successo ma **zero byte**, con buffer da 2, 4, 8 e 16 byte.
Il disassemblato del driver ufficiale già identificato conferma che il dispatcher
implementa `SET_FILTER` (0x222010) ma non `GET_FILTER` (0x222020). Il getter della
DLL inizializza il valore a zero e non controlla la dimensione restituita;
interpretarlo come lettura valida impediva l'avvio e avrebbe reso inaffidabile
anche la verifica del rilascio.

`Source/KeyboardFilterChannel.h` conserva ora il risultato effettivo della
scrittura `DeviceIoControl`, compreso il codice Windows se viene rifiutata.
Usa il solo handle dello slot tastiera selezionato, nel contesto appena creato
dalla DLL ufficiale v1.0.1 x64. Il layout interno è ammesso solo per quella DLL,
che continua a essere bloccata in lettura e verificata tramite SHA-256 prima
della creazione del contesto. Non è un adattatore per DLL generiche o x86.
Sono rifiutati slot mouse, handle invalidi e maschere diverse da zero/0xFFFF.
I filtri degli altri handle del nuovo contesto rimangono ai valori iniziali;
non vengono applicati filtri ad altre tastiere o al mouse.

Questa conferma è il completamento della scrittura, **non una rilettura del
filtro**. Avvio e rilascio esigono entrambi la conferma; in caso di errore viene
chiuso comunque il contesto. Identità periodica, hotplug, heartbeat, revoca
preventiva della consegna e coda limitata restano operativi. Gli errori sono
ora evidenziati anche in fondo alla finestra.

Verifiche: sei suite CTest superate, 2395 controlli bridge; regressione prima
osservata fallire, poi passare. Revisione indipendente senza rilievi importanti.
Sul driver reale il medesimo helper ha applicato **solo il filtro zero** a un
nuovo contesto della selezione VID_3554/PID_FA09/REV_0102/MI_00:
`slot=1 zero-filter-ack=1 Windows=0`. Nessuna cattura nonzero in questo controllo,
nessun tasto letto o inviato e nessun driver installato o modificato.

Log: `test-output/keyboard-bridge/filter-read-diagnostic.txt`,
`keyboard-driver-disasm.txt`, `filter-ack-red.txt`, `filter-ack-green.txt`,
`filter-ack-build.txt`, `filter-zero-ack.txt`.

Chiudere normalmente la build precedente, aprire quella corrente e riprovare
l'interruttore sulla tastiera già selezionata. Blocco dei caratteri, pad e
rilascio fisico restano da confermare manualmente. Le sezioni seguenti sono
cronologia: la prima build è superata per questo errore di avvio.

## Prima build di prova del blocco reale (superata) — 31 ottobre 2026

Pronta `DualDeckJuce-0.7-keyboard-block.exe`, separata dalle due copie approvate.
SHA-256 prova: `A105553B2CEB7B2FAEF208D8632A7A10ED16A38EDDF91101AB99F9A67C8FEC89`.
Le due copie approvate conservano SHA-256 `FC8276F8BA2F98788E09842B54C6581A55015B38F50164499B649F37DEDFFEE5`.
Non sono stati installati o modificati driver, servizi o impostazioni di sicurezza.

La build integra un broker figlio senza finestra che carica la DLL Interception
ufficiale x64 da `keyboard-runtime`, con hash verificato prima del caricamento.
Il setup prepara DLL, licenze e sorgenti della libreria; non esegue installer.
La cartella runtime deve restare accanto all'eseguibile. Questa è una prova locale;
il materiale per un'eventuale distribuzione pubblica, compreso il percorso di
sostituzione/ricollegamento della libreria, richiede una preparazione separata.

Raw Input identifica con F12 l'istanza esatta; SetupAPI ne ricava il primo
HardwareID PnP. Il broker richiede una sola corrispondenza completa tra dieci
slot tastiera, applica il filtro solo a quella e verifica gli altri slot.
Gli eventi dei pad arrivano dalla coda del broker, con ripetizioni eliminate.
I comandi associati ai tasti locali sono sospesi durante collegamento/cattura;
restano utilizzabili mouse e attivazione esplicita dei controlli focalizzati
tramite Invio o accessibilità.

Perdita del collegamento, cambio dispositivo, coda piena o pacchetti invalidi
interrompono la sessione; la consegna viene revocata prima delle chiamate al
driver di inoltro o pulizia, così la coda non avvia pad durante il rilascio.
L'heartbeat è letto prima di campionare l'orologio locale, evitando falsi timeout
tra due aggiornamenti concorrenti. Il broker rilascia il filtro dopo 1500 ms
senza heartbeat della GUI o alla chiusura del padre. Se la GUI rimane bloccata,
lo stop dei suoni già in esecuzione attende la sua ripresa: il rilascio della
tastiera è autonomo, lo stop audio passa ancora dalla GUI.

Verifiche della build finale:

- Compilazione MSVC riuscita; **6/6 suite CTest**, zero fallimenti.
- **113 controlli tastiera** e **2381 controlli bridge**; simulazioni di coda,
  identità, timeout, cleanup, inoltro e concorrenza. Le regressioni di heartbeat,
  consegna durante cleanup e consegna durante inoltro sono state osservate
  fallire prima delle rispettive correzioni.
- Revisione indipendente: rilievi risolti, nessun altro problema importante rilevato.
- Probe passivo: contesto Interception aperto, uscita zero, quattro slot leggibili;
  nessun filtro applicato e nessun evento letto o inviato.
- Raw Input e PnP Windows enumerano le stesse quattro interfacce presenti,
  VID_3554/PID_FA09 e VID_1532/PID_0099. La precedente selezione
  VID_17EF/PID_608C è assente anche da Windows: non è stata sostituita
  automaticamente con un'altra tastiera.
- Interazioni sintetiche: Invio dei pad, frecce/Invio/Maiusc+F10 della tastiera
  grafica e ordinamenti/pagine/filtri superati; schermata dedicata controllata
  visivamente, con stato «Tastiera salvata non collegata» e attivazione disabilitata.

Log locali: `test-output/keyboard-bridge/build-resume-final.txt`,
`passive-driver-status.txt`, `raw-keyboard-status.txt`, `ui-interactions.txt`,
`dedicated-keyboard-page.png` e i log RED delle tre regressioni.

### Prova fisica ancora da eseguire

1. Chiudere normalmente la versione precedente e aprire la build di prova.
2. Ricollegare la tastiera voluta, se assente. Usare **Riconosci con F12** sulla
   tastiera dedicata, rilasciare tutti i tasti e attivare l'interruttore.
3. Con Blocco note in primo piano, verificare che quella dedicata avvii i pad
   senza scrivere e che la principale continui a scrivere.
4. Disattivare e verificare il ritorno della scrittura anche sulla dedicata;
   ripetere con chiusura normale e scollegamento/ricollegamento.

Soppressione e recupero sul dispositivo fisico, arresto anomalo del broker e
riavvio del PC non sono dichiarati verificati. Nessuna cattura reale è stata
attivata automaticamente. Le sezioni seguenti conservano la cronologia delle
fasi precedenti, superate dallo stato di questa sezione.

## Nuovo controllo autorizzato: Interception già caricato — 31 ottobre 2026

Su richiesta dell'utente è stata ripetuta e autorizzata la lettura dei
driver installati. Il controllo è ora riuscito: **Interception è già
installato e i servizi kernel `keyboard` e `mouse` risultano RUNNING**
(`sc.exe query`, uscita zero). Non sono stati installati, avviati, fermati
o modificati servizi o driver durante il controllo.

La tastiera selezionata nella libreria corrisponde all'interfaccia HID
VID_17EF/PID_608C/MI_00 presente e in stato OK. Usa `kbdhid`, `keyboard.inf`,
versione 10.0.26100.8972. Non dichiara filtri specifici per dispositivo;
il filtro **di classe** tastiera contiene invece `keyboard`, seguito da
`kbdclass`. È importante distinguere i due livelli: le sole proprietà PnP
del dispositivo non avrebbero mostrato il filtro di classe Interception.
Un'altra interfaccia HID dichiara il filtro specifico `RzDev_0099`, da
preservare. In totale sono state enumerate cinque interfacce della classe
tastiera, che non equivalgono necessariamente a cinque tastiere fisiche.

I file già installati hanno firma Authenticode Valid, firmatario Francisco
Lopes da Silva, e corrispondono alle risorse x64 NT61 del pacchetto ufficiale
già analizzato:

| Driver installato | SHA-256 |
| --- | --- |
| `C:\Windows\System32\drivers\keyboard.sys` | `2CB5EC142CFAC879BCE4A2F9549258DB972AEBBD24F4551B6B748B464EB7DBA9` |
| `C:\Windows\System32\drivers\mouse.sys` | `0F12D47D01864CA5E1EB663A52B3D2C060521E57B68FF99D70E7F01506E400F9` |

Questo dimostra il caricamento dei driver presenti nella configurazione
attuale; non è una nuova certificazione del pacchetto né la verifica della
soppressione reale dei tasti. Il precedente vincolo di lettura negata è
superato da questa autorizzazione. Al momento di quel controllo il canale driver/broker era ancora da implementare.
La nuova integrazione è descritta qui sopra; blocco, rilascio e recupero
sul dispositivo fisico restano da provare manualmente.

## Prototipo offline e verifica del candidato gratuito — 31 ottobre 2026

L'utente autorizza l'avvio dei lavori e richiede **solo soluzioni gratuite**.
Questa fase aggiunge `Source/KeyboardBlockSession.h`: una policy di sessione
senza allocazioni, separata dall'app e da qualsiasi driver. Ritorna decisioni
di cattura su eventi simulati, **non blocca tasti di Windows**. Il chiamante
futuro dovrà rimuovere il filtro reale ogni volta che la policy si disattiva;
un controller non sostituisce un watchdog del broker o del driver.

La sessione richiede backend disponibile, dispositivo univoco, conferma
attendibile dei tasti rilasciati e conferma di stop dei pad della sessione
precedente. Tiene distinti scan code normali/E0/E1, ignora le ripetizioni per
i pad e lascia passare sempre gli altri dispositivi. Disconnessione,
cambio selezione, coda piena, pacchetti invalidi, perdita backend, regressione
del clock o heartbeat assente per 1500 ms disattivano la cattura e richiedono
lo stop dei pad. Un heartbeat tardivo non riattiva la sessione.

`Tests/KeyboardTests.cpp` contiene ora **98 controlli**, inclusi i 16
precedenti. TDD: lo stub iniziale fallisce sull'attivazione dopo il rilascio;
la revisione indipendente trova poi il caso `0xFF` di overrun, osservato
fallire prima della correzione. Il controller ora disattiva la cattura su
overrun del dispositivo selezionato senza generare un tasto fantasma; lo
stesso pacchetto da un'altra tastiera non modifica la sessione. Riferimento:
[RAWKEYBOARD Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawkeyboard).
Tutte le cinque suite CTest passano dopo la correzione. Log locali in
`test-output/keyboard-block/`, incluso `overrun-red.txt` e
`full-build-final.txt`. Revisione finale: rilievo risolto, nessun altro
problema rilevato nell'ambito del prototipo.

### Compatibilità e firma: cosa è verificato

- Sistema letto: Windows Core 26H2, build 26300.9711. Tastiera salvata:
  collezione HID VID_17EF/PID_608C/MI_00. La prima lettura dettagliata dei driver
  non era stata autorizzata; il controllo successivo autorizzato, descritto sopra,
  ha confermato i driver già caricati.
- Il candidato gratuito è [Interception v1.0.1](https://github.com/oblitum/Interception/releases/tag/v1.0.1),
  il cui autore descrive l'uso non commerciale e dichiara test fino a Windows 10.
  Il pacchetto ufficiale è stato scaricato su autorizzazione, non installato.
  SHA-256: `AD038963D6413055765128B0B931F6E765147C9916DBA79E65D872B261F9AF10`.
- `Inspect-KeyboardDriver.ps1 -Archive <archivio.zip>` estrae staticamente
  le risorse PE e controlla le firme senza eseguire l'installer. Accetta solo
  l'hash dell'archivio ufficiale analizzato: una copia alterata viene rifiutata
  prima dell'estrazione. Tutti i 20 driver incorporati risultano Authenticode
  `Valid`, firmatario Francisco Lopes da Silva. Il file tastiera NT61 x64
  (`machine=0x8664`) ha hash
  `2CB5EC142CFAC879BCE4A2F9549258DB972AEBBD24F4551B6B748B464EB7DBA9`.
  Report ripetibile: `test-output/keyboard-driver-audit/report.json`.
- **Authenticode Valid non prova** ammissibilità sotto la policy kernel,
  caricamento su 26H2 o funzionamento sul dispositivo. La [policy Microsoft](https://support.microsoft.com/en-us/windows/hardware/drivers/the-windows-driver-policy)
  distingue driver WHCP e vecchie firme ammesse dalla propria lista.
  Lo stato locale di questa policy non è stato verificato.
- [OpenInputBridge](https://github.com/Applet-LLC/OpenInputBridge) dichiara
  binari WHQL ma li distribuisce a pagamento; la versione firmata è esclusa
  dal vincolo gratuito. I sorgenti gratuiti non equivalgono a binari di
  produzione firmati. Gli strumenti portatili locali non includono gli
  header kernel WDK per un driver personalizzato.

### Passi ancora necessari per il blocco reale

Verificare firma/policy di caricamento del candidato gratuito e preparare
un broker separato che riceva direttamente dal driver e consegni gli eventi
ai pad: i tasti soppressi non possono essere recuperati dall'attuale Raw Input.
Serve identificazione esplicita del dispositivo attraverso il canale driver,
senza riuso automatico di slot dopo hotplug e senza scelta ambigua per VID/PID.
Prima dell'installazione verificare anche l'ambito del pacchetto, il ripristino
e la disinstallazione. L'installazione non è stata eseguita né autorizzata
in questa fase. Restano le prove fisiche di blocco, tastiera principale,
chiusura/crash del broker e riavvio.

I due eseguibili approvati in radice sono invariati, SHA-256
`FC8276F8BA2F98788E09842B54C6581A55015B38F50164499B649F37DEDFFEE5`.
Al termine della sola fase offline non era ancora disponibile una build con blocco reale.

## Obiettivo

La tastiera selezionata deve controllare i pad senza inviare caratteri o scorciatoie alle altre applicazioni. La tastiera principale deve continuare a funzionare normalmente. Scollegamento, errore e chiusura di DualDeck devono ripristinare il normale comportamento.

## Verifica svolta

`Tests/KeyboardIsolationProbe.cpp` registra Raw Input e un hook `WH_KEYBOARD_LL` per un breve intervallo. Il programma non blocca mai un tasto. La build Windows è `build-msvc/DualDeckKeyboardIsolationProbe.exe`.

Su questo PC la prova di 1,5 secondi ha enumerato quattro collezioni tastiera. Non sono stati premuti tasti durante l'intervallo: 0 eventi Raw Input e 0 eventi hook. La precedente prova fisica della versione 0.5 ha confermato la separazione delle tastiere per l'attivazione dei pad; non ha verificato il blocco dei tasti nelle altre app.

La struttura [KBDLLHOOKSTRUCT](https://learn.microsoft.com/en-us/windows/desktop/api/winuser/ns-winuser-kbdllhookstruct) fornita all'hook include scan code e flag, ma nessun identificativo del dispositivo. Raw Input fornisce il dispositivo tramite `hDevice`, mentre `RIDEV_NOLEGACY` riguarda i messaggi destinati all'applicazione registrata, secondo la [documentazione RAWINPUTDEVICE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputdevice). Correlare i due flussi in un hook per decidere quali tasti sopprimere non offre un'identificazione affidabile al momento della decisione. Il prototipo conferma quindi il limite della soluzione attuale in modalità utente; non abilita un blocco parziale potenzialmente errato.

## Implementazione successiva

Serve un filtro installato **solo sul dispositivo scelto**, con una modalità esplicita di attivazione e una via di recupero che lasci passare i tasti quando DualDeck non è operativo. Microsoft documenta l'installazione dei [filtri specifici per dispositivo](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/installing-a-filter-driver). Il suo esempio [Kbfiltr](https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/keyboard-input-wdf-filter-driver-kbfiltr/) dimostra il punto d'intercettazione, ma è un esempio per tastiere PS/2 e richiede adattamento e test per la tastiera USB/HID effettiva. Il pacchetto va compilato con WDK e [firmato](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/driver-signing) per Windows x64.

Prima di collegarlo all'app occorre verificare su un sistema di prova: identificazione della tastiera giusta; pressione e rilascio anche nei loop; nessuna interferenza con la tastiera principale; comportamento dopo scollegamento, crash e riavvio; combinazioni di tasti di sistema; disattivazione di emergenza. Nessun driver è stato installato su questo PC.
