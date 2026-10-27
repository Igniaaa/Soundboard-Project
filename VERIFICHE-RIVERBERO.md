# Riverbero diffuso e smorzato — 27 ottobre 2026

L'utente segnala gracchiare anche con il solo riverbero. La rete precedente
aveva quattro comb a37/53/71/89ms, senza diffusione o smorzamento.
Il probe a impulso rileva una coda molto rada. Nel confronto offline su
Bonk con mix32% il picco0,39865 rimane sotto la soglia0,98: il limiter
non distorce il caso riprodotto, che non esaurisce ogni possibile materiale.

La struttura a ritardi senza diffusione è nota per produrre flutter sui
transienti: [Schroeder e Logan, Colorless Artificial Reverberation](https://aes.org/publications/elibrary-page/?id=465).
È stato confrontato anche il sorgente locale `juce_Reverb.h`. Non viene
copiata l'approssimazione allpass di JUCE/Freeverb: questa revisione usa
quattro sezioni allpass vere sulla sola uscita wet.

## Modifica approvata

- Smorzamento passa basso nel feedback, polo tarato a6kHz. Il diretto e la
  prima riflessione non passano attraverso questo filtro.
- Allpass in serie a5,1/7,7/10,3/12,7ms, guadagno0,65; ogni sezione usa
  `y=d-g*x; memoria=x+g*y`, distribuendo l'energia nel tempo.
- Diretto pieno e mix additivo; stesso limiter. Nessun nuovo controllo,
  campo JSON, lookahead o ritardo del diretto. Le prime riflessioni wet
  possono essere meno evidenti perché l'energia viene distribuita.
- Buffer stereo allocati solo in prepare, con epoch per ciascuna voce;
  filtri e cursori scalari azzerati al trigger. Il callback non alloca,
  non usa lock e non azzera buffer estesi.
- Code prolungate per drenare i diffusori. Feedback inferiore a1e-20
  azzerato per evitare subnormal inutili. Durata continua a controllare il
  decadimento dei comb alle basse frequenze; gli acuti si spengono prima.

## TDD e risultati

`Tests/ReverbTests.cpp` falliva8 controlli su32 prima della modifica:
densità e smorzamento a44,1/48/96/192kHz. Ora passa tutti i32 controlli.
La densità è la frazione di campioni sopra1e-4 del picco della finestra
(-80dB), non una metrica percettiva normalizzata di echo density.
Tra200–500ms passa da0,15–0,17% a99,64–99,80%.

Con burst a600Hz e10kHz, il rapporto di potenza600–900ms rispetto a
100–200ms rimane circa0,012 alle basse e scende sotto0,0001 alle alte.
Verificati anche attacco, mix0, durata breve/lunga, wet stereo non nullo,
livelli finiti/protetti e riuso silenzioso di una voce già usata.

`Build.ps1` completato, CTest5/5: audio_behaviour520 controlli,
scrub_time_stretch192, audio_quality27, reverb_quality32, keyboard_filter16.
I vecchi test che richiedevano esattamente quattro riflessioni isolate
sono sostituiti da verifiche di coda e decadimento: la coda rada era il
difetto da correggere.

## Confronto offline su Bonk

File decodificato con FFmpeg a48kHz stereo, durata0,504s, riverbero32%,
durata2s e fade-out0 per il confronto; esportato il canale sinistro con3s
di coda. Il probe non scrive il file originale né la libreria.

| Misura | Prima | Dopo |
|---|---:|---:|
| Picco a volume100% |0,398650|0,402367|
| RMS wet a volume10%, su tutta l'esportazione |0,000426767|0,000426066|
| Errore di linearità tra10% e100% |2,98e-8|2,98e-8|
| Densità impulso100–500ms |0,00151042|0,956927|
| Densità impulso500–1000ms |0,00150000|0,998792|

Il livello wet complessivo rimane quasi invariato in questo esempio:
la correzione distribuisce l'energia senza abbassare tutto l'audio.
Il blocco128 campioni più lento nel run locale richiede0,061ms con Bonk
e0,184ms con impulso: una voce, senza dispositivi audio. Non certifica
il rispetto delle scadenze con32 voci su dispositivi reali.

Audio: `test-output/reverb-bonk-before.wav` e `reverb-bonk-after.wav`.
Probe e report: `.superpowers/sdd/2026-10-27-reverb-texture/`.
Baseline: `backups/reverb-diffusion-before-2026-10-27/AudioCore.h`.

## Build e limiti

Aggiornati i tre eseguibili locali, compreso `DualDeckJuce-0.7.exe`;
SHA-256 identico alla build verificata:
`AD9826D379EE6A314A388846641DD067FAA30EC180B3E06D5BD1C22770298670`.

Revisione indipendente: nessun problema critico o importante; rieseguiti
i32 test. Miglioramenti minori rinviati: caso stereo con ampiezze
esattamente opposte (il test usa0,5/-0,35) e controllo temporale più stretto
del drenaggio dei diffusori. Budget delle code e riuso per epoch sono
stati verificati per lettura. Nessun commit o push eseguito.

Non è stata eseguita una prova di ascolto fisico: densità e smorzamento
non dimostrano da soli che il gracchiare percepito sia eliminato. Resta
la conferma sugli stessi suoni e uscite dell'utente.
