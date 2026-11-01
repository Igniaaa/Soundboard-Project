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

## Interception v1.0.1: componente esterno opzionale

Autore: Francisco Lopes da Silva (oblitum).
Fonte: https://github.com/oblitum/Interception/releases/tag/v1.0.1 .
La libreria pubblica è accompagnata da LGPLv3; il README descrive condizioni
non commerciali per il percorso gratuito e la redistribuzione degli asset
binari attraverso la libreria/API. L'accesso ai sorgenti completi di driver
e installer è descritto come parte di una licenza commerciale separata.
I driver non sono presentati come parte open source di DualDeck.

L'anteprima esclude DLL, driver e installer Interception. Il bridge opzionale
verifica la DLL ufficiale x64 per SHA-256 e usa anche il layout interno del
contesto e DeviceIoControl SET_FILTER per ottenere conferma della scrittura
su un driver senza GET_FILTER. Tenere la dipendenza esterna **non risolve da
solo** l'ammissibilità di questa integrazione: prima della release stabile
vanno chiariti i termini o adeguata l'implementazione. Nessun diritto
commerciale sui driver è promesso. La soundboard locale funziona senza di essi.

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
