# Dipendenze e provenienza

AGPL-3.0-only del codice originale DualDeck non sostituisce le condizioni delle
dipendenze. Copyright originale: Alessandro Piermatteo, 2026.

## JUCE 9.0.4

Copyright (c) Raw Material Software Limited e autori delle rispettive dipendenze.
Fonte: https://github.com/juce-framework/JUCE/tree/9.0.4 . I moduli offrono
AGPLv3 oppure licenza JUCE commerciale. Questa preparazione segue AGPLv3;
non si rivendica una licenza commerciale. Testo upstream: JUCE-LICENSE.md.
Testo completo AGPL: LICENSE.

L'archivio originale JUCE incluso nell'anteprima contiene tutti i sorgenti e
gli avvisi, compreso JUCE.spdx.json, inventario delle licenze delle dipendenze
incorporate. Conservare questi avvisi. SHA-256 dell'archivio:
16AD4CFCA70B73DF6352E4E840BA5DC2E755D36B8AB2DD976EF3ECEDCD18F6A9.

## Foleys GUI Magic: prova UI opzionale

La prova opzionale della UI usa **Foleys GUI Magic**, copyright
(c) 2019-2023 Foleys Finest Audio - Daniel Walz, con licenza BSD 3-Clause.
Il modulo non è incluso nella build standard. Provenienza e riproduzione sono
in FOLEYS-UI.md; il testo completo è in Assets/Foleys-LICENSE.txt.

## Interception v1.0.1: componente esterno opzionale

Autore: Francisco Lopes da Silva (oblitum).
Fonte: https://github.com/oblitum/Interception/releases/tag/v1.0.1 .
La libreria pubblica è accompagnata da LGPLv3; il README descrive condizioni
non commerciali per il percorso gratuito e la redistribuzione degli asset
binari attraverso la libreria/API. L'accesso ai sorgenti completi di driver
e installer è descritto come parte di una licenza commerciale separata.
I driver non sono presentati come parte open source di DualDeck.

Scelta di distribuzione confermata l'8 novembre 2026: il pacchetto DualDeck
esclude DLL, driver, installer e archivi Interception. Include soltanto le
istruzioni per procurarsi i componenti dal produttore, installare separatamente
il driver e collegare la DLL. La procedura è in
[GETTING-STARTED.md](GETTING-STARTED.md) e in `keyboard-runtime/LEGGIMI.txt`
nel pacchetto Windows. Il setup standard non prepara questi componenti;
`Setup-KeyboardBridge.ps1` resta uno strumento opzionale di sviluppo locale.

Il bridge opzionale
verifica la DLL ufficiale x64 per SHA-256 e usa anche il layout interno del
contesto e DeviceIoControl SET_FILTER per ottenere conferma della scrittura
su un driver senza GET_FILTER. La condizione sull'uso esclusivo dell'API nel
README upstream è espressa nel passaggio sulla redistribuzione di driver e
installer, esclusi da questo pacchetto. La sua applicabilità all'integrazione
esterna e gli obblighi pertinenti della libreria restano oggetto di verifica:
questa nota non registra una violazione accertata né una verifica completa di
conformità. Nessun diritto commerciale sui driver è promesso. La soundboard
locale funziona senza di essi.

## Strumenti di sviluppo

CMake: https://github.com/Kitware/CMake (avvisi nel pacchetto ufficiale).
Ninja: https://github.com/ninja-build/ninja (Apache-2.0).
Python, MSVC, Windows SDK e Visual Studio/Build Tools restano strumenti esterni
con termini propri; non sono inclusi negli archivi pubblici di DualDeck.

Lo storico scripts/portable-msvc.py derivava dal gist di mmozeiko
https://gist.github.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977 . Non è stata
trovata una concessione di licenza esplicita nel materiale esaminato; il file
è stato tolto dalla versione corrente e dal pacchetto, conservando una copia
locale esclusa da Git. Non gli viene attribuita la licenza AGPL del progetto.

## Risorse

Icona e risorse originali accompagnano il codice originale. Non sono inclusi
audio personali o librerie utente, immagini o codice di SoundLord, Soundux,
SoundPad o altri prodotti usati come riferimento funzionale.
