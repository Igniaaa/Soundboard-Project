# Blocco della tastiera dedicata: risultato del prototipo

## Obiettivo

La tastiera selezionata deve controllare i pad senza inviare caratteri o scorciatoie alle altre applicazioni. La tastiera principale deve continuare a funzionare normalmente. Scollegamento, errore e chiusura di DualDeck devono ripristinare il normale comportamento.

## Verifica svolta

`Tests/KeyboardIsolationProbe.cpp` registra Raw Input e un hook `WH_KEYBOARD_LL` per un breve intervallo. Il programma non blocca mai un tasto. La build Windows è `build-msvc/DualDeckKeyboardIsolationProbe.exe`.

Su questo PC la prova di 1,5 secondi ha enumerato quattro collezioni tastiera. Non sono stati premuti tasti durante l'intervallo: 0 eventi Raw Input e 0 eventi hook. La precedente prova fisica della versione 0.5 ha confermato la separazione delle tastiere per l'attivazione dei pad; non ha verificato il blocco dei tasti nelle altre app.

La struttura [KBDLLHOOKSTRUCT](https://learn.microsoft.com/en-us/windows/desktop/api/winuser/ns-winuser-kbdllhookstruct) fornita all'hook include scan code e flag, ma nessun identificativo del dispositivo. Raw Input fornisce il dispositivo tramite `hDevice`, mentre `RIDEV_NOLEGACY` riguarda i messaggi destinati all'applicazione registrata, secondo la [documentazione RAWINPUTDEVICE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputdevice). Correlare i due flussi in un hook per decidere quali tasti sopprimere non offre un'identificazione affidabile al momento della decisione. Il prototipo conferma quindi il limite della soluzione attuale in modalità utente; non abilita un blocco parziale potenzialmente errato.

## Implementazione successiva

Serve un filtro installato **solo sul dispositivo scelto**, con una modalità esplicita di attivazione e una via di recupero che lasci passare i tasti quando DualDeck non è operativo. Microsoft documenta l'installazione dei [filtri specifici per dispositivo](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/installing-a-filter-driver). Il suo esempio [Kbfiltr](https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/keyboard-input-wdf-filter-driver-kbfiltr/) dimostra il punto d'intercettazione, ma è un esempio per tastiere PS/2 e richiede adattamento e test per la tastiera USB/HID effettiva. Il pacchetto va compilato con WDK e [firmato](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/driver-signing) per Windows x64.

Prima di collegarlo all'app occorre verificare su un sistema di prova: identificazione della tastiera giusta; pressione e rilascio anche nei loop; nessuna interferenza con la tastiera principale; comportamento dopo scollegamento, crash e riavvio; combinazioni di tasti di sistema; disattivazione di emergenza. Nessun driver è stato installato su questo PC.
