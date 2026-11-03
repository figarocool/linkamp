# Linkamp

Riproduttore musicale Linkamp, con interfaccia classica e supporto audio in streaming da YouTube.

Maintainer: Stefano Basile <stefanobasile@usa.com>.

**Download:** [ultimi pacchetti e plugin](https://github.com/figarocool/linkamp/releases/latest). Le release includono pacchetti `.deb` per Debian Bookworm/Trixie (amd64 e arm64), installer Windows x64 e installer macOS per Apple Silicon e Intel.

**Plugin browser:** [Chrome/Chromium](https://github.com/figarocool/linkamp/releases/latest/download/linkamp-chrome-extension.zip) · [Firefox](https://github.com/figarocool/linkamp/releases/latest/download/linkamp-firefox-extension.xpi). Se una release non è ancora disponibile, consulta i sorgenti e le istruzioni in [`chrome-extension/`](chrome-extension/README.md) e [`firefox-extension/`](firefox-extension/README.md).

**Installer:** [Windows x64](https://github.com/figarocool/linkamp/releases/latest/download/Linkamp-Setup-windows-x64.exe) · [macOS Apple Silicon](https://github.com/figarocool/linkamp/releases/latest/download/Linkamp-Installer-macos-arm64.dmg) · [macOS Intel](https://github.com/figarocool/linkamp/releases/latest/download/Linkamp-Installer-macos-x64.dmg) · [Linux `.deb`](https://github.com/figarocool/linkamp/releases/latest)

## Cosa fa Linkamp

- Riproduce file audio locali e streaming audio da link YouTube, mantenendo una playlist persistente.
- Offre un'interfaccia compatta ispirata a Winamp 2 Classic, con supporto alle skin classiche `.wsz`.
- Include equalizzatore, visualizzatore audio, sorgenti Bluetooth e Spotify, e riproduzione CD nativa su Linux.
- Le estensioni Chrome/Chromium e Firefox aggiungono il video YouTube corrente alla playlist tramite `linkamp://` e impediscono l'autoplay iniziale; sulle pagine HTTPS rilevano anche gli indirizzi e i link `.wsz` per installare e applicare direttamente le skin.
- Dal menu contestuale del player si può collegare la playlist a Google Drive o OneDrive con l'accesso nel browser. Linkamp salva la playlist nella cartella privata dell'app nel cloud e controlla gli aggiornamenti ogni 15 secondi, senza richiedere i client desktop o cartelle montate. Google richiede l'OAuth Client ID e il client secret dell'app; il secret si configura tramite `LINKAMP_GOOGLE_OAUTH_CLIENT_SECRET` o le impostazioni utente e non va salvato nel repository. I percorsi dei file musicali locali devono esistere anche sull'altro computer, mentre i link YouTube vengono condivisi direttamente.

Le estensioni Firefox pubblicate come `.xpi` in questo progetto non sono firmate da Mozilla: il caricamento è temporaneo da `about:debugging`. Per l'installazione permanente occorre la firma Mozilla Add-ons. L'estensione Chrome/Chromium si carica come pacchetto non pubblicato dalla pagina `chrome://extensions/` in modalità sviluppatore.

## Requirements

- Qt 6 with Qt Multimedia
- taglib 1.11.1 or newer
- libarchive (for Winamp Classic `.wsz` skins)
- libasound2-dev
- libpipewire-0.3-dev
- libcdio-dev
- libcdio-paranoia-dev
- libcdio-utils
- libdiscid-dev
- python3-pip
- python3-full
- python3-dev
- python3-dbus-next
- yt-dlp (installed in the application virtual environment)
- Node.js (used by yt-dlp for YouTube's JavaScript challenge support)

## Development

### Setup

The setup steps depend on your operating system. Linkamp is developed and tested on Debian Trixie, which is close to the target Raspberry Pi environment (DietPi based on Debian Trixie).

The instructions here are for Debian Trixie:

```bash
# Install Qt, Qt Multimedia and Qt Creator
sudo apt-get install build-essential qt6-base-dev qt6-base-dev-tools qt6-multimedia-dev qtcreator cmake -y

# Install third party library dependencies
sudo apt-get install libtag1-dev libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev -y
sudo apt-get install libarchive-dev -y

# Install native CD player dependencies
sudo apt-get install libcdio-dev libcdio-paranoia-dev libcdio-utils libdiscid-dev -y

# Create Python venv and install Python dependencies (for BT and Spotify sources)
## IMPORTANT: Make sure you are on the folder where you cloned this repo before running the following commands:
python3 -m venv venv
source venv/bin/activate
pip install -r python/requirements.txt
```

### Build and run

**Using Qt Creator:**

1. Open this project with Qt Creator (open CMakeLists.txt in Qt Creator)
2. Qt Creator should guide you setting up your paths and settings for building the proyect in your machine
3. You should be able to click the green "Play" icon in Qt Creator to build and run the app

**From the console:**

```bash
# From inside this repository:
## Generate build configuration, you only need to run this once:
cmake CMakeLists.txt
## Setup python dependencies, you only need to run this once:
./setup.sh

## Compile the project
make

## Run the app
./start.sh
```

**Local desktop build:** this installation uses the classic desktop layout: a 275×116 player window and a separate, compact playlist editor. The file picker opens as a separate system dialog.

### Winamp Classic skins

Linkamp can load Winamp 2.x Classic skins packaged as `.wsz` (or ZIP) files, or from an extracted skin folder. Open the player menu and use **Skins** to choose an installed skin, load a `.wsz` file or extracted folder, or restore **Classic (Linkamp default)**. The built-in Classic appearance is used on first launch, and the selected skin is saved between launches. Modern Winamp `.wal` skins are not supported.

## YouTube streaming (local desktop integration)

This local build accepts YouTube URLs from the command line or the playlist's **ADD** menu:

```bash
./start.sh 'https://youtu.be/S_gJpgFa6m0'
```

Linkamp legge il titolo e aggiunge l'URL della pagina alla playlist. `yt-dlp` risolve l'audio solo quando selezioni il brano, poi lo riproduce in streaming. L'audio viene bufferizzato per la riproduzione, senza salvare MP3 o altri file. Selezionare il brano successivo interrompe lo stream corrente. Le estensioni browser mettono in pausa l'autoplay di YouTube, lasciando disponibile il pulsante Play del sito.

Le estensioni sono in [`chrome-extension/`](chrome-extension/README.md) per Chrome/Chromium e [`firefox-extension/`](firefox-extension/README.md) per Firefox. Entrambe aggiungono **♫ Linkamp** accanto a Mi piace e sulle pagine skin di Winamp Skin Museum, usando lo schema locale `linkamp://`.

### Building a Debian package

On native Debian Trixie, you can build a package matching the CI `trixie` job without Docker:

```bash
./build-deb.sh
```

The script installs Debian packaging tools (`debhelper`, `devscripts`, `dh-python`, `dh-cmake`, and related packages), copies the tree to a local temp directory (so your git tree stays clean), runs `apt-get build-dep` for `debian/control`, builds with `dpkg-buildpackage`, and places the `.deb` in `~/linkamp-debs/`.

You can install the newly built packages with the following command (replace placeholders accordingly):

```bash
sudo apt install ./linkamp_[version]_[arch].deb
```

### Aggiornamenti

Linkamp controlla le nuove release una volta al giorno. Su Windows e macOS il manutentore incluso nell'installer scarica e applica l'aggiornamento. Su Debian installato dal pacchetto ufficiale, il player scarica il `.deb`, ne verifica l'hash SHA-256 e chiede l'autorizzazione amministrativa prima dell'installazione.

### Sistemi operativi supportati

Le release forniscono build per Linux, Windows x64 e macOS (Intel e Apple Silicon). La riproduzione di file e link YouTube è disponibile su tutte le piattaforme. CD, Bluetooth e Spotify, equalizzatore di sistema e analizzatore audio richiedono integrazioni Linux e sono disponibili solo nella build Linux.

---

## English

Linkamp is a compact Qt 6 music player inspired by Winamp 2 Classic. It plays local audio files and streams audio from YouTube links without saving audio files, and it keeps the playlist between launches. It also includes classic `.wsz` skin support, an equalizer, an audio visualizer, Bluetooth and Spotify sources, and native CD playback on Linux.

From the player's context menu, connect a playlist to Google Drive or OneDrive through browser sign-in. Linkamp stores it in the app's private cloud folder and checks for updates every 15 seconds, without requiring desktop sync clients or mounted folders. Google requires the app's OAuth client ID and client secret; configure the secret with `LINKAMP_GOOGLE_OAUTH_CLIENT_SECRET` or in user settings, and never commit it to the repository. Local audio files must also be available on the other computer, while YouTube links are shared directly.

To configure OAuth client IDs when building Linkamp, pass `-DLINKAMP_GOOGLE_OAUTH_CLIENT_ID=...` and `-DLINKAMP_ONEDRIVE_OAUTH_CLIENT_ID=...` to CMake. Register Linkamp as a desktop app with Google and a public desktop app with Microsoft Entra; the app uses the system browser and PKCE.

### Downloads and browser extensions

- [Latest Linux packages and extension downloads](https://github.com/figarocool/linkamp/releases/latest)
- [Windows x64 installer](https://github.com/figarocool/linkamp/releases/latest/download/Linkamp-Setup-windows-x64.exe)
- [macOS Apple Silicon installer](https://github.com/figarocool/linkamp/releases/latest/download/Linkamp-Installer-macos-arm64.dmg)
- [macOS Intel installer](https://github.com/figarocool/linkamp/releases/latest/download/Linkamp-Installer-macos-x64.dmg)
- [Chrome/Chromium extension ZIP](https://github.com/figarocool/linkamp/releases/latest/download/linkamp-chrome-extension.zip)
- [Firefox extension XPI](https://github.com/figarocool/linkamp/releases/latest/download/linkamp-firefox-extension.xpi)
- [Chrome/Chromium source and installation guide](chrome-extension/README.md)
- [Firefox source and installation guide](firefox-extension/README.md)

The extensions add the open YouTube video to Linkamp through the `linkamp://` protocol and pause YouTube's initial autoplay. On HTTPS pages they also detect `.wsz` URLs and links, offering one-click skin installation and application. The Chrome/Chromium extension is sideloaded in developer mode. The Firefox XPI published here is unsigned and can only be loaded temporarily through `about:debugging`; permanent installation requires signing through Mozilla Add-ons.

### Supported platforms

Release builds are provided for Linux, Windows x64, and macOS (Intel and Apple Silicon). Local file and YouTube playback are available on all platforms. CD, Bluetooth and Spotify sources, system equalizer, and audio visualizer use Linux-specific integrations and are available only in the Linux build.

### Updates

Linkamp checks for new releases once a day. On Windows and macOS, the maintenance tool included with the installer downloads and applies updates. On Debian installations from the official package, Linkamp downloads the matching `.deb`, verifies its SHA-256 digest, and requests administrator authorization before installation.

### Build from source

See the [Italian development and Debian packaging instructions above](#development) and the dependency list at the beginning of this README. Browser extension setup instructions are available in the Chrome/Chromium and Firefox folders.

### Known issues

- In order for the Python integration (BT and Spotify sources) to work and not crash, you need to run the app inside the Python venv. Use the "start.sh" script as a helper to set everything up for you and run it. However, because Qt Creator by default will directly run the player executable outside of the venv, those sources will crash. I'm yet to find a better way of running it inside Qt Creator, other than telling it to run the "start.sh" script instead of the executable.

### Debugging memory leaks:

- Install valgrind
- In Qt Creator, in the menu bar, click Analyze -> Valgrind Memory Analizer
- Wait for the app to start (it will be slow), use it and close it, you will get the results then.

### Links that I've found useful and/or got inspiration from:

- https://taglib.org/
- https://github.com/Znurre/QtMixer/blob/b222c49c8f202981e5104bd65c8bf49e73b229c1/QAudioDecoderStream.cpp#L153
- https://github.com/audacious-media-player/audacious/blob/master/src/libaudcore/visualization.cc#L110
- https://github.com/audacious-media-player/audacious/blob/master/src/libaudcore/fft.cc
- https://github.com/qt/qtmultimedia/blob/4fafcd6d2c164472ce63d5f09614b7e073c74bea/src/spatialaudio/qaudioengine.cpp
- https://github.com/audacious-media-player/audacious-plugins/blob/master/src/qt-spectrum/qt-spectrum.cc
- https://github.com/captbaritone/webamp/blob/master/packages/webamp/js/components/Visualizer.tsx
