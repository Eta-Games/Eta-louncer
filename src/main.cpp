#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSettings>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include "ui/MainWindow.h"
#include "core/Autostart.h"

static const char* SINGLETON_KEY = "ETALauncherCpp-Singleton";

// Su Windows, registra etagames:// in HKCU in modo che il browser possa
// riaprire questo eseguibile passando l'URL di ritorno del login Google
// (stesso meccanismo di app.setAsDefaultProtocolClient('etagames') in Electron).
static void registerProtocolHandlerWindows() {
#ifdef Q_OS_WIN
    QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    QSettings root("HKEY_CURRENT_USER\\Software\\Classes\\etagames", QSettings::NativeFormat);
    root.setValue("Default", "URL:ETA Games Protocol");
    root.setValue("URL Protocol", "");
    QSettings cmd("HKEY_CURRENT_USER\\Software\\Classes\\etagames\\shell\\open\\command", QSettings::NativeFormat);
    cmd.setValue("Default", QString("\"%1\" \"%2\"").arg(exe, "%1"));
#endif
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("ETA Games Launcher");
    app.setOrganizationName("ETA-Games");
    app.setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon(":/logo.png"));

    // Font del sito, incorporati nell'exe (non servono installati sul PC)
    for (const char* f : {"Rajdhani-Bold", "Rajdhani-SemiBold", "Inter-Regular", "Inter-SemiBold"})
        QFontDatabase::addApplicationFont(QString(":/fonts/%1.ttf").arg(f));
    QFont base("Inter", 10);
    base.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(base);

    registerProtocolHandlerWindows();
    if (Autostart::isEnabled()) Autostart::set(true);   // aggiorna il percorso se l'exe è stato spostato

    // ── Istanza singola: se c'è già un'istanza in esecuzione, inoltriamo
    //    gli argomenti (deep link etagames://... oppure --launch <id>) e usciamo.
    QLocalSocket probe;
    probe.connectToServer(SINGLETON_KEY);
    if (probe.waitForConnected(150)) {
        QStringList args = app.arguments();
        args.removeFirst();
        probe.write(args.join('\n').toUtf8());
        probe.flush();
        probe.waitForBytesWritten(200);
        return 0;
    }

    QLocalServer server;
    QLocalServer::removeServer(SINGLETON_KEY); // pulisce socket residui da crash precedenti
    server.listen(SINGLETON_KEY);

    MainWindow window;

    QObject::connect(&server, &QLocalServer::newConnection, [&]() {
        QLocalSocket* sock = server.nextPendingConnection();
        QObject::connect(sock, &QLocalSocket::readyRead, [&, sock]() {
            QString payload = QString::fromUtf8(sock->readAll());
            for (const QString& arg : payload.split('\n')) {
                if (arg.startsWith("etagames://")) window.handleDeepLink(arg);
                else if (arg == "--launch") { /* il prossimo arg è l'id, gestito sotto */ }
            }
            int idx = payload.split('\n').indexOf("--launch");
            if (idx >= 0 && idx + 1 < payload.split('\n').size())
                window.launchDirectly(payload.split('\n').at(idx + 1));
            window.showFromTray();
        });
    });

    // Argomenti passati a QUESTA prima istanza (avvio da link etagames:// o da shortcut --launch)
    QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i].startsWith("etagames://")) window.handleDeepLink(args[i]);
        else if (args[i] == "--launch" && i + 1 < args.size()) window.launchDirectly(args[i + 1]);
    }

    if (app.arguments().contains("--background")) window.startInBackground();   // avvio con Windows: nascosto nella tray
    else window.show();
    return app.exec();
}
