# Aggiungi a Linkamp — estensione Chrome/Chromium

L'estensione aggiunge il pulsante rosso **♫ Linkamp** accanto al controllo Mi piace sulle pagine video YouTube. Facendo clic, invia il link a Linkamp tramite lo schema locale `linkamp://`. Se hai già caricato l'estensione, premi **Ricarica** nella scheda `chrome://extensions/` dopo un aggiornamento.

## Installazione

1. Avvia Linkamp almeno una volta dal menu applicazioni: il protocollo `linkamp://` è registrato nel profilo utente.
2. In Chrome o Chromium apri `chrome://extensions/` e attiva **Modalità sviluppatore**.
3. Premi **Carica estensione non pacchettizzata** e seleziona questa cartella `chrome-extension`.
4. Riapri una pagina video YouTube. Al primo utilizzo del pulsante, conferma che Chrome può aprire Linkamp.

## Uso

Il link e il titolo vengono aggiunti alla playlist. Linkamp riproduce solo l'audio: quando selezioni il brano, `yt-dlp` risolve lo stream e Qt lo riproduce dalla rete. Non vengono creati file audio; passando al brano successivo, Linkamp interrompe lo stream corrente.
