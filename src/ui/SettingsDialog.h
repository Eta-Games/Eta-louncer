#pragma once
#include <QDialog>
#include <functional>

class QVBoxLayout;
class QStackedWidget;
class QButtonGroup;
class QLineEdit;

// Impostazioni del launcher: barra laterale + pagine (stile sezioni "Account" del sito).
// Le pagine di profilo/sicurezza si aggiungono con addPage() nella stessa barra.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

signals:
    void themeChanged(const QString& themeId);
    void restartRequested();   // l'utente ha cambiato lingua e vuole riavviare subito

private:
    QStackedWidget* m_stack = nullptr;
    QVBoxLayout* m_navLayout = nullptr;
    QButtonGroup* m_navGroup = nullptr;
    QLineEdit* m_gamesDirEdit = nullptr;

    QVBoxLayout* addPage(const QString& navText, const QString& title, const QString& subtitle);
    void addSection(QVBoxLayout* into, const QString& text);
    void addToggleRow(QVBoxLayout* into, const QString& title, const QString& desc, bool value,
                      std::function<void(bool)> onToggle);

    void buildGeneralPage();
    void buildAppearancePage();
    void buildGamesPage();
    void buildAboutPage();
    void refreshGamesDir();
};
