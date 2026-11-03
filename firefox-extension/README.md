# Aggiungi a Linkamp — estensione Firefox

L'estensione aggiunge **♫ Linkamp** accanto al controllo Mi piace nelle pagine video YouTube, mette in pausa l'avvio automatico e invia il link alla playlist tramite `linkamp://`. La riproduzione manuale dal player resta disponibile.

Su ogni pagina HTTPS controlla l'indirizzo e i link `.wsz`, mostrando **Installa in Linkamp** quando ne trova uno. Su [Winamp Skin Museum](https://skins.webamp.org/) il pulsante appare accanto al download. Facendo clic e autorizzando l'apertura dell'app, Linkamp scarica la skin, la installa e la applica subito. Firefox richiede l'autorizzazione ai siti web per permettere la ricerca dei link `.wsz` su domini diversi.

## Caricamento in Firefox

1. Apri `about:debugging#/runtime/this-firefox`.
2. Premi **Carica componente aggiuntivo temporaneo…**.
3. Seleziona `manifest.json` in questa cartella.
4. Apri una pagina video YouTube o una pagina HTTPS con un link/file `.wsz`. Al primo utilizzo, autorizza Firefox ad aprire Linkamp.

Il caricamento temporaneo dura fino alla chiusura di Firefox. Per installarlo stabilmente, l'estensione deve essere firmata da Mozilla Add-ons.
