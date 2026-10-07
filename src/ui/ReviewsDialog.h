#pragma once
#include <QDialog>
#include <QString>

class AuthManager;
class FirestoreClient;
class QVBoxLayout;
class QComboBox;
class QPlainTextEdit;
class QLabel;
class QPushButton;

// Voti e commenti di un gioco: collezione Firestore "reviews", documento <gameId>_<uid>.
// Stessi dati del sito (eta-reviews.js): una recensione per utente, visibile ovunque.
class ReviewsDialog : public QDialog {
    Q_OBJECT
public:
    ReviewsDialog(AuthManager* auth, FirestoreClient* fs, const QString& gameId,
                  const QString& title, QWidget* parent = nullptr);

private:
    void load();
    void submit();

    AuthManager* m_auth;
    FirestoreClient* m_fs;
    QString m_gameId;
    QLabel* m_summary = nullptr;
    QLabel* m_msg = nullptr;
    QVBoxLayout* m_list = nullptr;
    QComboBox* m_rating = nullptr;
    QPlainTextEdit* m_text = nullptr;
    QPushButton* m_send = nullptr;
};
