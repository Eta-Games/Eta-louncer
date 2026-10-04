# ETA Launcher — versione nativa C++ (Qt6)

Riscrittura nativa del launcher, prima basato su Electron (HTML/JS in una
finestra Chromium). Nessuna webview, nessun Node.js: è un eseguibile C++
puro che usa Qt6 Widgets per l'interfaccia e Qt6 Network per le chiamate
HTTP (GitHub API, Firebase Auth REST).

## Compilazione

Serve Qt6 (Widgets + Network) e CMake ≥ 3.16.

```bash
# Windows (MSVC o MinGW con Qt6 installato, es. via Qt Online Installer)
cmake -B build -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.7.0/msvc2019_64"
cmake --build build --config Release
```

```bash
# Linux/macOS (sviluppo/test)
cmake -B build -S .
cmake --build build -j4
```

Il progetto qui allegato è stato compilato e avviato con successo in questo
ambiente (Qt 6.4, Ubuntu) come verifica di base prima della consegna.

### "Qt6Network.dll non è stato trovato" all'avvio su Windows

Qt6 è a librerie dinamiche: l'eseguibile da solo non basta, vanno copiate
accanto le DLL di Qt (e i plugin delle piattaforme). Dopo la build, dalla
cartella `bin` dell'installazione Qt (es. `C:\Qt\6.7.0\msvc2019_64\bin`):

```bat
windeployqt.exe --release "percorso\build\ETALauncher.exe"
```

Questo copia `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6Network.dll`,
la cartella `platforms\qwindows.dll` e il resto del necessario accanto
all'exe. Per distribuire l'app, condividi l'INTERA cartella risultante, non
solo `ETALauncher.exe`.

## La barra di progresso: come viene calcolata dall'output di `git clone`

Questo è il punto centrale della richiesta. Invece di scaricare uno zip con
`fetch()` e misurare i byte ricevuti (come faceva la versione Electron),
`GitCloner` (src/core/GitCloner.cpp) lancia `git clone --progress <repo> <cartella>`
come processo esterno e legge **stderr** in tempo reale, perché è lì che git
scrive le righe di avanzamento, aggiornando la stessa riga con `\r` finché
una fase non è completa:

```
remote: Enumerating objects: 23, done.
remote: Counting objects: 100% (23/23), done.
remote: Compressing objects: 100% (19/19), done.
remote: Total 23 (delta 3), reused 18 (delta 1), pack-reused 0 (from 0)
Receiving objects: 100% (23/23), 164.72 KiB | 1.26 MiB/s, done.
Resolving deltas: 100% (3/3), done.
```

Da ogni riga vengono estratte le percentuali con queste regex:

| Fase | Pattern |
|---|---|
| Counting objects | `Counting objects:\s*(\d+)%` |
| Compressing objects | `Compressing objects:\s*(\d+)%` |
| Receiving objects | `Receiving objects:\s*(\d+)%` |
| Resolving deltas | `Resolving deltas:\s*(\d+)%` |

"Enumerating objects" non ha una percentuale (git mostra solo un conteggio),
quindi non contribuisce al calcolo ma viene comunque mostrata come messaggio
di stato.

La percentuale **complessiva** mostrata nella progress bar è una media
pesata delle quattro fasi (pesi in `GitCloner.h`, facilmente regolabili):

```
overall = 0.05·counting + 0.05·compressing + 0.70·receiving + 0.20·resolving
```

I pesi riflettono il fatto che "Receiving objects" è di solito la fase più
lunga (è il vero e proprio download), mentre "Resolving deltas" arriva dopo
ed è tipicamente più rapida. Sono solo un punto di partenza ragionevole:
modifica `W_COUNTING` / `W_COMPRESSING` / `W_RECEIVING` / `W_RESOLVING` in
`GitCloner.h` se vuoi pesare diversamente.

`InstallProgressDialog` mostra sia la fase corrente con la sua percentuale
sia la barra complessiva, più un log testuale con le righe grezze di git
(utile per debug).

## Cosa è stato convertito 1:1 e cosa è stato semplificato

Convertiti fedelmente:
- Libreria giochi, scheda per gioco, pulsante Installa/Avvia
- Ricerca ricorsiva di `gzdoom.exe` e dei file `.wad/.pk3/.pk7/.ipk3`,
  esclusi gli IWAD noti (stessa lista di esclusione dell'originale)
- Config persistita in `config.json` (stessa cartella dati, stessa forma)
- `meta.json` per gioco (titolo, versione, motore, percorso doom2.wad, wad trovati)
- Gestione gioco: apri cartella, crea collegamento desktop, cancella
  salvataggi (`.gzd`), ripristina config (`.ini`), disinstalla
- Controllo aggiornamenti via GitHub Releases API
- I 6 temi (Night, Valter House, Matrix, Hi-Contrast, Inverted, Day),
  generati come fogli di stile Qt (QSS) dai colori bg/accent originali
- Login email/password via le REST API di Identity Toolkit (Firebase),
  stessa API key pubblica già presente nel progetto originale

Semplificati o cambiati di proposito (da verificare/estendere secondo le tue esigenze):

- **Download via `git clone` invece di zip da GitHub Releases**: il
  `CATALOG` originale puntava a un asset `.zip` di una release; qui
  `GameCatalog.h` ha un campo `repoUrl` e clona il repository. Se i file
  del gioco non stanno nella root del repo ma in una release separata,
  serve o pubblicare i file nel repo stesso oppure adattare `GameManager`
  per clonare e poi scaricare/estrarre l'asset — fammi sapere e lo aggiungo.
- **Login Google**: l'app Electron apriva una `BrowserWindow` Chromium
  embedded sulla pagina `eta-games.github.io/auth`. Qt non include un
  motore Chromium "leggero", quindi qui si apre la stessa pagina nel
  **browser di sistema predefinito**; la pagina fa comunque il suo lavoro
  e reindirizza a `etagames://auth?idToken=...`, che il sistema operativo
  consegna di nuovo a questo eseguibile (schema URI registrato in
  `HKCU\Software\Classes\etagames`, gestito in `main.cpp`/`AuthManager`).
  È lo stesso pattern usato da app native come GitHub Desktop o Docker
  Desktop per l'OAuth.
- Il titolo del launcher ha una barra personalizzata minimale
  (riduci a icona, chiudi, impostazioni) con trascinamento manuale della
  finestra; sotto c'è una navbar in stile sito con logo, "ETA Games / Studio"
  e le 4 sezioni richieste (Login, Profilo, Libreria, Negozio), ma non
  replica pixel-per-pixel l'HTML/CSS originale.
- Firestore (sincronizzazione del tema tra dispositivi) non è stato
  riportato: il tema resta locale (`config.json`), come già accadeva nel
  fallback electron-store dell'originale.
- **Font**: il sito usa Rajdhani (titoli) e Inter (corpo) da Google Fonts.
  Qt non scarica font da internet: lo stylesheet chiede "Rajdhani"/"Inter"
  per nome e ricade su "Segoe UI" se non sono installati sul PC. Per avere
  l'aspetto esatto, scarica i file `.ttf` di
  [Rajdhani](https://fonts.google.com/specimen/Rajdhani) e
  [Inter](https://fonts.google.com/specimen/Inter), mettili in
  `assets/fonts/`, e in `main.cpp` caricali con
  `QFontDatabase::addApplicationFont(...)` prima di creare `MainWindow`
  — un paio di righe, dimmelo se vuoi che te le aggiunga.

## Sezioni (Login / Profilo / Libreria / Negozio)

La navbar ha solo le 4 sezioni richieste invece delle 5 del sito (Home,
Giochi, Extra, Profilo, Blog):

- **Login** — email/password (via Firebase REST) + "Accedi con Google".
  Il bottone Google apre il browser di sistema sulla stessa pagina
  `eta-games.github.io/auth` dell'app originale: **funziona solo se quella
  pagina hosted esiste e reindirizza correttamente a `etagames://auth?...`**.
  Se non hai (ancora) pubblicato quella pagina, il bottone resterà a vuoto:
  non è un bug di questo launcher ma una dipendenza esterna mancante — fammi
  sapere se vuoi che costruiamo un flusso alternativo (es. OAuth device flow
  con client ID Google dedicato all'app desktop, senza pagina intermedia).
- **Profilo** — mostra nome/email dell'utente loggato (REST o Google), o un
  invito ad accedere se non sei autenticato.
- **Libreria** — solo i giochi già installati, con Avvia/Gestisci.
- **Negozio** — l'intero catalogo, con Installa (clona il repo via git,
  barra di progresso come descritto sopra).

## Struttura

```
src/
  main.cpp                 - avvio, istanza singola, deep link etagames://
  core/
    Config.*                - config.json (percorsi, giochi installati, tema)
    GameCatalog.h            - elenco giochi (prima CATALOG in index.html)
    GitCloner.*              - clone git + parsing progresso (vedi sopra)
    GameManager.*            - installa/avvia/rimuovi/gestisci un gioco
    UpdateChecker.*          - GitHub Releases API
    ShortcutManager.*        - collegamento .lnk sul Desktop (Windows)
    AuthManager.*            - login email/password + Google (deep link)
    ThemeManager.*           - 6 temi -> QSS
  ui/
    MainWindow.*             - finestra principale, titlebar, libreria
    LoginWidget.*            - schermata di login
    GameCardWidget.*         - card di un gioco nella libreria
    InstallProgressDialog.*  - la progress bar guidata da GitCloner
    SettingsDialog.*         - tema, cartella installazione, percorsi GZDoom/wad
```
