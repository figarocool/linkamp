# Aggiungi a Linkamp — estensione Chrome/Chromium

L'estensione aggiunge il pulsante rosso **♫ Linkamp** accanto al controllo Mi piace sulle pagine video YouTube e mette in pausa l'avvio automatico. La riproduzione manuale dal player continua a funzionare. Facendo clic sul pulsante, invia il link a Linkamp tramite lo schema locale `linkamp://`.

Su ogni pagina HTTPS controlla l'indirizzo aperto e i link che terminano in `.wsz`; accanto a questi compare **Installa in Linkamp**. Sulle pagine skin di [Winamp Skin Museum](https://skins.webamp.org/) il pulsante appare vicino al download. Fai clic e conferma l'apertura dell'app: Linkamp scarica il file, lo installa nella propria cartella skin e lo applica subito. Chrome chiede accesso ai siti web perché l'estensione deve trovare i link `.wsz` anche fuori da Winamp Skin Museum.

## Installazione

1. Avvia Linkamp almeno una volta dal menu applicazioni: il protocollo `linkamp://` è registrato nel profilo utente.
2. In Chrome o Chromium apri `chrome://extensions/` e attiva **Modalità sviluppatore**.
3. Premi **Carica estensione non pacchettizzata** e seleziona questa cartella `chrome-extension`.
4. Apri una pagina video YouTube o una pagina HTTPS con un link/file `.wsz`. Al primo utilizzo, conferma che Chrome può aprire Linkamp.

## Uso

Il link e il titolo vengono aggiunti alla playlist. Linkamp riproduce solo l'audio: quando selezioni il brano, `yt-dlp` risolve lo stream e Qt lo riproduce dalla rete. Non vengono creati file audio; passando al brano successivo, Linkamp interrompe lo stream corrente.
