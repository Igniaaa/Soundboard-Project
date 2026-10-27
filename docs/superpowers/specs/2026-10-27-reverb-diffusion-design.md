# Coda del riverbero diffusa e smorzata

## Problema e prova

L'utente riferisce gracchiare anche con il solo riverbero. La rete attuale
somma quattro linee di ritardo con feedback, senza diffusione o smorzamento.
Il probe a impulso misura densità dei campioni significativi pari a circa
0,00151 tra100–500ms e0,00150 tra500–1000ms. Sul file locale Bonk, al32% di
mix, il picco a volume100% è0,39865 e il confronto di livello è lineare:
il limiter non è la causa del difetto in questo caso riprodotto.
Questo documenta una coda troppo rada, non prova da solo l'identità
percettiva con il gracchiare riferito dall'utente.

## Approccio proposto

Estendere la rete esistente con quattro sezioni allpass in serie sulla
sola uscita wet e smorzamento passa basso nel feedback delle quattro linee.
Gli allpass distribuiscono l'energia nel tempo senza una riduzione fissa
del diretto. Le linee mantengono il decadimento alle basse frequenze già
legato al controllo Durata; gli acuti si spengono più rapidamente.

L'alternativa di sostituire tutto con juce::Reverb cambierebbe la relazione
tra Durata e decadimento (in particolare a0,2s) e richiederebbe una diversa
gestione dello stato per pad. Per questo intervento è preferita l'estensione
limitata della rete esistente, secondo la struttura classica di Schroeder.

## Vincoli

Diretto a guadagno unitario, mix additivo, stesso limiter e nessun nuovo
controllo o schema dati. Buffer allocati solo in prepare; riuso delle voci
con epoch per evitare residui di altri pad; nessuna allocazione o lock nel
callback. Entrambi i canali rimangono elaborati senza sommarli in mono.
Nessun cambiamento a delay, scrub o file audio originali.

## Accettazione

Prima test fallito sulla coda: almeno metà dei campioni tra200–500ms deve
contenere energia significativa, a44,1/48/96/192kHz. Poi verificare diretto
invariato, zero mix, decadimento breve/lungo, smorzamento degli acuti,
stereo, riuso della voce, livelli finiti e picchi protetti. Eseguire tutte
le suite CTest, confronto offline su Bonk, build Release e revisione
indipendente. Una misura di densità non certifica naturalezza: conferma
d'ascolto dell'utente necessaria.

Stato: disegno approvato esplicitamente dall'utente; autorizzate correzione
e verifiche nella sessione corrente.
