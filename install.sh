#!/usr/bin/env bash
# ETA Launcher - installer per Linux (solo cartella utente, nessun root richiesto)
#
# Uso:
#   ./install.sh              installa / aggiorna
#   ./install.sh --uninstall  rimuove tutto
#
# Se "ETALauncher" e' accanto allo script lo usa, altrimenti lo scarica
# dal branch "linux" del repository.

set -euo pipefail

APP_NAME="ETA Launcher"
APP_ID="etalauncher"
BIN_NAME="ETALauncher"
REPO="Eta-Games/Eta-louncer"
BIN_URL="https://github.com/$REPO/raw/linux/$BIN_NAME"
ICON_URL="https://github.com/$REPO/raw/main/assets/logo.png"

DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
INSTALL_DIR="$DATA_HOME/$APP_ID"
BIN_DIR="$HOME/.local/bin"
APPS_DIR="$DATA_HOME/applications"
ICON_DIR="$DATA_HOME/icons/hicolor/256x256/apps"

say()  { printf '\033[1;36m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[!]\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m[x]\033[0m %s\n' "$*" >&2; exit 1; }

uninstall() {
    say "Rimozione di $APP_NAME..."
    rm -rf "$INSTALL_DIR"
    rm -f "$BIN_DIR/$APP_ID" \
          "$APPS_DIR/$APP_ID.desktop" \
          "$ICON_DIR/$APP_ID.png"
    update-desktop-database "$APPS_DIR" 2>/dev/null || true
    say "Disinstallato. (I dati utente del launcher, se presenti, non sono stati toccati.)"
    exit 0
}

[ "${1:-}" = "--uninstall" ] && uninstall
[ "$(id -u)" -eq 0 ] && die "Non eseguire come root: l'installazione e' per l'utente corrente."
[ "$(uname -m)" = "x86_64" ] || die "Il binario e' compilato per x86_64, questa macchina e' $(uname -m)."

fetch() { # fetch URL DEST
    if command -v curl >/dev/null 2>&1; then
        curl -fL --progress-bar -o "$2" "$1"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$2" "$1"
    else
        die "Serve curl o wget per scaricare i file."
    fi
}

cd "$(dirname "$(readlink -f "$0")")"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# --- binario ---------------------------------------------------------------
if [ -f "$BIN_NAME" ]; then
    say "Uso $BIN_NAME trovato accanto allo script."
    BIN_SRC="$PWD/$BIN_NAME"
else
    say "Scarico $BIN_NAME dal branch linux..."
    fetch "$BIN_URL" "$TMP/$BIN_NAME" || die "Download del binario non riuscito."
    BIN_SRC="$TMP/$BIN_NAME"
fi
head -c 4 "$BIN_SRC" | grep -q 'ELF' || die "Il file scaricato non e' un eseguibile Linux valido."

# --- icona (opzionale) -----------------------------------------------------
ICON_SRC=""
if [ -f "logo.png" ]; then
    ICON_SRC="$PWD/logo.png"
else
    say "Scarico l'icona..."
    fetch "$ICON_URL" "$TMP/logo.png" 2>/dev/null && ICON_SRC="$TMP/logo.png" \
        || warn "Icona non scaricata, continuo senza."
fi

# --- installazione ---------------------------------------------------------
say "Installo in $INSTALL_DIR"
mkdir -p "$INSTALL_DIR" "$BIN_DIR" "$APPS_DIR" "$ICON_DIR"
install -m 755 "$BIN_SRC" "$INSTALL_DIR/$BIN_NAME"
[ -n "$ICON_SRC" ] && install -m 644 "$ICON_SRC" "$ICON_DIR/$APP_ID.png"

# Wrapper: usa il Qt di /opt/Qt se presente (il binario e' stato compilato li')
cat > "$INSTALL_DIR/$APP_ID.sh" <<'EOF'
#!/usr/bin/env bash
DIR="$(dirname "$(readlink -f "$0")")"
for QT in /opt/Qt/6.12.0/gcc_64 "$HOME/Qt/6.12.0/gcc_64"; do
    if [ -d "$QT/lib" ]; then
        export LD_LIBRARY_PATH="$QT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        export QT_PLUGIN_PATH="$QT/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
        break
    fi
done
exec "$DIR/ETALauncher" "$@"
EOF
chmod 755 "$INSTALL_DIR/$APP_ID.sh"
ln -sf "$INSTALL_DIR/$APP_ID.sh" "$BIN_DIR/$APP_ID"

cat > "$APPS_DIR/$APP_ID.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=$APP_NAME
Comment=Launcher dei giochi ETA Games
Exec=$INSTALL_DIR/$APP_ID.sh
Icon=$APP_ID
Categories=Game;
Terminal=false
StartupWMClass=$BIN_NAME
EOF
chmod 644 "$APPS_DIR/$APP_ID.desktop"

update-desktop-database "$APPS_DIR" 2>/dev/null || true
gtk-update-icon-cache -f -t "$DATA_HOME/icons/hicolor" 2>/dev/null || true

# --- controlli dipendenze --------------------------------------------------
MISSING="$(LD_LIBRARY_PATH="$(for q in /opt/Qt/6.12.0/gcc_64 "$HOME/Qt/6.12.0/gcc_64"; do [ -d "$q/lib" ] && printf '%s/lib:' "$q"; done)${LD_LIBRARY_PATH:-}" \
           ldd "$INSTALL_DIR/$BIN_NAME" 2>/dev/null | awk '/not found/ {print $1}' || true)"

if [ -n "$MISSING" ]; then
    warn "Mancano queste librerie, il launcher non partira' finche' non le installi:"
    printf '      %s\n' $MISSING >&2
    echo >&2
    echo "    Il binario richiede Qt 6.12. Opzioni:" >&2
    echo "      - installa Qt 6.12 con l'installer ufficiale in /opt/Qt o ~/Qt (viene rilevato da solo)" >&2
    if command -v apt >/dev/null 2>&1; then
        echo "      - oppure, se la tua distro ha Qt >= 6.12: sudo apt install libqt6widgets6 libqt6network6 libqt6gui6" >&2
    elif command -v pacman >/dev/null 2>&1; then
        echo "      - oppure: sudo pacman -S qt6-base" >&2
    elif command -v dnf >/dev/null 2>&1; then
        echo "      - oppure: sudo dnf install qt6-qtbase-gui" >&2
    fi
    echo >&2
fi

command -v git >/dev/null 2>&1 || warn "git non trovato: il launcher lo usa per installare i giochi (installalo con il gestore pacchetti)."

say "Fatto! Cerca '$APP_NAME' nel menu applicazioni, oppure lancia: $APP_ID"
case ":$PATH:" in
    *":$BIN_DIR:"*) ;;
    *) warn "$BIN_DIR non e' nel PATH: usa il menu oppure aggiungilo al tuo .bashrc." ;;
esac
