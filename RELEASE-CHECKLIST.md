# Requisiti per DualDeck 1.0

Stato: **preparazione 1.0.0-rc1; non è una 1.0 stabile**.
Una casella senza evidenza non è una prova superata.

## Preparazione automatizzabile

- [x] Copyright indicato dall'utente e percorso AGPL-3.0-only documentati.
- [x] Testo AGPL completo, avvisi JUCE e dipendenze esterne identificati.
- [x] VERSION unica e metadati della candidata; copie 0.7 approvate conservate.
- [x] Setup standard senza bootstrap di terzi con licenza non chiarita.
- [x] Packaging con allowlist, hash JUCE e sorgenti inclusi.
- [x] Workflow Windows predisposto in sola lettura e senza pubblicazione.
- [x] Build da snapshot pulito e prove sintetiche: vedere VERIFICA-RELEASE.md.
- [ ] Esecuzione effettiva del workflow su GitHub dopo commit/push.

## Tastiera: prove fisiche aperte

Registrare versione Windows, modello dei dispositivi e risultato per ogni caso.
Conservare a disposizione mouse e tastiera principale.

| Prova | Evidenza |
| --- | --- |
| Dedicata comanda pad senza scrivere; principale scrive | Conferma storica utente; ripetere sulla candidata |
| Disattivazione e chiusura normale ripristinano la scrittura | Da provare sulla candidata |
| Scollegamento/ricollegamento non selezionano un'altra tastiera | Da provare |
| Arresto inatteso GUI rilascia la tastiera | Da provare |
| Arresto inatteso broker rilascia la tastiera | Da provare |
| Riavvio PC e nuova attivazione manuale | Da provare |
| DLL/driver assente: errore comprensibile | Avvio sintetico senza runtime; prova utente aperta |

Se la GUI si blocca, il broker può rilasciare la tastiera autonomamente; lo stop
dei suoni avviati attende ancora la ripresa della GUI. Non dichiarare garanzie
più ampie delle prove effettive.

## Audio, dati e compatibilità

- [ ] Sessione di almeno 60 minuti con Discord, microfono e cuffie: registrare
  CPU, RAM, buffer, campionamento e anomalie udibili.
- [ ] Più pad, effetti alti, spam, loop, scrub e streaming prolungato.
- [ ] Scollegamento/riconnessione uscite e scelta di una nuova uscita.
- [ ] Aggiornamento reale da 0.7 con backup: profili, tasti e impostazioni.
- [ ] Recupero reale da .bak e cartelle/file mancanti senza perdita dati.
- [ ] Primo avvio da account Windows senza toolchain e senza Interception.
- [ ] Almeno un secondo PC x64; documentare le versioni Windows provate.
- [ ] Navigazione fisica da tastiera e lettore di schermo.

## Prima della pubblicazione

- [ ] Risolvere Interception SET_FILTER (THIRD-PARTY-NOTICES.md), oppure
  escludere quell'integrazione dalla build pubblica.
- [ ] Verificare eventuali contributi di terzi e conservare tutti gli avvisi.
- [ ] Controllare archivi, sorgenti corrispondenti e SHA256SUMS.txt.
- [ ] Dichiarare lo stato della firma digitale; gli hash non sono una firma.
- [ ] Provare la candidata e registrare l'accettazione.
- [ ] Solo dopo le prove: adeguare la procedura che oggi accetta solo versioni
  rc, impostare 1.0.0 stabile, creare tag e release con sorgenti esatti.

La preparazione non esegue automaticamente push, tag, GitHub Release,
installazioni driver o prove di crash sul computer dell'utente.
