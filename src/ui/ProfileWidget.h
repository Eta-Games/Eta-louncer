#pragma once
#include <QWidget>
#include <QPixmap>
#include <QElapsedTimer>
#include "../core/AuthManager.h"

class FirestoreClient;
class QLabel;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;
class QButtonGroup;
class QLineEdit;
class QPlainTextEdit;
class QComboBox;
class QTimer;
class ToggleSwitch;

// Sezione "Profilo": le stesse sezioni di profilo.html (Panoramica, Aspetto Grafico, Impostazioni Profilo,
// Sicurezza e Privacy, Funzioni extra, La mia Libreria). I dati stanno su Firebase (Auth + Firestore users/<uid>).
class ProfileWidget : public QWidget {
    Q_OBJECT
public:
    ProfileWidget(AuthManager* auth, FirestoreClient* fs, QWidget* parent = nullptr);

    void refresh();                                 // ricarica (al massimo ogni minuto) i dati dell'utente
    void syncThemeToCloud(const QString& themeId);  // salva il tema in users/<uid>.theme come fa il sito
    void setOnlineStatusText(const QString& text);
    void addExtraSection(const QString& navText, const QString& title, QWidget* content); // sezione extra nella sidebar

signals:
    void goToLoginRequested();
    void loggedOut();
    void themeSelected(QString themeId);            // tema scelto qui o arrivato dal cloud
    void gameActionRequested(QString gameId);       // "Gioca"/"Installa" dalla libreria

private:
    AuthManager* m_auth;
    FirestoreClient* m_fs;
    QStackedWidget* m_stack;        // 0 = ospite, 1 = account
    QStackedWidget* m_sections = nullptr;
    QVBoxLayout* m_navLayout = nullptr;
    QButtonGroup* m_navGroup = nullptr;

    // Panoramica
    QLabel* m_overviewAvatar = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_emailLabel = nullptr;
    QLabel* m_statusLine = nullptr;
    QLabel* m_createdLabel = nullptr;
    QLabel* m_lastLoginLabel = nullptr;
    QLabel* m_daysLabel = nullptr;
    // Aspetto
    QLabel* m_avatarPreview = nullptr;
    QLabel* m_bannerPreview = nullptr;
    // Impostazioni profilo
    QLineEdit* m_usernameEdit = nullptr;
    QPlainTextEdit* m_bioEdit = nullptr;
    QComboBox* m_countryBox = nullptr;
    // Sicurezza
    ToggleSwitch* m_publicSwitch = nullptr;
    QLabel* m_publicText = nullptr;
    QLineEdit* m_newEmail = nullptr;
    QLineEdit* m_newPassword = nullptr;
    // Extra
    QLabel* m_badgeLabel = nullptr;
    QComboBox* m_themeBox = nullptr;
    // Libreria
    QVBoxLayout* m_libraryList = nullptr;
    // Messaggi
    QLabel* m_message = nullptr;
    QTimer* m_messageTimer = nullptr;

    QPixmap m_photo;   // foto profilo (da Firestore photoBase64)
    QElapsedTimer m_lastLoad;
    QString m_loadedUid;
    QString m_onlineText;

    QVBoxLayout* addSection(const QString& navText, const QString& title);
    void buildGuestPage();
    void buildAccountPage();
    void buildOverview();
    void buildAppearance();
    void buildProfileSettings();
    void buildSecurity();
    void buildExtra();
    void buildLibrary();

    void reload(bool force);
    void applyCloudData(const QJsonObject& data);
    void applyLookup(const QJsonObject& user);
    void updateAvatars();
    void uploadImage(bool banner);
    void showMessage(const QString& text, bool ok = true);
    void saveField(const QString& field, const QJsonValue& value, const QString& okText);
    void rebuildLibrary(const QStringList& ids);
};
