#pragma once
#include <QDialog>
#include <QString>

class AuthManager;
class FirestoreClient;
class QVBoxLayout;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QProgressBar;

// Recensioni stile Steam (Mi piace / Non mi piace) + commento.
// Collezione Firestore "reviews", documento <gameId>_<uid>.
// Compatibile col sito (eta-reviews.js): il pollice su salva rating=5, quello giù rating=1,
// e le vecchie recensioni a stelle (>=3) contano come positive.
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
    QProgressBar* m_bar = nullptr;
    QLabel* m_msg = nullptr;
    QVBoxLayout* m_list = nullptr;
    QPushButton* m_like = nullptr;
    QPushButton* m_dislike = nullptr;
    QPlainTextEdit* m_text = nullptr;
    QPushButton* m_send = nullptr;
};
